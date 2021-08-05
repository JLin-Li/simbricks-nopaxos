#pragma once

#include <openssl/sha.h>

#include "common/pbmessage.h"
#include "lib/signature.h"

namespace dsnet {
namespace tombft {

class TomBFTMessage : public Message {
 public:
  struct __attribute__((packed)) Header {
    // no sequencing flag, every packet has this header
    // non-sequencing packet has garbage in this header
    std::uint16_t sess_num;
    std::uint64_t msg_num;
    unsigned char hmac_list[16][SHA256_DIGEST_LENGTH];
  };
  Header meta;

  TomBFTMessage(::google::protobuf::Message &msg) : pb_msg(PBMessage(msg)) {
    // session number = 0 -> invalid header
    // signature verification will not be performed
    meta.sess_num = 0;
  }
  ~TomBFTMessage() {}

 private:
  TomBFTMessage(const TomBFTMessage &msg)
      : meta(msg.meta),
        pb_msg(*std::unique_ptr<PBMessage>(msg.pb_msg.Clone())) {}

 public:
  virtual TomBFTMessage *Clone() const override {
    return new TomBFTMessage(*this);
  }
  virtual std::string Type() const override { return pb_msg.Type(); }
  virtual size_t SerializedSize() const override {
    return sizeof(Header) + pb_msg.SerializedSize();
  }
  virtual void Parse(const void *buf, size_t size) override;
  virtual void Serialize(void *buf) const override;

 private:
  PBMessage pb_msg;
};

}  // namespace tombft
}  // namespace dsnet
