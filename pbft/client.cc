#include "common/client.h"

#include "common/request.pb.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "pbft/client.h"
#include "pbft/pbft-proto.pb.h"

namespace specpaxos {
namespace pbft {

PbftClient::PbftClient(const Configuration &config, Transport *transport,
                       uint64_t clientid)
    : Client(config, transport, clientid) {
  pendingRequest = NULL;
  pendingUnloggedRequest = NULL;
  lastReqId = 0;
  requestTimeout = new Timeout(transport, 1000, [this]() { ResendRequest(); });
}

PbftClient::~PbftClient() {
  if (pendingRequest) {
    delete pendingRequest;
  }
  if (pendingUnloggedRequest) {
    delete pendingUnloggedRequest;
  }
}

void PbftClient::Invoke(const string &request, continuation_t continuation) {
  // XXX Can only handle one pending request for now
  if (pendingRequest != NULL) {
    Panic("Client only supports one pending request");
  }

  ++lastReqId;
  pendingRequest = new PendingRequest(request, lastReqId, continuation);

  SendRequest();
}

void PbftClient::SendRequest() {
  proto::RequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(lastReqId);

  // Pbft: just send to replica 0
  transport->SendMessageToReplica(this, 0, reqMsg);

  requestTimeout->Reset();
}

void PbftClient::ResendRequest() {
  Warning("Timeout, resending request for req id %lu", lastReqId);
  SendRequest();
}

void PbftClient::InvokeUnlogged(int replicaIdx, const string &request,
                                continuation_t continuation,
                                timeout_continuation_t timeoutContinuation,
                                uint32_t timeout) {
  // XXX Can only handle one pending request for now
  if (pendingUnloggedRequest != NULL) {
    Panic("Client only supports one pending request");
  }

  pendingUnloggedRequest = new PendingRequest(request, 0, continuation);

  proto::UnloggedRequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingUnloggedRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(0);

  // Pbft: just send to replica 0
  if (replicaIdx != 0) {
    Panic("Attempt to invoke unlogged operation on replica that doesn't exist");
  }
  transport->SendMessageToReplica(this, 0, reqMsg);
}

void PbftClient::ReceiveMessage(const TransportAddress &remote,
                                const string &type, const string &data,
                                void *meta_data) {
  static proto::ReplyMessage reply;
  static proto::UnloggedReplyMessage unloggedReply;

  if (type == reply.GetTypeName()) {
    reply.ParseFromString(data);
    HandleReply(remote, reply);
  } else if (type == unloggedReply.GetTypeName()) {
    unloggedReply.ParseFromString(data);
    HandleUnloggedReply(remote, unloggedReply);
  } else {
    Client::ReceiveMessage(remote, type, data, NULL);
  }
}

void PbftClient::HandleReply(const TransportAddress &remote,
                             const proto::ReplyMessage &msg) {
  if (pendingRequest == NULL) {
    Warning("Received reply when no request was pending");
    return;
  }

  if (msg.req().clientreqid() != pendingRequest->clientreqid) {
    return;
  }

  Debug("Client received reply");

  requestTimeout->Stop();

  PendingRequest *req = pendingRequest;
  pendingRequest = NULL;

  req->continuation(req->request, msg.reply());
  delete req;
}

void PbftClient::HandleUnloggedReply(const TransportAddress &remote,
                                     const proto::UnloggedReplyMessage &msg) {
  if (pendingUnloggedRequest == NULL) {
    Warning("Received unloggedReply when no request was pending");
  }

  Debug("Client received unloggedReply");

  PendingRequest *req = pendingUnloggedRequest;
  pendingUnloggedRequest = NULL;

  req->continuation(req->request, msg.reply());
  delete req;
}

}  // namespace pbft
}  // namespace specpaxos
