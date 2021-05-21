#ifndef _PBFT_CLIENT_H_
#define _PBFT_CLIENT_H_

#include <map>
#include <set>

#include "common/client.h"
#include "lib/configuration.h"
#include "replication/pbft/pbft-proto.pb.h"

namespace dsnet {
namespace pbft {

class PbftClient : public Client {
 public:
  PbftClient(const Configuration &config, Transport *transport,
             uint64_t clientid = 0);
  virtual ~PbftClient();
  virtual void Invoke(const string &request,
                      continuation_t continuation) override;
  virtual void InvokeUnlogged(
      int replicaIdx, const string &request, continuation_t continuation,
      timeout_continuation_t timeoutContinuation = nullptr,
      uint32_t timeout = DEFAULT_UNLOGGED_OP_TIMEOUT) override;
  virtual void ReceiveMessage(const TransportAddress &remote,
                              const string &type, const string &data,
                              void *meta_data) override;

 private:
  int f;  // the number of faulty servers that could be toleranced

  struct PendingRequest {
    string request;
    uint64_t clientreqid;
    continuation_t continuation;
    // in each group the result is the same
    std::map<std::string, std::set<int>> replyGroupMap;
    PendingRequest(string request, uint64_t clientreqid,
                   continuation_t continuation)
        : request(request),
          clientreqid(clientreqid),
          continuation(continuation) {}
  };

  uint64_t lastReqId;
  PendingRequest *pendingRequest;
  Timeout *requestTimeout;

  PendingRequest *pendingUnloggedRequest;
  Timeout *unloggedRequestTimeout;
  timeout_continuation_t unloggedTimeoutContinuation;

  void HandleReply(const TransportAddress &remote,
                   const proto::ReplyMessage &msg);
  void HandleUnloggedReply(const TransportAddress &remote,
                           const proto::UnloggedReplyMessage &msg);

  // only for (logged request)
  void SendRequest();
  void ResendRequest();
};

}  // namespace pbft
}  // namespace specpaxos

#endif /* _PBFT_CLIENT_H_ */
