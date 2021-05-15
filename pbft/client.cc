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
  lastReqId = 0;
  pendingRequest = nullptr;
  requestTimeout = new Timeout(transport, 1000, [this]() { ResendRequest(); });

  pendingUnloggedRequest = nullptr;
  unloggedRequestTimeout =
      new Timeout(transport, DEFAULT_UNLOGGED_OP_TIMEOUT, [this]() {
        if (!unloggedTimeoutContinuation) {
          return;
        }
        Debug("Unlogged timeout call cont");
        unloggedTimeoutContinuation(pendingUnloggedRequest->request);
        unloggedRequestTimeout->Stop();
      });

  f = config.f;
}

PbftClient::~PbftClient() {
  delete requestTimeout;
  delete unloggedRequestTimeout;
  if (pendingRequest) {
    delete pendingRequest;
  }
  if (pendingUnloggedRequest) {
    delete pendingUnloggedRequest;
  }
}

void PbftClient::Invoke(const string &request, continuation_t continuation) {
  if (pendingRequest != NULL) {
    Panic("Client only supports one pending request");
  }
  lastReqId += 1;
  pendingRequest = new PendingRequest(request, lastReqId, continuation);
  SendRequest();
}

void PbftClient::SendRequest() {
  proto::RequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(lastReqId);
  // todo
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
  if (pendingUnloggedRequest != NULL) {
    Panic("Client only supports one pending request");
  }
  uint64_t clientReqId = 0;
  pendingUnloggedRequest =
      new PendingRequest(request, clientReqId, continuation);

  proto::UnloggedRequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingUnloggedRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(clientReqId);

  if (timeoutContinuation) {
    Debug("Set unlogged timeout");
    unloggedTimeoutContinuation = timeoutContinuation;
    unloggedRequestTimeout->Stop();
    unloggedRequestTimeout->SetTimeout(timeout);
    unloggedRequestTimeout->Start();
  }
  transport->SendMessageToReplica(this, replicaIdx, reqMsg);
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
  if (!pendingRequest) {
    Warning("Received reply when no request was pending");
    return;
  }
  if (msg.req().clientreqid() != pendingRequest->clientreqid) {
    return;
  }

  Debug("Client received reply");
  std::set<int> &replicaGroup = pendingRequest->replyGroupMap[msg.reply()];
  replicaGroup.insert(0);  // todo: include replica id and signature in reply
  int count = replicaGroup.size();
  if (count < f + 1) {
    Debug("%d replies has same result as current one, waiting for more", count);
    return;
  }

  Debug("f + 1 replies received, current request done");
  requestTimeout->Stop();
  PendingRequest *req = pendingRequest;
  pendingRequest = nullptr;
  req->continuation(req->request, msg.reply());
  delete req;
}

void PbftClient::HandleUnloggedReply(const TransportAddress &remote,
                                     const proto::UnloggedReplyMessage &msg) {
  if (pendingUnloggedRequest == nullptr) {
    Warning("Received unloggedReply when no request was pending");
  }

  Debug("Client received unloggedReply");
  unloggedRequestTimeout->Stop();

  PendingRequest *req = pendingUnloggedRequest;
  pendingUnloggedRequest = nullptr;
  req->continuation(req->request, msg.reply());
  delete req;
}

}  // namespace pbft
}  // namespace specpaxos
