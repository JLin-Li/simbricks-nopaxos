#include "lib/signature.h"

#include <gtest/gtest.h>

#include <string>

#include "lib/rsakeys.h"

using namespace std;
using namespace dsnet;

TEST(Signature, CanVerifyValid) {
  std::string message = "Hello!";
  Signer signer(PRIVATE_KEY);
  ASSERT_TRUE(signer.Initialize());
  std::string signature;
  ASSERT_TRUE(signer.Sign(message, signature));
  ASSERT_TRUE(VerifySignature(PUBLIC_KEY, message, signature));
}

TEST(Signature, CanVerifyInvalid) {
  std::string message = "Hello!";
  std::string signature = SignMessage(PRIVATE_KEY, message);
  std::string forged = "Goodbye!";
  ASSERT_FALSE(VerifySignature(PUBLIC_KEY, forged, signature));
}
