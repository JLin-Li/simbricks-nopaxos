// digest.c - sign and verify message using RSA
// author: Sun Guangda <sung@comp.nus.edu.sg>
// adopt from https://gist.github.com/irbull/08339ddcd5686f509e9826964b17bb59
// TODO: figure out how to chain a pipeline instead of creating temporary
// objects on every sign/verify to improve performance
// TODO: test for memory leaking

#include "signature.h"

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include <cstring>

namespace {

// https://gist.github.com/irbull/08339ddcd5686f509e9826964b17bb59
size_t calcDecodeLength(const char *b64input) {
  size_t len = strlen(b64input), padding = 0;

  if (b64input[len - 1] == '=' &&
      b64input[len - 2] == '=')  // last two chars are =
    padding = 2;
  else if (b64input[len - 1] == '=')  // last char is =
    padding = 1;
  return (len * 3) / 4 - padding;
}

}  // namespace

bool dsnet::Signer::Initialize(const std::string &privateKey) {
  BIO *keybio = BIO_new_mem_buf(privateKey.c_str(), -1);
  if (!keybio) return false;
  RSA *rsa = nullptr;
  rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa, nullptr, nullptr);
  if (!rsa) return false;
  pkey = EVP_PKEY_new();
  if (!pkey) return false;

  EVP_PKEY_assign_RSA(pkey, rsa);
  return true;
}

dsnet::Signer::~Signer() {
  // todo
  // signer should only be instantiated once per client/replica
  // so the memory leaking should be fine
}

// assume panic on failure so no "finally" clean up
bool dsnet::Signer::Sign(const std::string &message, std::string &signature) {
  // create binary signature
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
    return false;
  if (EVP_DigestSignUpdate(context, message.c_str(), message.size()) <= 0)
    return false;
  size_t binSigSize;
  if (EVP_DigestSignFinal(context, nullptr, &binSigSize) <= 0) return false;
  unsigned char *binSig = new unsigned char[binSigSize];
  if (!binSig) return false;
  if (EVP_DigestSignFinal(context, binSig, &binSigSize) <= 0) return false;
  EVP_MD_CTX_free(context);

  // (optional) create base64 signature
  BIO *b64 = BIO_new(BIO_f_base64());
  BIO *bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);
  BIO_write(bio, binSig, binSigSize);
  BIO_flush(bio);
  char *sigPtr;
  long sigSize = BIO_get_mem_data(bio, &sigPtr);
  signature.assign(sigPtr, sigSize);
  BIO_set_close(bio, BIO_NOCLOSE);
  BIO_free_all(bio);
  delete[] binSig;

  return true;
}

bool dsnet::Verifier::Initialize(const std::string &publicKey) {
  BIO *keybio = BIO_new_mem_buf(publicKey.c_str(), -1);
  if (!keybio) return false;
  RSA *rsa = nullptr;
  rsa = PEM_read_bio_RSA_PUBKEY(keybio, &rsa, nullptr, nullptr);
  if (!rsa) return false;
  pkey = EVP_PKEY_new();
  if (!pkey) return false;

  EVP_PKEY_assign_RSA(pkey, rsa);
  return true;
}

dsnet::Verifier::~Verifier() {
  // todo: same as above
}

bool dsnet::Verifier::Verify(const std::string &message,
                             const std::string &signature) {
  int bufferSize = calcDecodeLength(signature.c_str());
  unsigned char *binSig = new unsigned char[bufferSize + 1];
  if (!binSig) return false;
  binSig[bufferSize] = 0;

  BIO *bio = BIO_new_mem_buf(signature.c_str(), -1);
  BIO *b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  size_t binSigSize = BIO_read(bio, binSig, signature.size());
  BIO_free_all(bio);

  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (EVP_DigestVerifyInit(context, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
    return false;
  if (EVP_DigestVerifyUpdate(context, message.c_str(), message.size()) <= 0) {
    return false;
  }
  if (EVP_DigestVerifyFinal(context, binSig, binSigSize) != 1) {
    return false;
  }
  EVP_MD_CTX_free(context);
  delete[] binSig;
  return true;
}