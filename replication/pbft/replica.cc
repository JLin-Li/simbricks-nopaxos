#include "replication/pbft/replica.h"

#include "common/pbmessage.h"
#include "common/replica.h"
#include "lib/message.h"
#include "lib/rsakeys.h"
#include "lib/transport.h"
#include "replication/pbft/pbft-proto.pb.h"

#define RDebug(fmt, ...) Debug("[%d] " fmt, this->replicaIdx, ##__VA_ARGS__)
#define RNotice(fmt, ...) Notice("[%d] " fmt, this->replicaIdx, ##__VA_ARGS__)
#define RWarning(fmt, ...) Warning("[%d] " fmt, this->replicaIdx, ##__VA_ARGS__)
#define RPanic(fmt, ...) Panic("[%d] " fmt, this->replicaIdx, ##__VA_ARGS__)

using namespace std;

namespace dsnet {
namespace pbft {

using namespace proto;

PbftReplica::PbftReplica(Configuration config, int myIdx, bool initialize,
                         Transport *transport, AppReplica *app)
    : Replica(config, 0, myIdx, initialize, transport, app),
      log(false),
      prepareSet(2 * config.f),
      commitSet(2 * config.f + 1) {
  if (!initialize) NOT_IMPLEMENTED();

  this->status = STATUS_NORMAL;
  this->view = 0;
  this->seqNum = 0;

  signer.Initialize(PRIVATE_KEY);
  verifier.Initialize(PUBLIC_KEY);

  // 1min timeout to make sure no one ever want to change view
  viewChangeTimeout =
      new Timeout(transport, 60 * 1000, bind(&PbftReplica::OnViewChange, this));
}

void PbftReplica::ReceiveMessage(const TransportAddress &remote,
                                 const string &type, const string &data,
                                 void *meta_data) {
#define Handle(MessageType)                       \
  static proto::MessageType##Message MessageType; \
  if (type == MessageType.GetTypeName()) {        \
    MessageType.ParseFromString(data);            \
    Handle##MessageType(remote, MessageType);     \
    return;                                       \
  }

  Handle(Request);
  Handle(PrePrepare);
  Handle(Prepare);
  Handle(Commit);

#undef Handle

  RPanic("Received unexpected message type in pbft proto: %s", type.c_str());
}

void PbftReplica::HandleRequest(const TransportAddress &remote,
                                const RequestMessage &msg) {
  clientAddressTable[msg.req().clientid()] =
      unique_ptr<TransportAddress>(remote.clone());
  auto kv = clientTable.find(msg.req().clientid());
  if (kv != clientTable.end()) {
    ClientTableEntry &entry = kv->second;
    if (msg.req().clientreqid() < entry.lastReqId) {
      RNotice("Ignoring stale request");
      return;
    }
    if (msg.req().clientreqid() == entry.lastReqId) {
      Notice("Received duplicate request; resending reply");
      if (!(transport->SendMessage(this, remote, PBMessage(entry.reply)))) {
        Warning("Failed to resend reply to client");
      }
      return;
    }
  }

  if (!AmPrimary()) {
    RWarning("Received Request but not primary; is primary dead?");
    transport->SendMessageToReplica(this, configuration.GetLeaderIndex(view),
                                    msg);
    viewChangeTimeout->Start();
    return;
  }

  RDebug("Start pre-prepare for client#%lu req#%lu", msg.req().clientid(),
         msg.req().clientreqid());
  seqNum += 1;
  PrePrepareMessage prePrepare;
  prePrepare.mutable_common()->set_view(view);
  prePrepare.mutable_common()->set_seqnum(seqNum);
  // TODO digest and sig
  prePrepare.mutable_common()->set_digest(std::string());
  prePrepare.set_sig(std::string());
  *prePrepare.mutable_message() = msg;

  AcceptPrePrepare(prePrepare);
  transport->SendMessageToAll(this, prePrepare);

  TryBroadcastCommit(prePrepare.common());  // for single replica setup
}

void PbftReplica::HandlePrePrepare(const TransportAddress &remote,
                                   const proto::PrePrepareMessage &msg) {
  if (AmPrimary()) RPanic("Unexpected PrePrepare sent to primary");

  // TODO verify sig and digest
  // NOTICE verify both sig of PrePrepare and Request

  if (view != msg.common().view()) return;

  opnum_t seqNum = msg.common().seqnum();
  if (acceptedPrePrepareTable.count(seqNum) &&
      !Match(acceptedPrePrepareTable[seqNum], msg.common()))
    return;

  // no impl high and low water mark, along with GC

  RDebug("Enter PREPARE round for view#%ld seq#%ld", view, seqNum);
  AcceptPrePrepare(msg);
  PrepareMessage prepare;
  *prepare.mutable_common() = msg.common();
  prepare.set_replicaid(ReplicaId());
  // TODO sig
  prepare.set_sig(std::string());
  transport->SendMessageToAll(this, prepare);

  prepareSet.Add(seqNum, ReplicaId(), msg.common());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::HandlePrepare(const TransportAddress &remote,
                                const proto::PrepareMessage &msg) {
  prepareSet.Add(msg.common().seqnum(), msg.replicaid(), msg.common());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::HandleCommit(const TransportAddress &remote,
                               const proto::CommitMessage &msg) {
  commitSet.Add(msg.common().seqnum(), msg.replicaid(), msg.common());
  TryExecute(msg.common());
}

void PbftReplica::OnViewChange() { NOT_IMPLEMENTED(); }

void PbftReplica::AcceptPrePrepare(proto::PrePrepareMessage message) {
  acceptedPrePrepareTable[message.common().seqnum()] = message.common();
  if (message.common().seqnum() != log.LastOpnum() + 1) {
    // collect info of the gap, pend pre-prepare, and start state transfer
    NOT_IMPLEMENTED();
  }
  log.Append(
      LogEntry(viewstamp_t(message.common().view(), message.common().seqnum()),
               LOG_STATE_PREPARED, message.message().req()));
}

void PbftReplica::TryBroadcastCommit(const proto::Common &message) {
  if (!Prepared(message.seqnum(), message)) return;

  RDebug("Enter COMMIT round for view#%lu seq#%lu", message.view(),
         message.seqnum());

  // TODO set a Timeout to prevent duplicated broadcast

  CommitMessage commit;
  *commit.mutable_common() = message;
  commit.set_replicaid(ReplicaId());
  // TODO sig
  commit.set_sig(std::string());
  transport->SendMessageToAll(this, commit);

  commitSet.Add(message.seqnum(), ReplicaId(), message);
  TryExecute(message);  // for single replica setup
}

void PbftReplica::TryExecute(const proto::Common &message) {
  Assert(message.seqnum() <= log.LastOpnum());
  if (log.Find(message.seqnum())->state == LOG_STATE_COMMITTED ||
      log.Find(message.seqnum())->state == LOG_STATE_EXECUTED)
    return;

  if (!CommittedLocal(message.seqnum(), message)) return;

  RDebug("Reach commit point for view #%lu seq#%lu", message.view(),
         message.seqnum());
  log.SetStatus(message.seqnum(), LOG_STATE_COMMITTED);

  opnum_t executing = message.seqnum();
  if (executing != log.FirstOpnum() &&
      log.Find(executing - 1)->state != LOG_STATE_EXECUTED)
    return;
  while (auto *entry = log.Find(executing)) {
    Assert(entry->state != LOG_STATE_EXECUTED);
    if (entry->state != LOG_STATE_COMMITTED) break;
    entry->state = LOG_STATE_EXECUTED;

    const Request &req = log.Find(executing)->request;
    proto::ReplyMessage reply;
    UpcallArg arg;
    arg.isLeader = AmPrimary();
    Execute(executing, log.Find(executing)->request, reply, &arg);
    reply.set_view(view);
    *reply.mutable_req() = req;
    reply.set_replicaid(ReplicaId());
    // TODO sig
    reply.set_sig(std::string());
    UpdateClientTable(req, reply);
    if (clientAddressTable.count(req.clientid()))
      transport->SendMessage(this, *clientAddressTable[req.clientid()], reply);

    executing += 1;
  }
}

void PbftReplica::UpdateClientTable(const Request &req,
                                    const ToClientMessage &reply) {
  ClientTableEntry &entry = clientTable[req.clientid()];
  ASSERT(entry.lastReqId <= req.clientreqid());

  if (entry.lastReqId == req.clientreqid()) {  // Duplicate request
    return;
  }
  entry.lastReqId = req.clientreqid();
  entry.reply = reply;
}

}  // namespace pbft
}  // namespace dsnet
