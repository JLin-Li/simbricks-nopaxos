#include "replication/nopaxos/sequencer.h"

namespace dsnet {
namespace nopaxos {

typedef uint16_t HeaderSize;

NOPaxosSequencer::NOPaxosSequencer(const Configuration &config,
                                   Transport *transport, int id)
    : Sequencer(config, transport, id),
      sess_num_(0), msg_num_(0) { }

NOPaxosSequencer::~NOPaxosSequencer() { }

void
NOPaxosSequencer::ReceiveMessage(const TransportAddress &remote, void *buf, size_t size)
{
    char *p = (char *)buf;
    HeaderSize header_sz = *(HeaderSize *)p;
    p += sizeof(HeaderSize);
    if (header_sz > 0) {
        // Session number
        *(SessNum *)p = htobe64(sess_num_);
        p += sizeof(SessNum);
        // Message number
        *(MsgNum *)p = htobe64(++msg_num_);
        p += sizeof(MsgNum);

        transport_->SendMessageToAll(this, BufferMessage(buf, size));
    }
}

} // namespace nopaxos
} // namespace dsnet
