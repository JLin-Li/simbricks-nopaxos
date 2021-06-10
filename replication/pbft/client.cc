#include "replication/pbft/client.h"

#include "common/client.h"
#include "common/request.pb.h"
#include "lib/message.h"
#include "lib/rsakeys.h"
#include "lib/transport.h"
#include "replication/pbft/pbft-proto.pb.h"

namespace dsnet {
namespace pbft {

PbftClient::PbftClient(const Configuration &config, Transport *transport,
                       uint64_t clientid)
    : Client(config, transport, clientid) {
  lastReqId = 0;
  pendingRequest = nullptr;
  requestTimeout = new Timeout(transport, 1000, [this]() { ResendRequest(); });

  signer.Initialize(PRIVATE_KEY);
  verifier.Initialize(PUBLIC_KEY);
}

PbftClient::~PbftClient() {
  delete requestTimeout;
  if (pendingRequest) {
    delete pendingRequest;
  }
}

void PbftClient::Invoke(const string &request, continuation_t continuation) {
  if (pendingRequest != NULL) {
    Panic("Client only supports one pending request");
  }
  lastReqId += 1;
  pendingRequest =
      new PendingRequest(request, lastReqId, continuation, config.f + 1);
  SendRequest();
}

void PbftClient::SendRequest() {
  proto::RequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(lastReqId);

  // TODO sig
  reqMsg.set_sig(std::string());

  // TODO
  // transport->SendMessageToReplica(this, 0, reqMsg);
  transport->SendMessageToAll(this, reqMsg);
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
  NOT_IMPLEMENTED();
}

void PbftClient::ReceiveMessage(const TransportAddress &remote,
                                const string &type, const string &data,
                                void *meta_data) {
  static proto::ReplyMessage reply;
  // static proto::UnloggedReplyMessage unloggedReply;

  if (type == reply.GetTypeName()) {
    reply.ParseFromString(data);
    HandleReply(remote, reply);
  } else {
    Client::ReceiveMessage(remote, type, data, NULL);
  }
}

void PbftClient::HandleReply(const TransportAddress &remote,
                             const proto::ReplyMessage &msg) {
  if (!pendingRequest) {
    // Warning("Received reply when no request was pending");
    return;
  }
  if (msg.req().clientreqid() != pendingRequest->clientreqid) {
    return;
  }

  // TODO verify sig

  Debug("Client received reply");
  if (!pendingRequest->replySet.Add(msg.req().clientreqid(), msg.replicaid(),
                                    msg.SerializeAsString())) {
    return;
  }

  Debug("f + 1 replies received, current request done");
  requestTimeout->Stop();
  PendingRequest *req = pendingRequest;
  pendingRequest = nullptr;
  req->continuation(req->request, msg.reply());
  delete req;
}

}  // namespace pbft
}  // namespace dsnet
