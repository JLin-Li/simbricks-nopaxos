#include "lib/signature.h"

#include <gtest/gtest.h>

#include <string>

using namespace std;
using namespace dsnet;

TEST(Signature, CanSignAndVerify) {
  std::string message = "Hello!";
  FixSingleKeySecp256k1Security sec;
  std::string signature;
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(message, signature));
  ASSERT_GT(signature.size(), 0);

  RsaVerifier verifier;
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(message, signature));
}

TEST(Signature, MultipleSignAndVerify) {
  std::string hello = "Hello!", bye = "Goodbye!";
  FixSingleKeySecp256k1Security sec;
  std::string helloSig, byeSig;
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(bye, byeSig));

  RsaVerifier verifier;
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(bye, byeSig));
  ASSERT_FALSE(sec.GetReplicaVerifier(0).Verify(hello, byeSig));
  ASSERT_FALSE(sec.GetReplicaVerifier(0).Verify(bye, helloSig));
}
