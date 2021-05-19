// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * transport.h:
 *   message-passing network interface definition
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

#ifndef _LIB_TRANSPORT_H_
#define _LIB_TRANSPORT_H_

#include "lib/configuration.h"

#include <google/protobuf/message.h>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>

class TransportAddress
{
public:
    virtual ~TransportAddress() { }
    virtual TransportAddress *clone() const = 0;
    virtual std::string Serialize() const = 0;
};

class TransportReceiver
{
protected:
    typedef ::google::protobuf::Message Message;

public:
    enum class ReceiveMode {
        kReceiveMessage,
        kReceiveBuffer
    };

    virtual ~TransportReceiver();
    virtual void SetAddress(const TransportAddress *addr);
    virtual const TransportAddress& GetAddress();
    virtual ReceiveMode GetReceiveMode();
    virtual void ReceiveMessage(const TransportAddress &remote,
                                const string &type,
                                const string &data,
                                void * meta_data);
    virtual void ReceiveBuffer(const TransportAddress &remote,
                               void *buf, size_t len);

protected:
    const TransportAddress *myAddress;
};

typedef std::function<void (void)> timer_callback_t;

// ordered multicast meta data
typedef struct {
    sessnum_t sessnum;
    std::unordered_map<shardnum_t, msgnum_t> seqnums;
} multistamp_t;

class Transport
{
protected:
    typedef ::google::protobuf::Message Message;
public:
    virtual ~Transport() {}
    virtual void RegisterReplica(TransportReceiver *receiver,
                                 const dsnet::Configuration &config,
                                 int groupIdx,
                                 int replicaIdx) = 0;
    /* Set addr to nullptr if receiver can be bound to any address */
    virtual void RegisterAddress(TransportReceiver *receiver,
                                 const dsnet::Configuration &config,
                                 const dsnet::ReplicaAddress *addr) = 0;
    virtual void ListenOnMulticast(TransportReceiver *receiver,
                                   const dsnet::Configuration &config) = 0;
    virtual bool SendBuffer(TransportReceiver *src,
                            const TransportAddress &dst,
                            const void *buf,
                            size_t len) = 0;
    virtual bool SendBufferToAll(TransportReceiver *src,
                                 const void *buf,
                                 size_t len) = 0;
    virtual bool SendMessage(TransportReceiver *src,
                             const TransportAddress &dst,
                             const Message &m) = 0;
    /* Send message to a replica in the local/default(0) group */
    virtual bool SendMessageToReplica(TransportReceiver *src,
                                      int replicaIdx,
                                      const Message &m) = 0;
    /* Send message to a replica in a specific group */
    virtual bool SendMessageToReplica(TransportReceiver *src,
                                      int groupIdx,
                                      int replicaIdx,
                                      const Message &m) = 0;
    /* Send message to all replicas in the local/default(0) group */
    virtual bool SendMessageToAll(TransportReceiver *src,
                                  const Message &m) = 0;
    /* Send message to all replicas in all groups in the configuration */
    virtual bool SendMessageToAllGroups(TransportReceiver *src,
                                        const Message &m) = 0;
    /* Send message to all replicas in specific groups */
    virtual bool SendMessageToGroups(TransportReceiver *src,
                                     const std::vector<int> &groups,
                                     const Message &m) = 0;
    /* Send message to all replicas in a single group */
    virtual bool SendMessageToGroup(TransportReceiver *src,
                                    int groupIdx,
                                    const Message &m) = 0;
    /* Send multi-group message using ordered multicast.
     * An implementation may decide not to implement this.
     */
    virtual bool OrderedMulticast(TransportReceiver *src,
                                  const std::vector<int> &groups,
                                  const Message &m) = 0;
    /* Send ordered multicast to the default group */
    virtual bool OrderedMulticast(TransportReceiver *src,
                                  const Message &m) = 0;
    /* Send message to failure coordinator
     */
    virtual bool SendMessageToFC(TransportReceiver *src,
                                 const Message &m) = 0;
    virtual int Timer(uint64_t ms, timer_callback_t cb) = 0;
    virtual bool CancelTimer(int id) = 0;
    virtual void CancelAllTimers() = 0;
    virtual void Run() = 0;
    virtual void Stop() = 0;
};

class Timeout
{
public:
    Timeout(Transport *transport, uint64_t ms, timer_callback_t cb);
    virtual ~Timeout();
    virtual void SetTimeout(uint64_t ms);
    virtual uint64_t Start();
    virtual uint64_t Reset();
    virtual void Stop();
    virtual bool Active() const;

private:
    Transport *transport;
    uint64_t ms;
    timer_callback_t cb;
    int timerId;
};

#endif  // _LIB_TRANSPORT_H_
