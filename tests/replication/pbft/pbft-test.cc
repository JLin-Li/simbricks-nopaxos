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
#include <stdio.h>
#include <stdlib.h>

#include "common/client.h"
#include "common/replica.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/simtransport.h"
#include "lib/transport.h"
#include "replication/pbft/client.h"
#include "replication/pbft/replica.h"

using namespace dsnet;
using namespace dsnet::pbft;
using namespace dsnet::pbft::proto;
using std::map;
using std::vector;

static string replicaLastUnloggedOp;
static string clientLastOp;
static string clientLastReply;

class PbftTestApp : public AppReplica {
 public:
  string replicaLastOp;

  PbftTestApp(){};
  ~PbftTestApp(){};

  void ReplicaUpcall(opnum_t opnum, const string &req, string &reply,
                     void *arg = nullptr, void *ret = nullptr) override {
    replicaLastOp = req;
    reply = "reply: " + req;
  }

  void UnloggedUpcall(const string &req, string &reply) {
    replicaLastUnloggedOp = req;
    reply = "unlreply: " + req;
  }
};

static void ClientUpcallHandler(const string &req, const string &reply) {
  clientLastOp = req;
  clientLastReply = reply;
}

TEST(Pbft, OneOp) {
  map<int, vector<ReplicaAddress> > replicaAddrs = {
      {0, {{"localhost", "12345"}}}};
  Configuration c(1, 1, 0, replicaAddrs);
  SimulatedTransport transport;
  PbftTestApp app;
  PbftReplica replica(c, 0, true, &transport, &app);
  PbftClient client(c, ReplicaAddress("localhost", "0"), &transport);

  client.Invoke(string("test"), ClientUpcallHandler);
  transport.Run();

  EXPECT_EQ(app.replicaLastOp, "test");
  EXPECT_EQ(clientLastOp, "test");
  EXPECT_EQ(clientLastReply, "reply: test");
}

TEST(Pbft, OneOpFourServers) {
  map<int, vector<ReplicaAddress> > replicaAddrs = {{0,
                                                     {{"localhost", "1509"},
                                                      {"localhost", "1510"},
                                                      {"localhost", "1511"},
                                                      {"localhost", "1512"}}}};
  Configuration c(1, 4, 1, replicaAddrs);
  SimulatedTransport transport(true);
  PbftTestApp app1, app2, app3, app4;
  PbftReplica replica0(c, 0, true, &transport, &app1);
  PbftReplica replica1(c, 1, true, &transport, &app2);
  PbftReplica replica2(c, 2, true, &transport, &app3);
  PbftReplica replica3(c, 3, true, &transport, &app4);
  PbftClient client(c, ReplicaAddress("localhost", "0"), &transport);

  client.Invoke(string("test3"), ClientUpcallHandler);
  transport.Timer(3000, [&]() { transport.Stop(); });
  transport.Run();

  EXPECT_EQ(app1.replicaLastOp, "test3");
  EXPECT_EQ(app2.replicaLastOp, "test3");
  EXPECT_EQ(app3.replicaLastOp, "test3");
  EXPECT_EQ(app4.replicaLastOp, "test3");
  EXPECT_EQ(clientLastOp, "test3");
  EXPECT_EQ(clientLastReply, "reply: test3");
}
