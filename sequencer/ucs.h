#pragma once

#include <map>

#include "lib/transport.h"

namespace specpaxos {

typedef uint64_t SeqId;
typedef uint32_t GroupId;
typedef uint64_t SeqNum;

class Sequencer : public TransportReceiver {
public:
    Sequencer(const Configuration &config, Transport *transport, SeqId id);
    ~Sequencer();

    virtual ReceiveMode GetReceiveMode() override;
    virtual void ReceiveBuffer(const TransportAddress &remote,
                               void *buf, size_t len) override;

private:
    SeqNum Increment(GroupId id);

    const Configuration &config_;
    Transport *transport_;
    std::map<GroupId, SeqNum> seq_nums_;
    SeqId seq_id_;
};

} // namespace specpaxos
