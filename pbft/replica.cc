#include "pbft/replica.h"

#include "common/replica.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "pbft/pbft-proto.pb.h"

namespace specpaxos {
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
    const ClientTableEntry &entry = kv->second;
    if (msg.req().clientreqid() < entry.lastReqId) {
      Notice("Ignoring stale request");
      return;
    }
    if (msg.req().clientreqid() == entry.lastReqId) {
      Notice("Received duplicate request; resending reply");
      if (!(transport->SendMessage(this, remote, entry.reply))) {
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
  log.Append(v, msg.req(), LOG_STATE_RECEIVED);
  Debug("Received request %s", msg.req().op().c_str());

  ReplyMessage reply;
  Execute(0, msg.req(), reply);
  // The protocol defines these as required, even if they're not
  // meaningful.
  reply.set_view(0);
  reply.set_opnum(0);
  *(reply.mutable_req()) = msg.req();
  if (!(transport->SendMessage(this, remote, reply)))
    Warning("Failed to send reply message");

  UpdateClientTable(msg.req(), reply);
}

void PbftReplica::HandleUnloggedRequest(const TransportAddress &remote,
                                        const UnloggedRequestMessage &msg) {
  Debug("Received unlogged request %s", (char *)msg.req().op().c_str());
  UnloggedReplyMessage reply;
  ExecuteUnlogged(msg.req(), reply);
  if (!(transport->SendMessage(this, remote, reply)))
    Warning("Failed to send reply message");
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
    Panic("Received unexpected message type in pbft proto: %s", type.c_str());
  }
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
}  // namespace specpaxos
