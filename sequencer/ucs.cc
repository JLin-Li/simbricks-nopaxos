#include <fstream>

#include "lib/message.h"
#include "lib/udptransport.h"
#include "sequencer/ucs.h"

namespace specpaxos {

const uint32_t NONFRAG_MAGIC = 0x20050318;

Sequencer::Sequencer(const Configuration &config, Transport *transport, SeqId id)
    : config_(config), transport_(transport), seq_id_(id)
{
    if (config.NumSequencers() < 1) {
        Panic("Configuration requires at least one sequencer address");
    }
    transport_->RegisterAddress(this, config, &config.sequencer(0));
}

Sequencer::~Sequencer() { }

TransportReceiver::ReceiveMode
Sequencer::GetReceiveMode()
{
    return TransportReceiver::ReceiveMode::kReceiveBuffer;
}

void
Sequencer::ReceiveBuffer(const TransportAddress &remote, void *buf, size_t len)
{
    uint8_t *ptr = (uint8_t *)buf;
    size_t ngroups;

    if (*(uint32_t *)ptr != NONFRAG_MAGIC) {
        return;
    }
    ptr += sizeof(uint32_t) + sizeof(uint32_t);

    // Source address
    std::string src_addr = remote.Serialize();
    memcpy(ptr, src_addr.data(), src_addr.size());
    ptr += src_addr.size();

    // Session number
    *(SeqId *)ptr = htobe64(seq_id_);
    ptr += sizeof(SeqId);

    // Multi-stamp
    ngroups = ntohl(*(uint32_t *)ptr);
    ptr += sizeof(uint32_t);
    for (size_t i = 0; i < ngroups; i++) {
        GroupId groupid = ntohl(*(GroupId *)ptr);
        ptr += sizeof(GroupId);
        *(SeqNum *)ptr = htobe64(Increment(groupid));
        ptr += sizeof(SeqNum);
    }

    transport_->SendBufferToAll(this, buf, len);
}

SeqNum
Sequencer::Increment(GroupId id) {
    if (seq_nums_.find(id) == seq_nums_.end()) {
        seq_nums_.insert(std::make_pair(id, 0));
    }
    return ++seq_nums_[id];
}

} // namespace specpaxos

int main(int argc, char *argv[]) {
    const char *config_path = nullptr;
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
            break;
        }
    }

    if (config_path == nullptr) {
        Panic("option -c is required\n");
    }

    std::ifstream config_stream(config_path);
    if (config_stream.fail()) {
        Panic("unable to read configuration file: %s\n", config_path);
    }

    specpaxos::Configuration config(config_stream);
    UDPTransport transport;
    specpaxos::Sequencer sequencer(config, &transport, 0);
    transport.Run();

    return 0;
}
