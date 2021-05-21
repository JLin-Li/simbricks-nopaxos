// signature.h - sign and verify message using RSA
// author: Sun Guangda <sung@comp.nus.edu.sg>

#ifndef DSNET_COMMON_SIGNATURE_H_
#define DSNET_COMMON_SIGNATURE_H_

#include <string>

// change back to specpaxos if we are not adopting
namespace dsnet {

std::string SignMessage(const std::string &privateKey, const std::string &message);
bool VerifySignature(const std::string &publicKey, const std::string &message,
                     const std::string &signature);

}  // namespace dsnet

#endif