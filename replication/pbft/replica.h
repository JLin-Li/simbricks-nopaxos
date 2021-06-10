/***********************************************************************
 *
 * pbft/replica.h:
 *   PBFT protocol replica
 *   This is only a fast-path performance-equivalent implmentation. Noticable
 *   missing parts include recovery, crash tolerance (i.e. view changing) and
 *   part of Byzantine
 *
 * Copyright 2013 Dan R. K. Ports  <drkp@cs.washington.edu>
 * Copyright 2021 Sun Guangda      <sung@comp.nus.edu.sg>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/

#ifndef _PBFT_REPLICA_H_
#define _PBFT_REPLICA_H_

#include "common/log.h"
#include "common/quorumset.h"
#include "common/replica.h"
#include "lib/signature.h"
#include "replication/pbft/pbft-proto.pb.h"

namespace dsnet {
namespace pbft {

class PbftReplica : public Replica {
 public:
  PbftReplica(Configuration config, int myIdx, bool initialize,
              Transport *transport, AppReplica *app);
  void ReceiveMessage(const TransportAddress &remote, const string &type,
                      const string &data, void *meta_data) override;

 private:
  // fundamental
  Signer signer;
  Verifier verifier;

  // message handlers
  void HandleRequest(const TransportAddress &remote,
                     const proto::RequestMessage &msg);
  void HandlePrePrepare(const TransportAddress &remote,
                        const proto::PrePrepareMessage &msg);
  void HandlePrepare(const TransportAddress &remote,
                     const proto::PrepareMessage &msg);
  void HandleCommit(const TransportAddress &remote,
                    const proto::CommitMessage &msg);

  // timers and timeout handlers
  Timeout *viewChangeTimeout;
  void OnViewChange();

  // states and utils
  view_t view;
  opnum_t seqNum;                               // only primary use this
  int ReplicaId() const { return replicaIdx; }  // consistent naming to proto
  bool AmPrimary() const {  // following PBFT paper terminology
    return ReplicaId() == configuration.GetLeaderIndex(view);
  };

  Log log;
  // get cleared on view changing
  std::map<opnum_t, proto::Common> acceptedPrePrepareTable;
  // PREPARED state maps to pre-prepared in PBFT
  void AcceptPrePrepare(proto::PrePrepareMessage message);

  static bool Match(const proto::Common &lhs, const proto::Common &rhs) {
    return lhs.SerializeAsString() == rhs.SerializeAsString();
  }

  // get cleared on view changing
  ByzantineProtoQuorumSet<opnum_t, proto::Common> prepareSet, commitSet;
  // prepared(m, v, n, i) where v(view) and i(replica index) should
  // be fixed for each calling
  // theoretically this verb could use const this, but underlying CheckForQuorum
  // does not, and we actually don't need it to do so, so that's it
  bool Prepared(opnum_t seqNum, proto::Common message) {
    return acceptedPrePrepareTable.count(seqNum) &&
           Match(acceptedPrePrepareTable[seqNum], message) &&
           prepareSet.CheckForQuorum(seqNum, message);
  }
  void TryBroadcastCommit(const proto::Common &message);
  // similar to prepared
  bool CommittedLocal(opnum_t seqNum, proto::Common message) {
    return Prepared(seqNum, message) &&
           commitSet.CheckForQuorum(seqNum, message);
  }

  struct ClientTableEntry {
    uint64_t lastReqId;
    proto::ReplyMessage reply;
  };
  std::map<uint64_t, ClientTableEntry> clientTable;
  void UpdateClientTable(const Request &req, const proto::ReplyMessage &reply);
};

}  // namespace pbft
}  // namespace dsnet

#endif /* _PBFT_REPLICA_H_ */
