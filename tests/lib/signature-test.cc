#include "lib/signature.h"

#include <gtest/gtest.h>

#include <string>

#include "lib/configuration.h"

using namespace std;
using namespace dsnet;

map<int, vector<ReplicaAddress> > replicaAddrs = {{0,
                                                   {{"localhost", "1509"},
                                                    {"localhost", "1510"},
                                                    {"localhost", "1511"},
                                                    {"localhost", "1512"}}}};
Configuration c(1, 4, 1, replicaAddrs);

TEST(Signature, CanSignAndVerify) {
  std::string message = "Hello!";
  Secp256k1Signer signer;
  Secp256k1Verifier verifier(signer);
  HomogeneousSecurity sec(c, signer, verifier);
  std::string signature;
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(message, signature));
  ASSERT_GT(signature.size(), 0);
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(message, signature));
}

TEST(Signature, MultipleSignAndVerify) {
  std::string hello = "Hello!", bye = "Goodbye!";
  Secp256k1Signer signer;
  Secp256k1Verifier verifier(signer);
  HomogeneousSecurity sec(c, signer, verifier);
  std::string helloSig, byeSig;
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaSigner(0).Sign(bye, byeSig));
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(hello, helloSig));
  ASSERT_TRUE(sec.GetReplicaVerifier(0).Verify(bye, byeSig));
  ASSERT_FALSE(sec.GetReplicaVerifier(0).Verify(hello, byeSig));
  ASSERT_FALSE(sec.GetReplicaVerifier(0).Verify(bye, helloSig));
}
