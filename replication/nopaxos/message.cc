#include "replication/nopaxos/nopaxos-proto.pb.h"
#include "replication/nopaxos/message.h"

namespace dsnet {
namespace nopaxos {

/*
 * Packet format:
 * stamp len  + sess num + msg num
 */
typedef uint16_t StampSize;

size_t
Stamp::SerializedSize() const
{
    return sizeof(SessNum) + sizeof(MsgNum);
}

NOPaxosMessage::NOPaxosMessage(::google::protobuf::Message &msg, bool sequencing)
    : PBMessage(msg), sequencing_(sequencing) { }

NOPaxosMessage::~NOPaxosMessage() { }

size_t
NOPaxosMessage::SerializedSize() const
{
    size_t sz = sizeof(StampSize);
    if (sequencing_) {
        sz += stamp_.SerializedSize();
    }
    return sz + PBMessage::SerializedSize();
}

void
NOPaxosMessage::Parse(const void *buf, size_t size)
{
    const char *p = (const char*)buf;
    StampSize stamp_sz = *(StampSize *)p;
    p += sizeof(StampSize);
    if (stamp_sz > 0) {
        stamp_.sess_num = *(SessNum *)p;
        p += sizeof(SessNum);
        stamp_.msg_num = *(MsgNum *)p;
        p += sizeof(MsgNum);
    }
    PBMessage::Parse(p, size - sizeof(StampSize) - stamp_sz);
    if (stamp_sz > 0) {
        // Only client request message will be tagged with multistamp
        proto::ToReplicaMessage &to_replica =
            dynamic_cast<proto::ToReplicaMessage &>(*msg_);
        if (to_replica.msg_case() !=
                proto::ToReplicaMessage::MsgCase::kRequest) {
            Panic("Received stamp with wrong message type");
        }
        proto::RequestMessage *request = to_replica.mutable_request();
        request->set_sessnum(stamp_.sess_num);
        request->set_msgnum(stamp_.msg_num);
    }
}

void
NOPaxosMessage::Serialize(void *buf) const
{
    char *p = (char *)buf;
    *(StampSize *)p = sequencing_ ? stamp_.SerializedSize() : 0;
    p += sizeof(StampSize);
    if (sequencing_) {
        // sess num filled by sequencer
        p += sizeof(SessNum);
        // msg num filled by sequencer
        p += sizeof(MsgNum);
    }
    PBMessage::Serialize(p);
}

Stamp &
NOPaxosMessage::GetStamp()
{
    return stamp_;
}

} // namespace nopaxos
} // namespace dsnet
