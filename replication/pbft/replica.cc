#include "replication/pbft/replica.h"

#include "common/pbmessage.h"
#include "common/replica.h"
#include "lib/message.h"
#include "lib/signature.h"
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

PbftReplica::PbftReplica(const Configuration &config, int myIdx,
                         bool initialize, Transport *transport,
                         const Security &sec, AppReplica *app)
    : Replica(config, 0, myIdx, initialize, transport, app),
      security(sec),
      log(false),
      prepareSet(2 * config.f),
      commitSet(2 * config.f + 1) {
  if (!initialize) NOT_IMPLEMENTED();

  this->status = STATUS_NORMAL;
  this->view = 0;
  this->seqNum = 0;

  // 1h timeout to make sure no one ever want to change view
  viewChangeTimeout = new Timeout(transport, 3600 * 1000,
                                  bind(&PbftReplica::OnViewChange, this));
  stateTransferTimeout =
      new Timeout(transport, 1000, bind(&PbftReplica::OnStateTransfer, this));
  // resendPrePrepareTimeout =
  //     new Timeout(transport, 300, bind(&PbftReplica::OnResendPrePrepare,
  //     this));
}

void PbftReplica::ReceiveMessage(const TransportAddress &remote, void *buf,
                                 size_t size) {
  static ToReplicaMessage replica_msg;
  static PBMessage m(replica_msg);

  m.Parse(buf, size);

  switch (replica_msg.msg_case()) {
    case ToReplicaMessage::MsgCase::kRequest:
      HandleRequest(remote, replica_msg.request());
      break;
    case ToReplicaMessage::MsgCase::kPrePrepare:
      HandlePrePrepare(remote, replica_msg.pre_prepare());
      break;
    case ToReplicaMessage::MsgCase::kPrepare:
      HandlePrepare(remote, replica_msg.prepare());
      break;
    case ToReplicaMessage::MsgCase::kCommit:
      HandleCommit(remote, replica_msg.commit());
      break;
    default:
      RPanic("Received unexpected message type in pbft proto: %u",
             replica_msg.msg_case());
  }
}

void PbftReplica::HandleRequest(const TransportAddress &remote,
                                const RequestMessage &msg) {
  if (!security.GetClientVerifier(remote).Verify(msg.req().SerializeAsString(),
                                                 msg.sig())) {
    RWarning("Wrong signature for client");
    return;
  }

  if (!msg.relayed()) {
    clientAddressTable[msg.req().clientid()] =
        unique_ptr<TransportAddress>(remote.clone());
  }
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
    ToReplicaMessage m;
    *m.mutable_request() = msg;
    m.mutable_request()->set_relayed(true);
    transport->SendMessageToReplica(this, configuration.GetLeaderIndex(view),
                                    PBMessage(m));
    viewChangeTimeout->Start();
    return;
  }

  for (auto &pp : pendingPrePrepareList) {
    if (pp.clientId == msg.req().clientid() &&
        pp.clientReqId == msg.req().clientreqid()) {
      RNotice("Skip propose; active propose exist");
      return;
    }
  }

  RDebug("Start pre-prepare for client#%lu req#%lu", msg.req().clientid(),
         msg.req().clientreqid());
  seqNum += 1;
  ToReplicaMessage m;
  PrePrepareMessage &prePrepare = *m.mutable_pre_prepare();
  prePrepare.mutable_common()->set_view(view);
  prePrepare.mutable_common()->set_seqnum(seqNum);
  prePrepare.mutable_common()->set_digest(std::string());  // TODO
  security.GetReplicaSigner(ReplicaId())
      .Sign(prePrepare.common().SerializeAsString(), *prePrepare.mutable_sig());
  *prePrepare.mutable_message() = msg;

  Assert(prePrepare.common().seqnum() == log.LastOpnum() + 1);
  AcceptPrePrepare(prePrepare);
  transport->SendMessageToAll(this, PBMessage(m));
  PendingPrePrepare pp;
  pp.seqNum = seqNum;
  pp.clientId = msg.req().clientid();
  pp.clientReqId = msg.req().clientreqid();
  pp.timeout =
      std::unique_ptr<Timeout>(new Timeout(transport, 300, [this, m = m]() {
        RWarning("Resend PrePrepare #%lu", m.pre_prepare().common().seqnum());
        ToReplicaMessage copy(m);
        transport->SendMessageToAll(this, PBMessage(copy));
      }));
  pp.timeout->Start();
  pendingPrePrepareList.push_back(std::move(pp));
  TryBroadcastCommit(prePrepare.common());  // for single replica setup
}

void PbftReplica::HandlePrePrepare(const TransportAddress &remote,
                                   const proto::PrePrepareMessage &msg) {
  if (AmPrimary()) RPanic("Unexpected PrePrepare sent to primary");

  if (view != msg.common().view()) return;

  if (!security.GetReplicaVerifier(configuration.GetLeaderIndex(view))
           .Verify(msg.common().SerializeAsString(), msg.sig())) {
    RWarning("Wrong signature for PrePrepare");
    return;
  }
  uint64_t clientid = msg.message().req().clientid();
  if (!clientAddressTable.count(clientid)) {
    RWarning("sig@PrePrepare: no client address record");
    return;
  }
  if (!security.GetClientVerifier(*clientAddressTable[clientid])
           .Verify(msg.message().req().SerializeAsString(),
                   msg.message().sig())) {
    RWarning("Wrong signature for client in PrePrepare");
    return;
  }

  opnum_t seqNum = msg.common().seqnum();
  if (acceptedPrePrepareTable.count(seqNum) &&
      !Match(acceptedPrePrepareTable[seqNum], msg.common()))
    return;

  // no impl high and low water mark, along with GC

  if (msg.common().seqnum() > log.LastOpnum() + 1) {
    // TODO still prepare but record message out of Log
    RNotice("Gap detected, not prepare and schedule state transfer");
    // TODO pend pre-prepare
    if (!stateTransferTimeout->Active()) {
      stateTransferTimeout->Start();
    }
    return;
  }
  RDebug("Enter PREPARE round for view#%ld seq#%ld", view, seqNum);
  AcceptPrePrepare(msg);
  ToReplicaMessage m;
  PrepareMessage &prepare = *m.mutable_prepare();
  *prepare.mutable_common() = msg.common();
  prepare.set_replicaid(ReplicaId());
  security.GetReplicaSigner(ReplicaId())
      .Sign(prepare.common().SerializeAsString(), *prepare.mutable_sig());
  transport->SendMessageToAll(this, PBMessage(m));

  prepareSet.Add(seqNum, ReplicaId(), msg.common());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::HandlePrepare(const TransportAddress &remote,
                                const proto::PrepareMessage &msg) {
  if (!security.GetReplicaVerifier(msg.replicaid())
           .Verify(msg.common().SerializeAsString(), msg.sig())) {
    RWarning("Wrong signature for Prepare");
    return;
  }

  prepareSet.Add(msg.common().seqnum(), msg.replicaid(), msg.common());
  TryBroadcastCommit(msg.common());
}

void PbftReplica::HandleCommit(const TransportAddress &remote,
                               const proto::CommitMessage &msg) {
  if (!security.GetReplicaVerifier(msg.replicaid())
           .Verify(msg.common().SerializeAsString(), msg.sig())) {
    RWarning("Wrong signature for Commit");
    return;
  }

  commitSet.Add(msg.common().seqnum(), msg.replicaid(), msg.common());
  TryExecute(msg.common());
}

void PbftReplica::OnViewChange() { NOT_IMPLEMENTED(); }

void PbftReplica::OnStateTransfer() { NOT_IMPLEMENTED(); }

void PbftReplica::AcceptPrePrepare(proto::PrePrepareMessage message) {
  acceptedPrePrepareTable[message.common().seqnum()] = message.common();
  log.Append(new LogEntry(
      viewstamp_t(message.common().view(), message.common().seqnum()),
      LOG_STATE_PREPARED, message.message().req()));
}

void PbftReplica::TryBroadcastCommit(const proto::Common &message) {
  if (!Prepared(message.seqnum(), message)) return;

  RDebug("Enter COMMIT round for view#%lu seq#%lu", message.view(),
         message.seqnum());

  if (AmPrimary()) {
    for (auto iter = pendingPrePrepareList.begin();
         iter != pendingPrePrepareList.end(); ++iter) {
      if (iter->seqNum == message.seqnum()) {
        iter->timeout->Stop();
        pendingPrePrepareList.erase(iter);
        break;
      }
    }
  }

  // TODO set a Timeout to prevent duplicated broadcast

  ToReplicaMessage m;
  CommitMessage &commit = *m.mutable_commit();
  *commit.mutable_common() = message;
  commit.set_replicaid(ReplicaId());
  security.GetReplicaSigner(ReplicaId())
      .Sign(commit.common().SerializeAsString(), *commit.mutable_sig());
  transport->SendMessageToAll(this, PBMessage(m));

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
    ToClientMessage m;
    proto::ReplyMessage &reply = *m.mutable_reply();
    UpcallArg arg;
    arg.isLeader = AmPrimary();
    Execute(executing, log.Find(executing)->request, reply, &arg);
    reply.set_view(view);
    *reply.mutable_req() = req;
    reply.set_replicaid(ReplicaId());
    reply.set_sig(std::string());
    security.GetReplicaSigner(ReplicaId())
        .Sign(reply.SerializeAsString(), *reply.mutable_sig());
    UpdateClientTable(req, m);
    if (clientAddressTable.count(req.clientid()))
      transport->SendMessage(this, *clientAddressTable[req.clientid()],
                             PBMessage(m));

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
