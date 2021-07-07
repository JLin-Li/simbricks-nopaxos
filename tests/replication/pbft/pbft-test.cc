/***********************************************************************
 *
 * pbft-test.cc:
 *   test cases for pbft protocol
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

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>

#include "common/client.h"
#include "common/replica.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/signature.h"
#include "lib/simtransport.h"
#include "lib/transport.h"
#include "replication/pbft/client.h"
#include "replication/pbft/replica.h"

using namespace dsnet;
using namespace dsnet::pbft;
using namespace dsnet::pbft::proto;
using std::map;
using std::sprintf;
using std::string;
using std::vector;

class PbftTestApp : public AppReplica {
 public:
  vector<string> opList;
  string LastOp() { return opList.back(); }

  PbftTestApp(){};
  ~PbftTestApp(){};

  void ReplicaUpcall(opnum_t opnum, const string &req, string &reply,
                     void *arg = nullptr, void *ret = nullptr) override {
    opList.push_back(req);
    reply = "reply: " + req;
  }
};

void OneClientMultiOp(int numberOp, Security &security) {
  map<int, vector<ReplicaAddress> > replicaAddrs = {{0,
                                                     {{"localhost", "1509"},
                                                      {"localhost", "1510"},
                                                      {"localhost", "1511"},
                                                      {"localhost", "1512"}}}};
  Configuration c(1, 4, 1, replicaAddrs);
  SimulatedTransport transport(true);
  PbftTestApp app1, app2, app3, app4;
  PbftReplica replica0(c, 0, true, &transport, security, &app1);
  PbftReplica replica1(c, 1, true, &transport, security, &app2);
  PbftReplica replica2(c, 2, true, &transport, security, &app3);
  PbftReplica replica3(c, 3, true, &transport, security, &app4);
  PbftClient client(c, ReplicaAddress("localhost", "0"), &transport, security);

  int opIndex = 0;
  std::function<void(const string &, const string &)> onResp;
  onResp = [&](const string &req, const string &reply) {
    char buf[100];
    sprintf(buf, "test%d", opIndex);
    ASSERT_EQ(req, buf);
    sprintf(buf, "reply: test%d", opIndex);
    ASSERT_EQ(reply, buf);
    opIndex += 1;
    if (opIndex < numberOp) {
      sprintf(buf, "test%d", opIndex);
      client.Invoke(buf, onResp);
    }
  };

  client.Invoke("test0", onResp);
  transport.Timer(1500, [&]() { transport.Stop(); });
  transport.Run();

  ASSERT_EQ(opIndex, numberOp);
}

TEST(Pbft, 1Op) {
  NopSecurity security;
  OneClientMultiOp(1, security);
}

TEST(Pbft, 1OpSign) {
  FixSingleKeySecp256k1Security security;
  OneClientMultiOp(1, security);
}

TEST(Pbft, 100Op) {
  NopSecurity security;
  OneClientMultiOp(100, security);
}

TEST(Pbft, 100OpSign) {
  FixSingleKeySecp256k1Security security;
  OneClientMultiOp(100, security);
}