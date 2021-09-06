/* -*- P4_16 -*- */

#include "headers.h"


/******  G L O B A L   I N G R E S S   M E T A D A T A  *********/

struct my_ingress_headers_t {
    ethernet_h  ethernet;
    ipv4_h      ipv4;
    arp_h       arp;
    udp_h       udp;
    nopaxos_h   nopaxos;
}

struct my_ingress_metadata_t {
}

/***********************  P A R S E R  **************************/
parser IngressParser(
    packet_in pkt,
    out my_ingress_headers_t hdr,
    out my_ingress_metadata_t meta,
    out ingress_intrinsic_metadata_t ig_intr_md){

    /* This is a mandatory state, required by Tofino Architecture */
     state start {
        pkt.extract(ig_intr_md);
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type){
            ETHERTYPE_IPV4: parse_ipv4;
            ETHERTYPE_ARP: parse_arp;
            default: accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol){
            IP_PROTOCOL_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_arp {
        pkt.extract(hdr.arp);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dst_port) {
            MCAST_DST_PORT: parse_nopaxos;
            default: accept;
        }
    }

    state parse_nopaxos {
        pkt.extract(hdr.nopaxos);
        transition accept;
    }
}

/***************** M A T C H - A C T I O N  *********************/

control Ingress(
    /* User */
    inout my_ingress_headers_t                       hdr,
    inout my_ingress_metadata_t                      meta,
    /* Intrinsic */
    in    ingress_intrinsic_metadata_t               ig_intr_md,
    in    ingress_intrinsic_metadata_from_parser_t   ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t        ig_tm_md)
{
    Register<reg_val_t, reg_key_t>(1) reg_cnt; // value, key
    RegisterAction<reg_val_t, reg_key_t, reg_val_t>(reg_cnt)
    sequencing = {
        void apply(inout reg_val_t register_data, out reg_val_t seq_num) {
            register_data = register_data + 1;
            seq_num = register_data;
        }
    };

    action send(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;
    }

    action drop() {
        ig_dprsr_md.drop_ctl = 1;
    }

    action multicast(MulticastGroupId_t mcast_grp) {
        ig_tm_md.mcast_grp_a = mcast_grp;
    }

    table ipv4_host {
        key = { hdr.ipv4.dst_addr : exact; }
        actions = {
            send; drop; multicast;
            @defaultonly NoAction;
        }
        const default_action = NoAction();
        size = IPV4_HOST_SIZE;
    }

    apply {
        hdr.udp.checksum = 0;
        ig_tm_md.bypass_egress = 1w1;

        if(hdr.ethernet.ether_type == ETHERTYPE_ARP){
            // do the broadcast to all involved ports
            ig_tm_md.mcast_grp_a = 1;
        }
        else if(hdr.udp.dst_port == MCAST_DST_PORT){
            hdr.nopaxos.msg_num = (msg_num_t)sequencing.execute(0);
            hdr.nopaxos.sess_num = (sess_num_t)0;
            ig_tm_md.mcast_grp_a = 2;
        }
        else if(hdr.ipv4.isValid() && hdr.udp.isValid()) { // for non-multicast packet
            if (!ipv4_host.apply().hit) {
                ig_dprsr_md.drop_ctl = 1;
            }
        }
    }

}  // End of SwitchIngressControl

/*********************  D E P A R S E R  ************************/

control IngressDeparser(packet_out pkt,
    /* User */
    inout my_ingress_headers_t                       hdr,
    in    my_ingress_metadata_t                      meta,
    /* Intrinsic */
    in    ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md)
{
    apply {
        pkt.emit(hdr);
    }
}


/*************************************************************************
 ****************  E G R E S S   P R O C E S S I N G   *******************
 *************************************************************************/

/***********************  H E A D E R S  ************************/

struct my_egress_headers_t {
    ethernet_h   ethernet;
    vlan_tag_h   vlan_tag;
    ipv4_h       ipv4;
}

/********  G L O B A L   E G R E S S   M E T A D A T A  *********/

struct my_egress_metadata_t {
}

/***********************  P A R S E R  **************************/

parser EgressParser(packet_in        pkt,
    /* User */
    out my_egress_headers_t          hdr,
    out my_egress_metadata_t         meta,
    /* Intrinsic */
    out egress_intrinsic_metadata_t  eg_intr_md)
{
    /* This is a mandatory state, required by Tofino Architecture */
    state start {
        pkt.extract(eg_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_TPID:  parse_vlan_tag;
            ETHERTYPE_IPV4:  parse_ipv4;
            default: accept;
        }
    }

    state parse_vlan_tag {
        pkt.extract(hdr.vlan_tag);
        transition select(hdr.vlan_tag.ether_type) {
            ETHERTYPE_IPV4:  parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition accept;
    }
}

/***************** M A T C H - A C T I O N  *********************/

control Egress(
    /* User */
    inout my_egress_headers_t                          hdr,
    inout my_egress_metadata_t                         meta,
    /* Intrinsic */
    in    egress_intrinsic_metadata_t                  eg_intr_md,
    in    egress_intrinsic_metadata_from_parser_t      eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t     eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t  eg_oport_md)
{
    apply {
    }
}

    /*********************  D E P A R S E R  ************************/

control EgressDeparser(packet_out pkt,
    /* User */
    inout my_egress_headers_t                       hdr,
    in    my_egress_metadata_t                      meta,
    /* Intrinsic */
    in    egress_intrinsic_metadata_for_deparser_t  eg_dprsr_md)
{
    Checksum() ipv4_checksum;
    apply {
        if (hdr.ipv4.isValid()) {
            hdr.ipv4.hdr_checksum = ipv4_checksum.update({
                hdr.ipv4.version,
                hdr.ipv4.ihl,
                hdr.ipv4.diffserv,
                hdr.ipv4.total_len,
                hdr.ipv4.identification,
                hdr.ipv4.flags,
                hdr.ipv4.frag_offset,
                hdr.ipv4.ttl,
                hdr.ipv4.protocol,
                hdr.ipv4.src_addr,
                hdr.ipv4.dst_addr});
        }
        pkt.emit(hdr);
    }
}


/************ F I N A L   P A C K A G E ******************************/
Pipeline(IngressParser(),
         Ingress(),
         IngressDeparser(),
         EgressParser(),
         Egress(),
         EgressDeparser()
         ) pipe;

Switch(pipe) main;
