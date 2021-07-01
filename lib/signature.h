// signature.h - sign and verify message using RSA
// author: Sun Guangda <sung@comp.nus.edu.sg>

#ifndef DSNET_COMMON_SIGNATURE_H_
#define DSNET_COMMON_SIGNATURE_H_

#include <openssl/evp.h>
#include <secp256k1.h>

#include <string>

#include "lib/configuration.h"
#include "lib/transport.h"

// change back to specpaxos if we are not adopting
namespace dsnet {

class Signer {
 public:
  // return true and overwrite signature on sucess
  virtual bool Sign(const std::string &message, std::string &signature) const {
    signature = "signed";
    return true;
  }
};

class Verifier {
 public:
  // return false on both signature mismatching and verification failure
  virtual bool Verify(const std::string &message,
                      const std::string &signature) const {
    return signature == "signed";
  }
};

class RsaSigner : public Signer {
 private:
  EVP_PKEY *pkey;

 public:
  RsaSigner() : pkey(nullptr) {}
  bool SetKey(const std::string &privateKey);
  ~RsaSigner();
  bool Sign(const std::string &message, std::string &signature) const override;
};

class RsaVerifier : public Verifier {
 private:
  EVP_PKEY *pkey;

 public:
  RsaVerifier() : pkey(nullptr) {}
  bool SetKey(const std::string &publicKey);
  ~RsaVerifier();
  bool Verify(const std::string &message,
              const std::string &signature) const override;
};

class Secp256k1Signer : public Signer {
 private:
  secp256k1_context *ctx;
  unsigned char secKey[32];
  friend class Secp256k1Verifier;

 public:
  // if not specify secert key then random key will be generate
  Secp256k1Signer(const unsigned char *secKey = nullptr);
  ~Secp256k1Signer();
  bool Sign(const std::string &message, std::string &signature) const override;
};

class Secp256k1Verifier : public Verifier {
 private:
  secp256k1_context *ctx;
  secp256k1_pubkey *pubKey;

 public:
  Secp256k1Verifier(const Secp256k1Signer &signer);
  ~Secp256k1Verifier();
  bool Verify(const std::string &message,
              const std::string &signature) const override;
};

// each BFT replica/client should accept a &Security in its constructor so
// proper signature impl can be injected
class Security {
 public:
  virtual const Signer &GetReplicaSigner(int replicaIndex) const = 0;
  virtual const Verifier &GetReplicaVerifier(int replicaIndex) const = 0;
  virtual const Signer &GetClientSigner(const TransportAddress &addr) const = 0;
  virtual const Verifier &GetClientVerifier(
      const TransportAddress &addr) const = 0;
};

class NopSecurity : public Security {
 private:
  Signer s;
  Verifier v;

 public:
  NopSecurity() {}
  const Signer &GetReplicaSigner(int replicaIndex) const override { return s; }
  const Signer &GetClientSigner(const TransportAddress &addr) const override {
    return s;
  }
  const Verifier &GetReplicaVerifier(int replicaIndex) const override {
    return v;
  }
  const Verifier &GetClientVerifier(
      const TransportAddress &addr) const override {
    return v;
  }
};

class FixSingleKeySecp256k1Security : public Security {
 private:
  Secp256k1Signer s;
  Secp256k1Verifier v;
  static const unsigned char *SECRET;

 public:
  FixSingleKeySecp256k1Security() : s(SECRET), v(s) {}
  const Signer &GetReplicaSigner(int replicaIndex) const override { return s; }
  const Signer &GetClientSigner(const TransportAddress &addr) const override {
    return s;
  }
  const Verifier &GetReplicaVerifier(int replicaIndex) const override {
    return v;
  }
  const Verifier &GetClientVerifier(
      const TransportAddress &addr) const override {
    return v;
  }
};

}  // namespace dsnet

#endif