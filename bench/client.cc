// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * client.cpp:
 *   test instantiation of a client application
 *
 * Copyright 2013 Dan R. K. Ports  <drkp@cs.washington.edu>
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

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/udptransport.h"

#include "bench/benchmark.h"
#include "common/client.h"
#include "lib/configuration.h"
#include "replication/vr/client.h"
#include "replication/fastpaxos/client.h"
#include "replication/unreplicated/client.h"
#include "replication/nopaxos/client.h"

#include <unistd.h>
#include <stdlib.h>
#include <fstream>

static void
Usage(const char *progName)
{
        fprintf(stderr, "usage: %s [-n requests] [-t threads] [-w warmup-secs] [-s stats-file] [-q dscp] [-d delay-ms] [-u duration-sec] -c conf-file -m unreplicated|vr|fastpaxos|nopaxos\n",
                progName);
        exit(1);
}

void
PrintReply(const string &request, const string &reply)
{
    Notice("Request succeeded; got response %s", reply.c_str());
}

int main(int argc, char **argv)
{
    const char *configPath = NULL;
    int numClients = 1;
    int duration = 1;
    int dscp = 0;
    uint64_t delay = 0;
    int tputInterval = 0;

    enum
    {
        PROTO_UNKNOWN,
        PROTO_UNREPLICATED,
        PROTO_VR,
        PROTO_FASTPAXOS,
        PROTO_SPEC,
	PROTO_NOPAXOS
    } proto = PROTO_UNKNOWN;

    string statsFile;

    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "c:d:q:s:m:t:i:u:")) != -1) {
        switch (opt) {
        case 'c':
            configPath = optarg;
            break;

        case 'd':
        {
            char *strtolPtr;
            delay = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr,
                        "option -d requires a numeric arg\n");
                Usage(argv[0]);
            }
            break;
        }

        case 'q':
        {
            char *strtolPtr;
            dscp = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (dscp < 0))
            {
                fprintf(stderr,
                        "option -q requires a numeric arg\n");
                Usage(argv[0]);
            }
            break;
        }

        case 's':
            statsFile = string(optarg);
            break;

        case 'm':
            if (strcasecmp(optarg, "unreplicated") == 0) {
                proto = PROTO_UNREPLICATED;
            } else if (strcasecmp(optarg, "vr") == 0) {
                proto = PROTO_VR;
            } else if (strcasecmp(optarg, "fastpaxos") == 0) {
                proto = PROTO_FASTPAXOS;
            } else if (strcasecmp(optarg, "nopaxos") == 0) {
                proto = PROTO_NOPAXOS;
            }
            else {
                fprintf(stderr, "unknown mode '%s'\n", optarg);
                Usage(argv[0]);
            }
            break;

        case 'u':
        {
            char *strtolPtr;
            duration = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (duration <= 0))
            {
                fprintf(stderr,
                        "option -n requires a numeric arg\n");
                Usage(argv[0]);
            }
            break;
        }

        case 't':
        {
            char *strtolPtr;
            numClients = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0') ||
                (numClients <= 0))
            {
                fprintf(stderr,
                        "option -t requires a numeric arg\n");
                Usage(argv[0]);
            }
            break;
        }

        case 'i':
        {
            char *strtolPtr;
            tputInterval = strtoul(optarg, &strtolPtr, 10);
            if ((*optarg == '\0') || (*strtolPtr != '\0'))
            {
                fprintf(stderr,
                        "option -d requires a numeric arg\n");
                Usage(argv[0]);
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown argument %s\n", argv[optind]);
            Usage(argv[0]);
            break;
        }
    }

    if (!configPath) {
        fprintf(stderr, "option -c is required\n");
        Usage(argv[0]);
    }
    if (proto == PROTO_UNKNOWN) {
        fprintf(stderr, "option -m is required\n");
        Usage(argv[0]);
    }

    // Load configuration
    std::ifstream configStream(configPath);
    if (configStream.fail()) {
        fprintf(stderr, "unable to read configuration file: %s\n",
                configPath);
        Usage(argv[0]);
    }
    dsnet::Configuration config(configStream);

    UDPTransport transport(0, 0, dscp);
    std::vector<dsnet::Client *> clients;
    std::vector<dsnet::BenchmarkClient *> benchClients;

    for (int i = 0; i < numClients; i++) {
        dsnet::Client *client;
        switch (proto) {
        case PROTO_UNREPLICATED:
            client =
                new dsnet::unreplicated::UnreplicatedClient(config,
                                                                &transport);
            break;

        case PROTO_VR:
            client = new dsnet::vr::VRClient(config, &transport);
            break;

        case PROTO_FASTPAXOS:
            client = new dsnet::fastpaxos::FastPaxosClient(config,
                                                               &transport);
            break;

	case PROTO_NOPAXOS:
	    client = new dsnet::nopaxos::NOPaxosClient(config, &transport);
	    break;

        default:
            NOT_REACHABLE();
        }

        dsnet::BenchmarkClient *bench =
            new dsnet::BenchmarkClient(*client, transport,
                                           duration, delay,
                                           tputInterval);

        transport.Timer(0, [=]() { bench->Start(); });
        clients.push_back(client);
        benchClients.push_back(bench);
    }

    Timeout checkTimeout(&transport, 100, [&]() {
            for (auto x : benchClients) {
                if (!x->done) {
                    return;
                }
            }
            Notice("All clients done.");

            Latency_t sum;
            _Latency_Init(&sum, "total");
            std::map<int, int> agg_latencies;
            uint64_t agg_ops = 0;
            for (unsigned int i = 0; i < benchClients.size(); i++) {
                Latency_Sum(&sum, &benchClients[i]->latency);
                for (const auto &kv : benchClients[i]->latencies) {
                    agg_latencies[kv.first] += kv.second;
                }
                agg_ops += benchClients[i]->completedOps;
            }
            Latency_Dump(&sum);

            Notice("Total throughput is %ld ops/sec", agg_ops / duration);
            enum class Mode {
                kMedian,
                k90,
                k95,
                k99
            };
            uint64_t count = 0;
            int median, p90, p95, p99;
            Mode mode = Mode::kMedian;
            for (const auto &kv : agg_latencies) {
                count += kv.second;
                switch (mode) {
                    case Mode::kMedian:
                        if (count >= agg_ops/2) {
                            median = kv.first;
                            Notice("Median latency is %d us", median);
                            mode = Mode::k90;
                            // fall through
                        } else {
                            break;
                        }
                    case Mode::k90:
                        if (count >= agg_ops*90/100) {
                            p90 = kv.first;
                            Notice("90th percentile latency is %d us", p90);
                            mode = Mode::k95;
                            // fall through
                        } else {
                            break;
                        }
                    case Mode::k95:
                        if (count >= agg_ops*95/100) {
                            p95 = kv.first;
                            Notice("95th percentile latency is %d us", p95);
                            mode = Mode::k99;
                            // fall through
                        } else {
                            break;
                        }
                    case Mode::k99:
                        if (count >= agg_ops*99/100) {
                            p99 = kv.first;
                            Notice("99th percentile latency is %d us", p99);
                            goto done;
                        } else {
                            break;
                        }
                }
            }

done:
            if (statsFile.size() > 0) {
                std::ofstream fs(statsFile.c_str(),
                        std::ios::out);
                fs << agg_ops / duration << std::endl;
                fs << median << " " << p90 << " " << p95 << " " << p99 << std::endl;
                for (const auto &kv : agg_latencies) {
                    fs << kv.first << " " << kv.second << std::endl;
                }
            }
            exit(0);
        });
    checkTimeout.Start();

    transport.Run();
}
