//
#pragma once

#include "common/replica.h"
#include "lib/signature.h"

namespace dsnet {
namespace tombft {

class TomBFTReplica : public Replica {
 public:
  TomBFTReplica(const Configuration &config, int myIdx, bool initialize,
                Transport *transport, const Security &securtiy,
                AppReplica *app);
  ~TomBFTReplica();

  void ReceiveMessage(const TransportAddress &remote, void *buf,
                      size_t size) override;
};

}  // namespace tombft
}  // namespace dsnet
