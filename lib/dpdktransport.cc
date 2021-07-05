#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "lib/dpdktransport.h"

namespace dsnet {

#define NUM_MBUFS 2048
#define MEMPOOL_CACHE_SIZE 256
#define RTE_RX_DESC 4096
#define RTE_TX_DESC 4096

DPDKTransportAddress::DPDKTransportAddress(const std::string &s)
{
    Parse(s);
}

DPDKTransportAddress::DPDKTransportAddress(const struct rte_ether_addr &ether_addr,
                                           rte_be32_t ip_addr,
                                           rte_be16_t udp_addr,
                                           uint16_t dev_port)
    : ether_addr_(ether_addr), ip_addr_(ip_addr), udp_addr_(udp_addr),
    dev_port_(dev_port) { }

DPDKTransportAddress *
DPDKTransportAddress::clone() const
{
    return new DPDKTransportAddress(*this);
}

std::string
DPDKTransportAddress::Serialize() const
{
    std::string s;
    s.append((const char *)&ether_addr_, sizeof(ether_addr_));
    s.append((const char *)&ip_addr_, sizeof(ip_addr_));
    s.append((const char *)&udp_addr_, sizeof(udp_addr_));
    s.append((const char *)&dev_port_, sizeof(dev_port_));
    return s;
}

void
DPDKTransportAddress::Parse(const std::string &s)
{
    const char *p = s.data();
    ether_addr_ = *(struct rte_ether_addr *)p;
    p += sizeof(ether_addr_);
    ip_addr_ = *(rte_be32_t *)p;
    p += sizeof(ip_addr_);
    udp_addr_ = *(rte_be16_t *)p;
    p += sizeof(udp_addr_);
    dev_port_ = *(uint16_t *)p;
}

bool
operator==(const DPDKTransportAddress &a, const DPDKTransportAddress &b)
{
    return (memcmp(&a.ether_addr_, &b.ether_addr_, sizeof(a.ether_addr_)) == 0 &&
            a.ip_addr_ == b.ip_addr_ &&
            a.udp_addr_ == b.udp_addr_ &&
            a.dev_port_ == b.dev_port_);
}

bool
operator<(const DPDKTransportAddress &a, const DPDKTransportAddress &b)
{
    return (memcmp(&a.ether_addr_, &b.ether_addr_, sizeof(a.ether_addr_)) < 0 ||
            a.ip_addr_ < b.ip_addr_ ||
            a.udp_addr_ < b.udp_addr_ ||
            a.dev_port_ < b.dev_port_);
}

static void
ConstructArguments(int argc, char **argv)
{
    argv[0] = new char[strlen("command")+1];
    strcpy(argv[0], "command");
    argv[1] = new char[strlen("-l")+1];
    strcpy(argv[1], "-l");
    argv[2] = new char[strlen("1")+1];
    strcpy(argv[2], "0");
    argv[3] = new char[strlen("--proc-type=auto")+1];
    strcpy(argv[3], "--proc-type=auto");
}

DPDKTransport::DPDKTransport(double drop_rate)
    : drop_rate_(drop_rate)
{
    // Initialize DPDK
    int argc = 4;
    char **argv = new char*[argc];
    ConstructArguments(argc, argv);

    if (rte_eal_init(argc, argv) < 0) {
        Panic("rte_eal_init failed");
    }

    if (rte_eth_dev_count_avail() == 0) {
        Panic("No available Ethernet ports");
    }

    char pool_name[32];
    sprintf(pool_name, "pktmbuf_pool");
    pktmbuf_pool_ = rte_pktmbuf_pool_create(pool_name,
                                            NUM_MBUFS,
                                            MEMPOOL_CACHE_SIZE,
                                            0,
                                            RTE_MBUF_DEFAULT_BUF_SIZE,
                                            rte_socket_id());

    if (pktmbuf_pool_ == nullptr) {
        Panic("rte_pktmbuf_pool_create failed");
    }

    for (int i = 0; i < argc; i++) {
        delete argv[i];
    }
    delete argv;
}

DPDKTransport::~DPDKTransport()
{
}

void
DPDKTransport::RegisterInternal(TransportReceiver *receiver,
                                const ReplicaAddress *addr,
                                int group_id, int replica_id)
{
    // Initialize port
    struct rte_eth_conf port_conf;
    memset(&port_conf, 0, sizeof(port_conf));
    port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;
    port_conf.rx_adv_conf.rss_conf.rss_key = nullptr;
    port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_NONFRAG_IPV4_UDP;

    struct rte_eth_dev_info dev_info;
    if (rte_eth_dev_info_get(addr->dev_port, &dev_info) != 0) {
        Panic("rte_eth_dev_info_get failed");
    }
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    int num_rx_queues = 1;
    int num_tx_queues = 1;
    if (rte_eth_dev_configure(addr->dev_port,
                              num_rx_queues,
                              num_tx_queues,
                              &port_conf) < 0) {
        Panic("rte_eth_dev_configure failed");
    }
    uint16_t nb_rxd = RTE_RX_DESC, nb_txd = RTE_TX_DESC;
    if (rte_eth_dev_adjust_nb_rx_tx_desc(addr->dev_port, &nb_rxd, &nb_txd) < 0) {
        Panic("rte_eth_dev_adjust_nb_rx_tx_desc failed");
    }

    // Initialize RX queues
    struct rte_eth_rxconf rxconf = dev_info.default_rxconf;
    rxconf.offloads = port_conf.rxmode.offloads;
    for (int i = 0; i < num_rx_queues; i++) {
        if (rte_eth_rx_queue_setup(addr->dev_port,
                                   i,
                                   nb_rxd,
                                   rte_eth_dev_socket_id(addr->dev_port),
                                   &rxconf,
                                   pktmbuf_pool_) < 0) {
            Panic("rte_eth_rx_queue_setup failed");
        }
    }

    // Initialize TX queues
    struct rte_eth_txconf txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    for (int i = 0; i < num_tx_queues; i++) {
        if (rte_eth_tx_queue_setup(addr->dev_port,
                                   i,
                                   nb_txd,
                                   rte_eth_dev_socket_id(addr->dev_port),
                                   &txconf) < 0) {
            Panic("rte_eth_tx_queue_setup failed");
        }
    }

    // Start device
    if (rte_eth_dev_start(addr->dev_port) < 0) {
        Panic("rte_eth_dev_start failed");
    }
    if (rte_eth_promiscuous_enable(addr->dev_port) != 0) {
        Panic("rte_eth_promiscuous_enable failed");
    }
}

void
DPDKTransport::ListenOnMulticast(TransportReceiver *receiver,
                                 const Configuration &config)
{
    // DPDK doesn't require special initialization for multicast
    return;
}

void
DPDKTransport::Run()
{
}

void
DPDKTransport::Stop()
{
}

int
DPDKTransport::Timer(uint64_t ms, timer_callback_t cb)
{
    return 0;
}

bool
DPDKTransport::CancelTimer(int id)
{
    return true;
}

void
DPDKTransport::CancelAllTimers()
{
}

bool
DPDKTransport::SendMessageInternal(TransportReceiver *src,
                                   const DPDKTransportAddress &dst,
                                   const Message &m)
{
    return true;
}

DPDKTransportAddress
DPDKTransport::LookupAddress(const ReplicaAddress &addr)
{
    return DPDKTransportAddress("");
}

} // namespace dsnet
