#include "replication/tombft/replica.h"

namespace dsnet {
namespace tombft {

TomBFTReplica::TomBFTReplica(const Configuration &config, int myIdx,
                             bool initialize, Transport *transport,
                             const Security &securtiy, AppReplica *app)
    : Replica(config, 0, myIdx, initialize, transport, app) {
  //
}

}  // namespace tombft
}  // namespace dsnet