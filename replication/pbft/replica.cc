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
  Handle(UnloggedRequest);
  Handle(PrePrepare);
  Handle(Prepare);
  // Handle(Commit);

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
    // redirect to primary and start view change timer
    NOT_IMPLEMENTED();
  }

  Debug("Start pre-prepare for client#%ld req#%ld", msg.req().clientid(),
        msg.req().clientreqid());
  seqNum += 1;
  PrePrepareMessage prePrepare;
  prePrepare.mutable_common()->set_view(view);
  prePrepare.mutable_common()->set_seqnum(seqNum);
  // TODO digest and sig
  prePrepare.mutable_common()->set_digest(std::string());
  prePrepare.set_sig(std::string());
  *prePrepare.mutable_message() = msg;

  acceptedPrePrepareTable[seqNum] = prePrepare;
  transport->SendMessageToAll(this, prePrepare);
}

void PbftReplica::HandleUnloggedRequest(const TransportAddress &remote,
                                        const UnloggedRequestMessage &msg) {
  RDebug("Received unlogged request %s", (char *)msg.req().op().c_str());
  UnloggedReplyMessage reply;
  ExecuteUnlogged(msg.req(), reply);
  if (!(transport->SendMessage(this, remote, reply)))
    RWarning("Failed to send reply message");
}

void PbftReplica::HandlePrePrepare(const TransportAddress &remote,
                                   const proto::PrePrepareMessage &msg) {
  if (AmPrimary()) RPanic("Unexpected PrePrepare sent to primary");

  // TODO verify sig and digest
  // NOTICE verify both sig of PrePrepare and Request

  if (view != msg.common().view()) return;

  opnum_t seqNum = msg.common().seqnum();
  if (acceptedPrePrepareTable.count(seqNum)) return;

  // no impl high and low water mark, along with GC

  RDebug("Backup accept pre-prepare message for view#%ld seq#%ld", view,
         seqNum);
  acceptedPrePrepareTable[msg.common().seqnum()] = msg;
  PrepareMessage prepare;
  *prepare.mutable_common() = msg.common();
  prepare.set_replicaid(ReplicaId());
  // TODO sig
  prepare.set_sig(std::string());
  transport->SendMessageToAll(this, prepare);

  prepareSet.Add(seqNum, ReplicaId(), msg.common().SerializeAsString());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::HandlePrepare(const TransportAddress &remote,
                                const proto::PrepareMessage &msg) {
  prepareSet.Add(msg.common().seqnum(), msg.replicaid(),
                 msg.common().SerializeAsString());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::TryBroadcastCommit(const proto::Common &message) {
  if (!Prepared(message.seqnum(), message)) return;

  // TODO set a Timeout to prevent duplicated broadcast

  CommitMessage commit;
  *commit.mutable_common() = message;
  commit.set_replicaid(replicaIdx);
  // TODO sig
  commit.set_sig(std::string());
  transport->SendMessageToAll(this, commit);
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
