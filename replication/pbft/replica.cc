#include "replication/pbft/replica.h"

#include "common/replica.h"
#include "common/pbmessage.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/pbft/pbft-proto.pb.h"

namespace dsnet {
namespace pbft {

using namespace proto;

PbftReplica::PbftReplica(Configuration config, int myIdx, bool initialize,
                         Transport *transport, AppReplica *app)
    : Replica(config, 0, myIdx, initialize, transport, app), log(false) {
  if (!initialize) {
    Panic("Recovery does not make sense for pbft mode");
  }

  this->status = STATUS_NORMAL;
  this->last_op_ = 0;
}

void PbftReplica::HandleRequest(const TransportAddress &remote,
                                const RequestMessage &msg) {
  auto kv = clientTable.find(msg.req().clientid());
  if (kv != clientTable.end()) {
    ClientTableEntry &entry = kv->second;
    if (msg.req().clientreqid() < entry.lastReqId) {
      Notice("Ignoring stale request");
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

  ++last_op_;
  viewstamp_t v;
  v.view = 0;
  v.opnum = last_op_;
  v.sessnum = 0;
  v.msgnum = 0;

  log.Append(new LogEntry(v, LOG_STATE_RECEIVED, msg.req()));
  Debug("Received request %s", msg.req().op().c_str());

  ToClientMessage m;
  ReplyMessage *reply = m.mutable_reply();
  Execute(0, msg.req(), *reply);
  // The protocol defines these as required, even if they're not
  // meaningful.
  reply->set_view(0);
  reply->set_opnum(0);

  reply->set_replicaid(replicaIdx);
  *(reply->mutable_req()) = msg.req();
  if (!(transport->SendMessage(this, remote, PBMessage(m))))
    Warning("Failed to send reply message");

  UpdateClientTable(msg.req(), m);
}

void PbftReplica::HandleUnloggedRequest(const TransportAddress &remote,
                                        const UnloggedRequestMessage &msg) {
  Debug("Received unlogged request %s", (char *)msg.req().op().c_str());
  ToClientMessage m;
  UnloggedReplyMessage *reply = m.mutable_unlogged_reply();
  ExecuteUnlogged(msg.req(), *reply);
  if (!(transport->SendMessage(this, remote, PBMessage(m))))
    Warning("Failed to send reply message");
}

void PbftReplica::ReceiveMessage(const TransportAddress &remote,
                                 void *buf, size_t size)
{
    static ToReplicaMessage replica_msg;
    static PBMessage m(replica_msg);

    m.Parse(buf, size);

    switch (replica_msg.msg_case()) {
        case ToReplicaMessage::MsgCase::kRequest:
            HandleRequest(remote, replica_msg.request());
            break;
        case ToReplicaMessage::MsgCase::kUnloggedRequest:
            HandleUnloggedRequest(remote, replica_msg.unlogged_request());
            break;
        default:
            Panic("Received unexpected message type: %u", replica_msg.msg_case());
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
