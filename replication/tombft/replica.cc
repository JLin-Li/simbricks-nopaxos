#include "replication/tombft/replica.h"

#include "replication/tombft/message.h"
#include "replication/tombft/tombft-proto.pb.h"

namespace dsnet {
namespace tombft {

TomBFTReplica::TomBFTReplica(const Configuration &config, int myIdx,
                             bool initialize, Transport *transport,
                             const Security &securtiy, AppReplica *app)
    : Replica(config, 0, myIdx, initialize, transport, app),
      log(false),
      vs(0, 0) {
  transport->ListenOnMulticast(this, config);
  // TODO
}

void TomBFTReplica::ReceiveMessage(const TransportAddress &remote, void *buf,
                                   size_t size) {
  proto::Message msg;
  TomBFTMessage m(msg);
  m.Parse(buf, size);
  switch (msg.msg_case()) {
    case proto::Message::kRequest:
      HandleRequest(msg, m.meta, remote);
      break;
    default:
      Panic("Received unexpected message type #%u", msg.msg_case());
  }
}

void TomBFTReplica::HandleRequest(const proto::Message &msg,
                                  const TomBFTMessage::Header &meta,
                                  const TransportAddress &remote) {
  //
}

}  // namespace tombft
}  // namespace dsnet