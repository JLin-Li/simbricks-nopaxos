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
  PbftReplica(const Configuration &config, int myIdx, bool initialize,
              Transport *transport, const Security &sec, AppReplica *app);
  void ReceiveMessage(const TransportAddress &remote, void *buf,
                      size_t size) override;

 private:
  const Security &security;

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
  // resend strategy
  // this implementation has two resending, resending preprepare and general state transfer
  // primary resend preprepare if itself has not received 2f replied prepare
  // if primary has received 2f prepare, there must be 2f + 1 replicas (including primary)
  // have entered prepare round, which means the system does not need any further preprepare resending
  // to progress
  // replicas schedule state transfer after
  // * they broadcast prepare (backup only) (in HandlePrePrepare)
  // * they broadcast commit (in TryBroadcastCommit)
  // * they received out-of-order preprepare (backup only) (in HandlePrePrepare)
  // currently, a scheduled state transfer only get cancelled when the seqnum reaches commit point
  // i.e. LOG_STATE_COMMITTED, which indicates that no further message need to be received for the seqnum
  // for backup, a seqnum will be (re)scheduled for state transfer for 2 or 3 times: (optional) higher preprepare received,
  // enter prepare round and enter commit round, on rescheduling the timer is reset
  // for primary, a seqnum will be scheduled for resend prepreare once, 
  // and scheduled for state transfer once when enter commit round
  // each seqnum has independent scheduling for both prepreare resending and state transfering
  // when state transfer is requested, a replica send whatever it has for a seqnum, which means:
  // * 2f prepare and 2f + 1 commit when committed
  // * 2f prepare when prepared
  // * nothing otherwise
  // in conclusion, a replica send messages in three conditions:
  // * following standard protocol spec, including preprepare, prepare and commit broadcast, and reply to client
  // * individule reply prepare/commit for delayed preprepare/prepare
  // * state transfer
  // TODO view change details
  Timeout *stateTransferTimeout;
  opnum_t tranferTarget;
  void OnStateTransfer();
  struct PendingPrePrepare {
    opnum_t seqNum;
    uint64_t clientId, clientReqId;
    std::unique_ptr<Timeout> timeout;
  };
  std::list<PendingPrePrepare> pendingPrePrepareList;

  // states and utils
  view_t view;
  opnum_t seqNum;                               // only primary use this
  int ReplicaId() const { return replicaIdx; }  // consistent naming to proto
  bool AmPrimary() const {  // following PBFT paper terminology
    return ReplicaId() == configuration.GetLeaderIndex(view);
  };

  Log log;
  std::unordered_map<opnum_t, proto::Common> acceptedPrePrepareTable;
  ByzantineProtoQuorumSet<opnum_t, proto::Common> prepareSet, commitSet;
  // prepared(m, v, n, i) where v(view) and i(replica index) should
  // be fixed for each calling
  // theoretically this verb could use const this, but underlying CheckForQuorum
  // does not, and we actually don't need it to do so, so that's it
  bool Prepared(opnum_t seqNum, const proto::Common &message) {
    return acceptedPrePrepareTable.count(seqNum) &&
           Match(acceptedPrePrepareTable[seqNum], message) &&
           prepareSet.CheckForQuorum(seqNum, message);
  }
  // similar to prepared
  bool CommittedLocal(opnum_t seqNum, const proto::Common &message) {
    return Prepared(seqNum, message) &&
           commitSet.CheckForQuorum(seqNum, message);
  }
  std::unordered_set<opnum_t> pastCommitted;

  // multi-entry common actions
  // HandleRequest, HandlePrePrepare
  // PREPARED state maps to pre-prepared in PBFT
  void AcceptPrePrepare(proto::PrePrepareMessage message);
  // AcceptPrePrepare, HandlePrePrepare, HandlePrepare
  void TryBroadcastCommit(const proto::Common &message);
  // TryBroadcastCommit, HandleCommit
  void TryExecute(const proto::Common &message);

  void ScheduleTransfer(opnum_t target);

  struct ClientTableEntry {
    uint64_t lastReqId;
    proto::ToClientMessage reply;
  };
  std::unordered_map<uint64_t, ClientTableEntry> clientTable;
  std::unordered_map<uint64_t, std::unique_ptr<TransportAddress>>
      clientAddressTable;
  void UpdateClientTable(const Request &req,
                         const proto::ToClientMessage &reply);

  static bool Match(const proto::Common &lhs, const proto::Common &rhs) {
    return lhs.SerializeAsString() == rhs.SerializeAsString();
  }
};

}  // namespace pbft
}  // namespace dsnet

#endif /* _PBFT_REPLICA_H_ */
