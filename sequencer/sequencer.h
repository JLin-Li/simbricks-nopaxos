#pragma once

#include <map>

#include "lib/transport.h"

namespace dsnet {

typedef uint16_t SessNum;
typedef uint64_t MsgNum;

class BufferMessage : public Message {
public:
    BufferMessage(const void *buf, size_t size);
    ~BufferMessage();

    virtual std::string Type() const override;
    virtual size_t SerializedSize() const override;
    virtual void Parse(const void *buf, size_t size) override;
    virtual void Serialize(void *buf) const override;

private:
    const void *buf_;
    size_t size_;
};

class Sequencer : public TransportReceiver {
public:
    Sequencer(const Configuration &config, Transport *transport, int id);
    ~Sequencer();

    virtual void ReceiveMessage(const TransportAddress &remote,
                               void *buf, size_t size) override;

private:
    const Configuration &config_;
    Transport *transport_;
    SessNum sess_num_;
    MsgNum msg_num_;
};

} // namespace dsnet
