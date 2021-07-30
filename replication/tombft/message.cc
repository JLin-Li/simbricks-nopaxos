//

#include "replication/tombft/message.h"

#include <cstring>

#include "lib/assert.h"

namespace dsnet {
namespace tombft {

void TomBFTMessage::Parse(const void *buf, size_t size) {
  const size_t header_size = sizeof(TomBFTMessage::Header);
  Assert(size > header_size);
  auto bytes = reinterpret_cast<const uint8_t *>(buf);
  std::memcpy(&meta, bytes, header_size);
  pb_msg.Parse(bytes + header_size, size - header_size);
}

void TomBFTMessage::Serialize(void *buf) const {
  const size_t header_size = sizeof(TomBFTMessage::Header);
  auto bytes = reinterpret_cast<uint8_t *>(buf);
  std::memcpy(bytes, &meta, header_size);
  pb_msg.Serialize(bytes + header_size);
}

}  // namespace tombft
}  // namespace dsnet