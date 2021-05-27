#include "lib/signature.h"

#include <gtest/gtest.h>

#include <string>

#include "lib/rsakeys.h"

using namespace std;
using namespace dsnet;

TEST(Signature, CanSignAndVerify) {
  std::string message = "Hello!";
  Signer signer;
  ASSERT_TRUE(signer.Initialize(PRIVATE_KEY));
  std::string signature;
  ASSERT_TRUE(signer.Sign(message, signature));
  ASSERT_GT(signature.size(), 0);

  Verifier verifier;
  ASSERT_TRUE(verifier.Initialize(PUBLIC_KEY));
  ASSERT_TRUE(verifier.Verify(message, signature));
}

TEST(Signature, MultipleSignAndVerify) {
  std::string hello = "Hello!", bye = "Goodbye!";
  Signer signer;
  ASSERT_TRUE(signer.Initialize(PRIVATE_KEY));
  std::string helloSig, byeSig;
  ASSERT_TRUE(signer.Sign(hello, helloSig));
  ASSERT_TRUE(signer.Sign(bye, byeSig));

  Verifier verifier;
  ASSERT_TRUE(verifier.Initialize(PUBLIC_KEY));
  ASSERT_TRUE(verifier.Verify(hello, helloSig));
  ASSERT_TRUE(verifier.Verify(hello, helloSig));
  ASSERT_TRUE(verifier.Verify(bye, byeSig));
  ASSERT_FALSE(verifier.Verify(hello, byeSig));
  ASSERT_FALSE(verifier.Verify(bye, helloSig));
}
