//
#pragma once

#include "common/replica.h"
#include "lib/signature.h"
#include "replication/tombft/message.h"
#include "replication/tombft/tombft-proto.pb.h"

namespace dsnet {
namespace tombft {

class TomBFTReplica : public Replica {
 public:
  TomBFTReplica(const Configuration &config, int myIdx, bool initialize,
                Transport *transport, const Security &securtiy,
                AppReplica *app);
  ~TomBFTReplica() {}

  void ReceiveMessage(const TransportAddress &remote, void *buf,
                      size_t size) override;

 private:
  void HandleRequest(const proto::Message &msg,
                     const TomBFTMessage::Header &meta,
                     const TransportAddress &remote);

  viewstamp_t vs;
  Log log;
};

}  // namespace tombft
}  // namespace dsnet
