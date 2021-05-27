// signature.h - sign and verify message using RSA
// author: Sun Guangda <sung@comp.nus.edu.sg>

#ifndef DSNET_COMMON_SIGNATURE_H_
#define DSNET_COMMON_SIGNATURE_H_

#include <openssl/rsa.h>

#include <string>

// change back to specpaxos if we are not adopting
namespace dsnet {

std::string SignMessage(const std::string &privateKey,
                        const std::string &message);
bool VerifySignature(const std::string &publicKey, const std::string &message,
                     const std::string &signature);

class Signer {
 private:
  const std::string privateKey;
  RSA *rsa;

 public:
  Signer(const std::string &privateKey)
      : privateKey(privateKey), rsa(nullptr) {}
  bool Initialize();
  ~Signer();

  // return true and overwrite signature on sucess
  bool Sign(const std::string &message, std::string &signature);
};

}  // namespace dsnet

#endif