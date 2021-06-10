#include "replication/pbft/replica.h"

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
  auto kv = clientTable.find(msg.req().clientid());
  if (kv != clientTable.end()) {
    const ClientTableEntry &entry = kv->second;
    if (msg.req().clientreqid() < entry.lastReqId) {
      RNotice("Ignoring stale request");
      return;
    }
    if (msg.req().clientreqid() == entry.lastReqId) {
      RNotice("Received duplicate request; resending reply");
      if (!(transport->SendMessage(this, remote, entry.reply))) {
        RWarning("Failed to resend reply to client");
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

  RDebug("Backup accept pre-prepare message for view#%ld seq#%ld", view,
         seqNum);
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
  //
}

void PbftReplica::OnViewChange() { NOT_IMPLEMENTED(); }

void PbftReplica::AcceptPrePrepare(proto::PrePrepareMessage message) {
  // TODO append to log
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

  RDebug("Enter commit round for view#%lu seq#%lu", message.view(),
         message.seqnum());

  // TODO set a Timeout to prevent duplicated broadcast

  CommitMessage commit;
  *commit.mutable_common() = message;
  commit.set_replicaid(replicaIdx);
  // TODO sig
  commit.set_sig(std::string());
  transport->SendMessageToAll(this, commit);

  commitSet.Add(message.seqnum(), ReplicaId(), message);
  // TODO try execute for single replica setup
}

void PbftReplica::UpdateClientTable(const Request &req,
                                    const proto::ReplyMessage &reply) {
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
