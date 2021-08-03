#include <gtest/gtest.h>

#include "lib/signature.h"
#include "replication/tombft/message.h"
#include "replication/tombft/tombft-proto.pb.h"

using namespace dsnet;
using namespace dsnet::tombft;

TEST(TomBFT, MessageSerDe) {
  NopSecurity s;

  proto::Message msg;
  TomBFTMessage m(msg);
  dsnet::Request req;
  req.set_clientid(42);
  req.set_clientreqid(1);
  req.set_op("test op");
  *msg.mutable_request()->mutable_req() = req;
  s.ClientSigner().Sign(req.SerializeAsString(),
                        *msg.mutable_request()->mutable_sig());
  m.meta.sess_num = 0;
  m.meta.msg_num = 73;
  const size_t m_size = m.SerializedSize();
  ASSERT_LT(m_size, 1500);

  uint8_t *msg_buf = new uint8_t[m_size];
  m.Serialize(msg_buf);

  proto::Message parsed_msg;
  TomBFTMessage parsed_m(parsed_msg);
  parsed_m.Parse(msg_buf, m_size);
  ASSERT_EQ(parsed_m.meta.sess_num, m.meta.sess_num);
  ASSERT_EQ(parsed_m.meta.msg_num, m.meta.msg_num);
  ASSERT_TRUE(
      s.ClientVerifier().Verify(parsed_msg.request().req().SerializeAsString(),
                                parsed_msg.request().sig()));
  ASSERT_EQ(parsed_msg.request().req().clientid(),
            msg.request().req().clientid());
  ASSERT_EQ(parsed_msg.request().req().clientreqid(),
            msg.request().req().clientreqid());
  ASSERT_EQ(parsed_msg.request().req().op(), msg.request().req().op());
}