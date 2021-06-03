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
    : Replica(config, 0, myIdx, initialize, transport, app), log(false) {
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
  static proto::RequestMessage request;
  static proto::UnloggedRequestMessage unloggedRequest;

  if (type == request.GetTypeName()) {
    request.ParseFromString(data);
    HandleRequest(remote, request);
  } else if (type == unloggedRequest.GetTypeName()) {
    unloggedRequest.ParseFromString(data);
    HandleUnloggedRequest(remote, unloggedRequest);
  } else {
    RPanic("Received unexpected message type in pbft proto: %s", type.c_str());
  }
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
  prePrepare.set_view(view);
  prePrepare.set_seqnum(seqNum);
  // TODO digest and sig
  prePrepare.set_digest(std::string());
  prePrepare.set_sig(std::string());
  *prePrepare.mutable_message() = msg;

  loggedPrePrepareMessageTable[seqNum] = prePrepare;
  // prepare replica table init?
  transport->SendMessageToAll(this, prePrepare);
}

void PbftReplica::HandlePrePrepare(const TransportAddress &remote,
                                   const proto::PrePrepareMessage &msg) {
  if (AmPrimary()) RPanic("Unexpected PrePrepare sent to primary");

  // TODO verify sig and digest

  if (view != msg.view()) return;

  if (loggedPrePrepareMessageTable.count(msg.seqnum()) &&
      loggedPrePrepareMessageTable[msg.seqnum()].digest() != msg.digest()) {
    // Byzantine case
    NOT_IMPLEMENTED();
  }
  if (loggedPrepareMessageTable.count(msg.seqnum()) &&
      loggedPrepareMessageTable[msg.seqnum()].digest() != msg.digest()) {
    // Byzantine case
    NOT_IMPLEMENTED();
  }

  // no impl high and low water mark

  RDebug("Backup accept pre-prepare message for view#%ld seq#%ld", view,
         seqNum);
  loggedPrePrepareMessageTable[msg.seqnum()] = msg;
  PrepareMessage prepare;
  prepare.set_view(view);
  prepare.set_seqnum(msg.seqnum());
  prepare.set_digest(msg.digest());
  prepare.set_replicaid(replicaIdx);
  // TODO sig
  prepare.set_sig(std::string());

  loggedPrepareMessageTable[msg.seqnum()] = prepare;
  loggedPrepareReplicaTable[msg.seqnum()].insert(replicaIdx);
  transport->SendMessageToAll(this, prepare);
  BroadcastCommitIfPrepared(msg.seqnum());
}

void PbftReplica::HandlePrepare(const TransportAddress &remote,
                                const proto::PrepareMessage &msg) {
  if (loggedPrePrepareMessageTable.count(msg.seqnum()) &&
      loggedPrePrepareMessageTable[msg.seqnum()].digest() != msg.digest()) {
    // Byzantine case
    NOT_IMPLEMENTED();
  }
  if (loggedPrepareMessageTable.count(msg.seqnum()) &&
      loggedPrepareMessageTable[msg.seqnum()].digest() != msg.digest()) {
    // Byzantine case
    NOT_IMPLEMENTED();
  }

  loggedPrepareMessageTable[msg.seqnum()] = msg;
  loggedPrepareReplicaTable[msg.seqnum()].insert(replicaIdx);
  BroadcastCommitIfPrepared(msg.seqnum());
}

void PbftReplica::BroadcastCommitIfPrepared(opnum_t seqNum) {
  if (!prepared(seqNum)) return;
  CommitMessage commit;
  commit.set_view(view);
  commit.set_seqnum(seqNum);
  commit.set_digest(loggedPrePrepareMessageTable[seqNum].digest());
  commit.set_replicaid(replicaIdx);
  // TODO sig
  commit.set_sig(std::string());
  transport->SendMessageToAll(this, commit);
}

void PbftReplica::HandleUnloggedRequest(const TransportAddress &remote,
                                        const UnloggedRequestMessage &msg) {
  RDebug("Received unlogged request %s", (char *)msg.req().op().c_str());
  UnloggedReplyMessage reply;
  ExecuteUnlogged(msg.req(), reply);
  if (!(transport->SendMessage(this, remote, reply)))
    RWarning("Failed to send reply message");
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
