// signature.h - sign and verify message using RSA
// author: Sun Guangda <sung@comp.nus.edu.sg>

#ifndef DSNET_COMMON_SIGNATURE_H_
#define DSNET_COMMON_SIGNATURE_H_

#include <openssl/evp.h>

#include <string>

// change back to specpaxos if we are not adopting
namespace dsnet {

class Signer {
 private:
  EVP_PKEY *pkey;

 public:
  Signer() : pkey(nullptr) {}
  bool Initialize(const std::string &privateKey);
  ~Signer();

  // return true and overwrite signature on sucess
  bool Sign(const std::string &message, std::string &signature);
};

class Verifier {
 private:
  EVP_PKEY *pkey;

 public:
  Verifier() : pkey(nullptr) {}
  bool Initialize(const std::string &publicKey);
  ~Verifier();

  // return false on both signature mismatching and verification failure
  bool Verify(const std::string &message, const std::string &signature);
};

}  // namespace dsnet

#endif