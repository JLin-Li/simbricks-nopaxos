#include <fstream>
#include <cstring>

#include "lib/message.h"
#include "lib/udptransport.h"
#include "sequencer/sequencer.h"
#include "replication/nopaxos/sequencer.h"

namespace dsnet {

BufferMessage::BufferMessage(const void *buf, size_t size)
    : buf_(buf), size_(size) { }

BufferMessage::~BufferMessage() { }

std::string
BufferMessage::Type() const
{
    return std::string("Buffer Message");
}

size_t
BufferMessage::SerializedSize() const
{
    return size_;
}
void
BufferMessage::Parse(const void *buf, size_t size) { }


void
BufferMessage::Serialize(void *buf) const
{
    memcpy(buf, buf_, size_);
}

Sequencer::Sequencer(const Configuration &config, Transport *transport, int id)
    : config_(config), transport_(transport)
{
    if (config.NumSequencers() <= id) {
        Panic("Address for sequencer %d not properly configured", id);
    }
    transport_->RegisterAddress(this, config, &config.sequencer(id));
}

Sequencer::~Sequencer() { }

} // namespace dsnet

static void
Usage(const char *name)
{
    fprintf(stderr, "usage: %s -c conf-file -m nopaxos\n", name);
    exit(1);
}
int main(int argc, char *argv[]) {
    const char *config_path = nullptr;
    dsnet::Sequencer *sequencer = nullptr;
    int opt;

    enum {
        PROTO_UNKNOWN,
        PROTO_NOPAXOS
    } proto = PROTO_UNKNOWN;

    while ((opt = getopt(argc, argv, "c:m:")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;

        case 'm':
            if (strcasecmp(optarg, "nopaxos") == 0) {
                proto = PROTO_NOPAXOS;
            } else {
                Panic("Unknown sequencer mode '%s'", optarg);
            }
            break;

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
            break;
        }
    }

    if (config_path == nullptr) {
        fprintf(stderr, "option -c is required\n");
        Usage(argv[0]);
    }

    if (proto == PROTO_UNKNOWN) {
        fprintf(stderr, "option -m is required\n");
        Usage(argv[0]);
    }

    std::ifstream config_stream(config_path);
    if (config_stream.fail()) {
        Panic("unable to read configuration file: %s\n", config_path);
    }

    dsnet::Configuration config(config_stream);
    dsnet::UDPTransport transport;
    switch (proto) {
        case PROTO_NOPAXOS:
            sequencer = new dsnet::nopaxos::NOPaxosSequencer(config, &transport, 0);
            break;
        default:
            NOT_REACHABLE();
    }
    transport.Run();
    delete sequencer;

    return 0;
}
