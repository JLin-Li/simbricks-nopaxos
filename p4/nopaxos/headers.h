#include <core.p4>
#include <tna.p4>

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;

typedef bit<16> sess_num_t;
typedef bit<8> seqid_t;
typedef bit<32> groupid_t;
typedef bit<64> msg_num_t;
typedef bit<32> reg_val_t;
typedef bit<8> reg_key_t;
typedef bit<64> timestamp_t;

#define ETHERTYPE_TPID    0x8100
#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_PKTGEN 16w0x7777
#define IP_PROTOCOL_UDP 17
#define IP_PROTOCOL_TCP 6
#define MCAST_DST_PORT 22222

#define IPV4_HOST_SIZE 1024

/*************************************************************************
 ***********************  H E A D E R S  *********************************
 *************************************************************************/

/*  Define all the headers the program will recognize             */
/*  The actual sets of headers processed by each gress can differ */

/* Standard ethernet header */
header ethernet_h {
    bit<48>   dst_addr;
    bit<48>   src_addr;
    bit<16>   ether_type;
}

header remaining_ethernet_h {
    bit<48> src_addr;
    bit<16> ether_type;
}

header vlan_tag_h {
    bit<3>   pcp;
    bit<1>   cfi;
    bit<12>  vid;
    bit<16>  ether_type;
}

header arp_h {
    bit<16> htype;
    bit<16> ptype;
    bit<8> hlen;
    bit<8> plen;
    bit<16> oper;
    bit<48> sender_hw_addr;
    bit<32> sender_ip_addr;
    bit<48> target_hw_addr;
    bit<32> target_ip_addr;
}

header ipv4_h {
    bit<4>   version;
    bit<4>   ihl;
    bit<8>   diffserv;
    bit<16>  total_len;
    bit<16>  identification;
    bit<3>   flags;
    bit<13>  frag_offset;
    bit<8>   ttl;
    bit<8>   protocol;
    bit<16>  hdr_checksum;
    bit<32>  src_addr;
    bit<32>  dst_addr;
}

header udp_h {
    bit<16>  src_port;
    bit<16>  dst_port;
    bit<16>  len;
    bit<16>  checksum;
}

header nopaxos_h {
    bit<32>         is_frag;
    bit<16>         header_size;
    // meta_data from here
    sess_num_t      sess_num;
    msg_num_t       msg_num;
}
