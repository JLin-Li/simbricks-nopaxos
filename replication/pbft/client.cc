#include "replication/pbft/client.h"

#include "common/client.h"
#include "common/request.pb.h"
#include "common/pbmessage.h"
#include "lib/message.h"
#include "lib/rsakeys.h"
#include "lib/transport.h"
#include "replication/pbft/pbft-proto.pb.h"

namespace dsnet {
namespace pbft {

using namespace proto;

PbftClient::PbftClient(const Configuration &config, const ReplicaAddress &addr,
                       Transport *transport, uint64_t clientid)
    : Client(config, addr, transport, clientid) {
  lastReqId = 0;
  pendingRequest = nullptr;
  requestTimeout = new Timeout(transport, 1000, [this]() { ResendRequest(); });

  signer.Initialize(PRIVATE_KEY);
  verifier.Initialize(PUBLIC_KEY);

  view = 0;
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

void PbftClient::SendRequest(bool broadcast) {
  proto::RequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(pendingRequest->request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(lastReqId);

  // TODO sig
  reqMsg.set_sig(std::string());

  if (broadcast)
    transport->SendMessageToAll(this, reqMsg);
  else
    transport->SendMessageToReplica(this, config.GetLeaderIndex(view), reqMsg);
  requestTimeout->Reset();
}

void PbftClient::ResendRequest() {
  Warning("Timeout, resending request for req id %lu", lastReqId);
  SendRequest(true);
}

void PbftClient::InvokeUnlogged(int replicaIdx, const string &request,
                                continuation_t continuation,
                                timeout_continuation_t timeoutContinuation,
                                uint32_t timeout) {
   NOT_IMPLEMENTED();
}

void PbftClient::ReceiveMessage(const TransportAddress &remote,
                                void *buf, size_t size) {

    static ToClientMessage client_msg;
    static PBMessage m(client_msg);

    m.Parse(buf, size);

    switch (client_msg.msg_case()) {
        case ToClientMessage::MsgCase::kReply:
            HandleReply(remote, client_msg.reply());
            break;
        case ToClientMessage::MsgCase::kUnloggedReply:
            HandleUnloggedReply(remote, client_msg.unlogged_reply());
            break;
        default:
            Panic("Received unexpected message type %u", client_msg.msg_case());
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
                                    msg.reply())) {
    return;
  }

  Debug("f + 1 replies received, current request done");
  requestTimeout->Stop();
  PendingRequest *req = pendingRequest;
  pendingRequest = nullptr;
  req->continuation(req->request, msg.reply());
  delete req;

  Assert(msg.view() >= view);
  view = msg.view();
}

}  // namespace pbft
}  // namespace dsnet
