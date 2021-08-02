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
  RsaSigner(const std::string &privateKey);
  ~RsaSigner();
  bool Sign(const std::string &message, std::string &signature) const override;
};

class RsaVerifier : public Verifier {
 private:
  EVP_PKEY *pkey;

 public:
  RsaVerifier(const std::string &publicKey);
  ~RsaVerifier();
  bool Verify(const std::string &message,
              const std::string &signature) const override;
};

class Secp256k1Signer : public Signer {
 private:
  static const unsigned char *kDefaultSecret;
  secp256k1_context *ctx;
  unsigned char secKey[32];
  friend class Secp256k1Verifier;

 public:
  // if not specify secert key then random key will be generate
  Secp256k1Signer(
      const unsigned char *secKey = Secp256k1Signer::kDefaultSecret);
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
 private:
  const Configuration &config;

 public:
  Security(const Configuration &config) : config(config) {}
  virtual const Signer &GetSigner(const ReplicaAddress &address) const = 0;
  virtual const Verifier &GetVerifier(const ReplicaAddress &address) const = 0;

  virtual const Signer &GetReplicaSigner(int replica_id) const {
    return GetSigner(config.replica(0, replica_id));
  }
  virtual const Verifier &GetReplicaVerifier(int replica_id) const {
    return GetVerifier(config.replica(0, replica_id));
  }
  virtual const Signer &GetSequencerSigner(int index = 0) const {
    return GetSigner(config.sequencer(index));
  }
  virtual const Verifier &GetSequencerVerifier(int index = 0) const {
    return GetVerifier(config.sequencer(index));
  }
};

// for bench
class HomogeneousSecurity : public Security {
 private:
  const Signer &s, &seq_s;
  const Verifier &v, &seq_v;

 public:
  HomogeneousSecurity(const Configuration &config, const Signer &s,
                      const Verifier &v, const Signer &seq_s,
                      const Verifier &seq_v)
      : Security(config), s(s), v(v), seq_s(s), seq_v(v) {}
  HomogeneousSecurity(const Configuration &config, const Signer &s,
                      const Verifier &v)
      : HomogeneousSecurity(config, s, v, s, v) {}

  virtual const Signer &GetSigner(
      const ReplicaAddress &address) const override {
    return s;
  }
  virtual const Verifier &GetVerifier(
      const ReplicaAddress &address) const override {
    return v;
  }
  virtual const Signer &GetSequencerSigner(int index) const override {
    return seq_s;
  }
  virtual const Verifier &GetSequencerVerifier(int index) const override {
    return seq_v;
  }
};

// for debug
// the only way to fail NopSecurity is to Verify without Sign
class NopSecurity : public HomogeneousSecurity {
 private:
  const static Signer s;
  const static Verifier v;

 public:
  NopSecurity(const Configuration &config)
      : HomogeneousSecurity(config, NopSecurity::s, NopSecurity::v) {}
};

}  // namespace dsnet

#endif