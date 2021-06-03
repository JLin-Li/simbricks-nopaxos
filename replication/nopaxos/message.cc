#include "lib/message.h"
#include "replication/nopaxos/nopaxos-proto.pb.h"
#include "replication/nopaxos/message.h"

namespace dsnet {
namespace nopaxos {

using namespace proto;

/*
 * Packet format:
 * stamp len  + sess num + msg num
 */
typedef uint16_t StampSize;

NOPaxosMessage::NOPaxosMessage(::google::protobuf::Message &msg, bool sequencing)
    : PBMessage(msg), sequencing_(sequencing) { }

NOPaxosMessage::~NOPaxosMessage() { }

size_t
NOPaxosMessage::SerializedSize() const
{
    size_t sz = sizeof(StampSize);
    if (sequencing_) {
        sz += sizeof(SessNum) + sizeof(MsgNum);
    }
    return sz + PBMessage::SerializedSize();
}

void
NOPaxosMessage::Parse(const void *buf, size_t size)
{
    SessNum sess_num;
    MsgNum msg_num;
    const char *p = (const char*)buf;
    StampSize stamp_sz = *(StampSize *)p;
    p += sizeof(StampSize);
    if (stamp_sz > 0) {
        sess_num = *(SessNum *)p;
        p += sizeof(SessNum);
        msg_num = *(MsgNum *)p;
        p += sizeof(MsgNum);
    }
    PBMessage::Parse(p, size - sizeof(StampSize) - stamp_sz);
    if (stamp_sz > 0) {
        // Only client request message will be tagged with multistamp
        ToReplicaMessage &to_replica =
            dynamic_cast<proto::ToReplicaMessage &>(*msg_);
        if (to_replica.msg_case() !=
                proto::ToReplicaMessage::MsgCase::kRequest) {
            Panic("Received stamp with wrong message type");
        }
        proto::RequestMessage *request = to_replica.mutable_request();
        request->set_sessnum(sess_num);
        request->set_msgnum(msg_num);
    }
}

void
NOPaxosMessage::Serialize(void *buf) const
{
    char *p = (char *)buf;
    *(StampSize *)p = sequencing_ ? sizeof(SessNum) + sizeof(MsgNum) : 0;
    p += sizeof(StampSize);
    if (sequencing_) {
        // sess num filled by sequencer
        p += sizeof(SessNum);
        // msg num filled by sequencer
        p += sizeof(MsgNum);
    }
    PBMessage::Serialize(p);
}

} // namespace nopaxos
} // namespace dsnet
