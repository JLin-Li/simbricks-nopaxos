#include <fstream>
#include <cstring>

#include "lib/message.h"
#include "lib/udptransport.h"
#include "sequencer/sequencer.h"

namespace dsnet {

typedef uint16_t HeaderSize;

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
    : config_(config), transport_(transport), sess_num_(0), msg_num_(0)
{
    if (config.NumSequencers() <= id) {
        Panic("Address for sequencer %d not properly configured", id);
    }
    transport_->RegisterAddress(this, config, &config.sequencer(id));
}

Sequencer::~Sequencer() { }

void
Sequencer::ReceiveMessage(const TransportAddress &remote, void *buf, size_t size)
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

} // namespace dsnet

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

    dsnet::Configuration config(config_stream);
    dsnet::UDPTransport transport;
    dsnet::Sequencer sequencer(config, &transport, 0);
    transport.Run();

    return 0;
}
