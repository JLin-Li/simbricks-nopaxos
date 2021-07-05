#pragma once

#include <rte_ether.h>
#include <rte_byteorder.h>

#include "lib/configuration.h"
#include "lib/transport.h"
#include "lib/transportcommon.h"

namespace dsnet {

class DPDKTransportAddress : public TransportAddress
{
public:
    DPDKTransportAddress(const std::string &s);
    virtual DPDKTransportAddress * clone() const override;
    virtual std::string Serialize() const override;
    virtual void Parse(const std::string &s) override;

private:
    struct rte_ether_addr ether_addr_;
    rte_be32_t ip_addr_;
    rte_be16_t udp_addr_;
    uint16_t dev_port_;

    DPDKTransportAddress(const struct rte_ether_addr &ether_addr,
                         rte_be32_t ip_addr,
                         rte_be16_t udp_addr,
                         uint16_t dev_port);
    friend bool operator==(const DPDKTransportAddress &a,
                           const DPDKTransportAddress &b);
    friend bool operator<(const DPDKTransportAddress &a,
                          const DPDKTransportAddress &b);
};

class DPDKTransport : public TransportCommon<DPDKTransportAddress>
{
public:
    DPDKTransport(double drop_rate = 0.0);
    virtual ~DPDKTransport();
    virtual void RegisterInternal(TransportReceiver *receiver,
                                  const ReplicaAddress *addr,
                                  int group_id, int replica_id) override;
    virtual void ListenOnMulticast(TransportReceiver *receiver,
                                   const Configuration &config) override;
    virtual void Run() override;
    virtual void Stop() override;
    virtual int Timer(uint64_t ms, timer_callback_t cb) override;
    virtual bool CancelTimer(int id) override;
    virtual void CancelAllTimers() override;

private:
    double drop_rate_;
    struct rte_mempool *pktmbuf_pool_;

    virtual bool SendMessageInternal(TransportReceiver *src,
                                     const DPDKTransportAddress &dst,
                                     const Message &m) override;
    virtual DPDKTransportAddress
    LookupAddress(const ReplicaAddress &addr) override;
};

} // namespace dsnet
