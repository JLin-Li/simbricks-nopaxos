// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * benchmark.cpp:
 *   simple replication benchmark client
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

#include "bench/benchmark.h"
#include "common/client.h"
#include "lib/latency.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "lib/timeval.h"

#include <sys/time.h>
#include <string>
#include <sstream>
#include <algorithm>

namespace specpaxos {

DEFINE_LATENCY(op);

BenchmarkClient::BenchmarkClient(Client &client, Transport &transport,
                                 int numRequests, uint64_t delay,
                                 int warmupSec,
                                 int tputInterval)
    : tputInterval(tputInterval), client(client),
    transport(transport), numRequests(numRequests),
    delay(delay), warmupSec(warmupSec)
{
    if (delay != 0) {
        Notice("Delay between requests: %ld ms", delay);
    }
    started = false;
    done = false;
    cooldownDone = false;
    finRequests = 0;
    _Latency_Init(&latency, "op");
}

void
BenchmarkClient::Start()
{
    n = 0;
    transport.Timer(warmupSec * 1000,
                    std::bind(&BenchmarkClient::WarmupDone,
                               this));

    if (tputInterval > 0) {
	msSinceStart = 0;
	opLastInterval = n;
	transport.Timer(tputInterval, std::bind(&BenchmarkClient::TimeInterval,
						this));
    }
    SendNext();
}

void
BenchmarkClient::TimeInterval()
{
    if (done) {
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    msSinceStart += tputInterval;
    Notice("Completed %d requests at %lu ms", n-opLastInterval, (((tv.tv_sec*1000000+tv.tv_usec)/1000)/10)*10);
    opLastInterval = n;
    transport.Timer(tputInterval, std::bind(&BenchmarkClient::TimeInterval,
					    this));
}

void
BenchmarkClient::WarmupDone()
{
    started = true;
    Notice("Completed warmup period of %d seconds with %d requests",
           warmupSec, n);
    gettimeofday(&startTime, NULL);
    n = 0;
}

void
BenchmarkClient::CooldownDone()
{
    enum class Mode {
        kMedian,
        k90,
        k95,
        k99
    };

    cooldownDone = true;
    int count = 0;
    Mode mode = Mode::kMedian;

    Notice("Finished cooldown period.");

    for (const auto &kv : latencies) {
        count += kv.second;
        switch (mode) {
            case Mode::kMedian:
                if (count >= finRequests/2) {
                    Notice("Median latency is %d us", kv.first);
                    mode = Mode::k90;
                    // fall through
                } else {
                    break;
                }
            case Mode::k90:
                if (count >= finRequests*90/100) {
                    Notice("90th percentile latency is %d us", kv.first);
                    mode = Mode::k95;
                    // fall through
                } else {
                    break;
                }
            case Mode::k95:
                if (count >= finRequests*95/100) {
                    Notice("95th percentile latency is %d us", kv.first);
                    mode = Mode::k99;
                    // fall through
                } else {
                    break;
                }
            case Mode::k99:
                if (count >= finRequests*99/100) {
                    Notice("99th percentile latency is %d us", kv.first);
                    return;
                } else {
                    break;
                }
        }
    }
}

void
BenchmarkClient::SendNext()
{
    std::ostringstream msg;
    msg << "request" << n;

    Latency_Start(&latency);
    client.Invoke(msg.str(), std::bind(&BenchmarkClient::OnReply,
                                       this,
                                       std::placeholders::_1,
                                       std::placeholders::_2));
}

void
BenchmarkClient::OnReply(const string &request, const string &reply)
{
    if (cooldownDone) {
        return;
    }

    if ((started) && (!done) && (n != 0)) {
        uint64_t ns = Latency_End(&latency);
        latencies[ns/1000]++;
        finRequests++;
        if (n > numRequests) {
            Finish();
        }
    }

    n++;
    if (delay == 0) {
       SendNext();
    } else {
        uint64_t rdelay = rand() % delay*2;
        transport.Timer(rdelay,
                        std::bind(&BenchmarkClient::SendNext, this));
    }
}

void
BenchmarkClient::Finish()
{
    gettimeofday(&endTime, NULL);

    struct timeval diff = timeval_sub(endTime, startTime);

    Notice("Completed %d requests in " FMT_TIMEVAL_DIFF " seconds",
           numRequests, VA_TIMEVAL_DIFF(diff));
    done = true;

    transport.Timer(warmupSec * 1000,
                    std::bind(&BenchmarkClient::CooldownDone,
                              this));
}

} // namespace specpaxos
