/* Copyright (c) 2011, TrafficLab, Ericsson Research, Hungary
 * Copyright (c) 2012, CPqD, Brazil
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Ericsson Research nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <bofuss/dpctl.h>
#include <bofuss/ipv6-util.h>
#include <bofuss/ofl-actions.h>
#include <bofuss/ofl-messages.h>
#include <bofuss/openflow.h>
#include <bofuss/util.h>
#include <bofuss/vlog.h>

#define LOG_MODULE VLM_dpctl

#if defined(__GNUC__)
// Define dpctl_send_and_print and dpctl_transact_and_print functions as weak,
// so ns3 can override it and send the dpctl message over simulated channel.
#pragma weak dpctl_send_and_print
#pragma weak dpctl_transact_and_print
#endif

#define FLOW_MOD_COMMAND "cmd"
#define FLOW_MOD_COOKIE "cookie"
#define FLOW_MOD_COOKIE_MASK "cookie_mask"
#define FLOW_MOD_TABLE_ID "table"
#define FLOW_MOD_IDLE "idle"
#define FLOW_MOD_HARD "hard"
#define FLOW_MOD_PRIO "prio"
#define FLOW_MOD_BUFFER "buffer"
#define FLOW_MOD_OUT_PORT "out_port"
#define FLOW_MOD_OUT_GROUP "out_group"
#define FLOW_MOD_FLAGS "flags"
#define FLOW_MOD_MATCH "match"

#define MATCH_IN_PORT "in_port"
#define MATCH_DL_SRC "eth_src"
#define MATCH_DL_SRC_MASK "eth_src_mask"
#define MATCH_DL_DST "eth_dst"
#define MATCH_DL_DST_MASK "eth_dst_mask"
#define MATCH_DL_VLAN "vlan_vid"
#define MATCH_IP_DSCP "ip_dscp"
#define MATCH_IP_ECN "ip_ecn"
#define MATCH_DL_VLAN_PCP "vlan_pcp"
#define MATCH_DL_TYPE "eth_type"
#define MATCH_NW_PROTO "ip_proto"
#define MATCH_NW_SRC "ip_src"
#define MATCH_NW_SRC_MASK "nw_src_mask"
#define MATCH_NW_DST "ip_dst"
#define MATCH_NW_DST_MASK "ipv4_dst_mask"
#define MATCH_TP_SRC "tcp_src"
#define MATCH_TP_DST "tcp_dst"
#define MATCH_UDP_SRC "udp_src"
#define MATCH_UDP_DST "udp_dst"
#define MATCH_SCTP_SRC "sctp_src"
#define MATCH_SCTP_DST "sctp_dst"
#define MATCH_ICMPV4_CODE "icmp_code"
#define MATCH_ICMPV4_TYPE "icmp_type"
#define MATCH_ARP_OP "arp_op"
#define MATCH_ARP_SHA "arp_sha"
#define MATCH_ARP_THA "arp_tha"
#define MATCH_ARP_SPA "arp_spa"
#define MATCH_ARP_TPA "arp_tpa"
#define MATCH_NW_SRC_IPV6 "ipv6_src"
#define MATCH_NW_DST_IPV6 "ipv6_dst"
#define MATCH_ICMPV6_CODE "icmpv6_code"
#define MATCH_ICMPV6_TYPE "icmpv6_type"
#define MATCH_IPV6_FLABEL "ipv6_flabel"
#define MATCH_IPV6_ND_TARGET "ipv6_nd_target"
#define MATCH_IPV6_ND_SLL "ipv6_nd_sll"
#define MATCH_IPV6_ND_TLL "ipv6_nd_tll"
#define MATCH_MPLS_LABEL "mpls_label"
#define MATCH_MPLS_TC "mpls_tc"
#define MATCH_MPLS_BOS "mpls_bos"
#define MATCH_METADATA "meta"
#define MATCH_METADATA_MASK "meta_mask"
#define MATCH_PBB_ISID "pbb_isid"
#define MATCH_TUNNEL_ID "tunn_id"
#define MATCH_EXT_HDR "ext_hdr"

#define GROUP_MOD_COMMAND "cmd"
#define GROUP_MOD_TYPE "type"
#define GROUP_MOD_GROUP "group"

#define BUCKET_WEIGHT "weight"
#define BUCKET_WATCH_PORT "port"
#define BUCKET_WATCH_GROUP "group"

#define METER_MOD_COMMAND "cmd"
#define METER_MOD_FLAGS "flags"
#define METER_MOD_METER "meter"

#define BAND_RATE "rate"
#define BAND_BURST_SIZE "burst"
#define BAND_PREC_LEVEL "prec_level"

#define CONFIG_FLAGS "flags"
#define CONFIG_MISS "miss"

#define PORT_MOD_PORT "port"
#define PORT_MOD_HW_ADDR "addr"
#define PORT_MOD_HW_CONFIG "conf"
#define PORT_MOD_MASK "mask"
#define PORT_MOD_ADVERTISE "adv"

#define TABLE_MOD_TABLE "table"
#define TABLE_MOD_CONFIG "conf"

#define KEY_VAL_EQU "="
#define KEY_VAL_COL ":"
#define KEY_SEP ","
#define MASK_SEP "/"

#define ADD "+"

#define NUM_ELEMS(x) (sizeof(x) / sizeof(x[0]))

struct names8
{
    uint8_t code;
    const char *name;
};

static struct names8 group_type_names[] = {
    {OFPGT_ALL, "all"},
    {OFPGT_SELECT, "sel"},
    {OFPGT_INDIRECT, "ind"},
    {OFPGT_FF, "ff"}};

static struct names8 table_names[] = {
    {OFPTT_ALL, "all"}};

static struct names8 flow_mod_cmd_names[] = {
    {OFPFC_ADD, "add"},
    {OFPFC_MODIFY, "mod"},
    {OFPFC_MODIFY_STRICT, "mods"},
    {OFPFC_DELETE, "del"},
    {OFPFC_DELETE_STRICT, "dels"}};

struct names16
{
    uint16_t code;
    const char *name;
};

static struct names16 ext_header_names[] = {
    {OFPIEH_NONEXT, "no_next"},
    {OFPIEH_ESP, "esp"},
    {OFPIEH_AUTH, "auth"},
    {OFPIEH_DEST, "dest"},
    {OFPIEH_FRAG, "frag"},
    {OFPIEH_ROUTER, "router"},
    {OFPIEH_HOP, "hop"},
    {OFPIEH_UNREP, "unrep"},
    {OFPIEH_UNSEQ, "unseq"}};

static struct names16 group_mod_cmd_names[] = {
    {OFPGC_ADD, "add"},
    {OFPGC_MODIFY, "mod"},
    {OFPGC_DELETE, "del"}};

static struct names16 meter_mod_cmd_names[] = {
    {OFPMC_ADD, "add"},
    {OFPMC_MODIFY, "mod"},
    {OFPMC_DELETE, "del"}};

static struct names16 inst_names[] = {
    {OFPIT_GOTO_TABLE, "goto"},
    {OFPIT_WRITE_METADATA, "meta"},
    {OFPIT_WRITE_ACTIONS, "write"},
    {OFPIT_APPLY_ACTIONS, "apply"},
    {OFPIT_CLEAR_ACTIONS, "clear"},
    {OFPIT_METER, "meter"}};

static struct names16 vlan_vid_names[] = {
    {OFPVID_PRESENT, "any"},
    {OFPVID_NONE, "none"}};

static struct names16 action_names[] = {
    {OFPAT_OUTPUT, "output"},
    {OFPAT_COPY_TTL_OUT, "ttl_out"},
    {OFPAT_COPY_TTL_IN, "ttl_in"},
    {OFPAT_SET_MPLS_TTL, "mpls_ttl"},
    {OFPAT_DEC_MPLS_TTL, "mpls_dec"},
    {OFPAT_PUSH_VLAN, "push_vlan"},
    {OFPAT_POP_VLAN, "pop_vlan"},
    {OFPAT_PUSH_PBB, "push_pbb"},
    {OFPAT_POP_PBB, "pop_pbb"},
    {OFPAT_PUSH_MPLS, "push_mpls"},
    {OFPAT_POP_MPLS, "pop_mpls"},
    {OFPAT_SET_QUEUE, "queue"},
    {OFPAT_GROUP, "group"},
    {OFPAT_SET_NW_TTL, "nw_ttl"},
    {OFPAT_DEC_NW_TTL, "nw_dec"},
    {OFPAT_SET_FIELD, "set_field"}};

static struct names16 band_names[] = {
    {OFPMBT_DROP, "drop"},
    {OFPMBT_DSCP_REMARK, "dscp_remark"}};

struct names32
{
    uint32_t code;
    const char *name;
};

static struct names32 port_names[] = {
    {OFPP_IN_PORT, "in_port"},
    {OFPP_TABLE, "table"},
    {OFPP_NORMAL, "normal"},
    {OFPP_FLOOD, "flood"},
    {OFPP_ALL, "all"},
    {OFPP_CONTROLLER, "ctrl"},
    {OFPP_LOCAL, "local"},
    {OFPP_ANY, "any"}};

static struct names32 queue_names[] = {
    {OFPQ_ALL, "all"}};

static struct names32 group_names[] = {
    {OFPG_ALL, "all"},
    {OFPG_ANY, "any"}};

static struct names32 buffer_names[] = {
    {OFP_NO_BUFFER, "none"}};

struct oxm_str_mapping
{
    char *name;
    uint32_t oxm_id;
};

struct oxm_str_mapping oxm_str_map[] = {
    {MATCH_IN_PORT, OXM_OF_IN_PORT},
    {MATCH_DL_SRC, OXM_OF_ETH_SRC},
    {MATCH_DL_DST, OXM_OF_ETH_DST},
    {MATCH_ARP_SHA, OXM_OF_ARP_SHA},
    {MATCH_ARP_THA, OXM_OF_ARP_THA},
    {MATCH_ARP_SPA, OXM_OF_ARP_SPA},
    {MATCH_ARP_TPA, OXM_OF_ARP_TPA},
    {MATCH_ARP_OP, OXM_OF_ARP_OP},
    {MATCH_DL_VLAN, OXM_OF_VLAN_VID},
    {MATCH_DL_VLAN_PCP, OXM_OF_VLAN_PCP},
    {MATCH_DL_TYPE, OXM_OF_ETH_TYPE},
    {MATCH_IP_ECN, OXM_OF_IP_ECN},
    {MATCH_IP_DSCP, OXM_OF_IP_DSCP},
    {MATCH_NW_PROTO, OXM_OF_IP_PROTO},
    {MATCH_NW_SRC, OXM_OF_IPV4_SRC},
    {MATCH_NW_DST, OXM_OF_IPV4_DST},
    {MATCH_ICMPV4_CODE, OXM_OF_ICMPV4_CODE},
    {MATCH_ICMPV4_TYPE, OXM_OF_ICMPV4_TYPE},
    {MATCH_TP_SRC, OXM_OF_TCP_SRC},
    {MATCH_TP_DST, OXM_OF_TCP_DST},
    {MATCH_UDP_SRC, OXM_OF_UDP_SRC},
    {MATCH_UDP_DST, OXM_OF_UDP_DST},
    {MATCH_SCTP_SRC, OXM_OF_SCTP_SRC},
    {MATCH_SCTP_DST, OXM_OF_SCTP_DST},
    {MATCH_MPLS_LABEL, OXM_OF_MPLS_LABEL},
    {MATCH_MPLS_TC, OXM_OF_MPLS_TC},
    {MATCH_MPLS_BOS, OXM_OF_MPLS_BOS},
    {MATCH_NW_SRC_IPV6, OXM_OF_IPV6_SRC},
    {MATCH_NW_DST_IPV6, OXM_OF_IPV6_DST},
    {MATCH_IPV6_FLABEL, OXM_OF_IPV6_FLABEL},
    {MATCH_ICMPV6_CODE, OXM_OF_ICMPV6_CODE},
    {MATCH_ICMPV6_TYPE, OXM_OF_ICMPV6_TYPE},
    {MATCH_IPV6_ND_TARGET, OXM_OF_IPV6_ND_TARGET},
    {MATCH_IPV6_ND_SLL, OXM_OF_IPV6_ND_SLL},
    {MATCH_IPV6_ND_TLL, OXM_OF_IPV6_ND_TLL},
    {MATCH_METADATA, OXM_OF_METADATA},
    {MATCH_PBB_ISID, OXM_OF_PBB_ISID},
    {MATCH_TUNNEL_ID, OXM_OF_TUNNEL_ID},
    {MATCH_EXT_HDR, OXM_OF_IPV6_EXTHDR},
};

void dpctl_transact_and_print(struct vconn *vconn UNUSED, struct ofl_msg_header *req UNUSED,
                              struct ofl_msg_header **repl UNUSED)
{
    NOT_IMPLEMENTED();
}

void dpctl_send_and_print(struct vconn *vconn UNUSED, struct ofl_msg_header *msg UNUSED)
{
    NOT_IMPLEMENTED();
}

static void
parse_flow_mod_args(char *str, struct ofl_msg_flow_mod *req);

static void
parse_group_mod_args(char *str, struct ofl_msg_group_mod *req);

static void
parse_meter_mod_args(char *str, struct ofl_msg_meter_mod *req);

static void
parse_bucket(char *str, struct ofl_bucket *b);

static void
parse_flow_stat_args(char *str, struct ofl_msg_multipart_request_flow *req);

static void
parse_match(char *str, struct ofl_match_header **match);

static void
parse_inst(char *str, struct ofl_instruction_header **inst);

static void
parse_actions(char *str, size_t *acts_num, struct ofl_action_header ***acts);

static void
parse_config(char *str, struct ofl_config *config);

static void
parse_port_mod(char *str, struct ofl_msg_port_mod *msg);

static void
parse_table_mod(char *str, struct ofl_msg_table_mod *msg);

static void
parse_band(char *str, struct ofl_msg_meter_mod *m, struct ofl_meter_band_header **b);

static void
make_all_match(struct ofl_match_header **match);

static int
parse_port(char *str, uint32_t *port);

static int
parse_queue(char *str, uint32_t *port);

static int
parse_group(char *str, uint32_t *group);

static int
parse_meter(char *str, uint32_t *meter);

static int
parse_table(char *str, uint8_t *table);

static int
parse_dl_addr(char *str, uint8_t *addr, uint8_t **mask);

static int
parse_nw_addr(char *str, uint32_t *addr, uint32_t **mask);

static int
parse_vlan_vid(char *str, uint16_t *vid);

static int
parse_ext_hdr(char *str, uint16_t *ext_hdr);

static int
parse8(char *str, struct names8 *names, size_t names_num, uint8_t max, uint8_t *val);

static int
parse16(char *str, struct names16 *names, size_t names_num, uint16_t max, uint16_t *val);

static int
parse16m(char *str, struct names16 *names, size_t names_num, uint16_t max, uint16_t *val, uint16_t **mask) UNUSED;

static int
parse32(char *str, struct names32 *names, size_t names_num, uint32_t max, uint32_t *val);

static int
parse32m(char *str, struct names32 *names, size_t names_num, uint32_t max, uint32_t *val, uint32_t **mask);

static int
parse64(char *str, uint64_t max, uint64_t *val);

static int
parse64m(char *str, uint64_t max, uint64_t *val, uint64_t **mask);

static void
table_features(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_table_features *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_table_features));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_TABLE_FEATURES;
    msg->header.flags = 0x0000;
    msg->tables_num = 0;
    msg->table_features = NULL;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
features(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_header));
    msg->type = OFPT_FEATURES_REQUEST;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
get_config(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_header));
    msg->type = OFPT_GET_CONFIG_REQUEST;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_desc(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_header));
    msg->header.type = OFPT_MULTIPART_REQUEST;
    msg->type = OFPMP_DESC;
    msg->flags = 0x0000;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
port_desc(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_header));
    msg->header.type = OFPT_MULTIPART_REQUEST;
    msg->type = OFPMP_PORT_DESC;
    msg->flags = 0x0000;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_flow(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_flow *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_flow));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_FLOW;
    msg->header.flags = 0x0000;
    msg->cookie = 0x0000000000000000ULL;
    msg->cookie_mask = 0x0000000000000000ULL;
    msg->table_id = 0xff;
    msg->out_port = OFPP_ANY;
    msg->out_group = OFPG_ANY;
    msg->match = NULL;

    if (argc > 0)
    {
        parse_flow_stat_args(argv[0], msg);
    }

    if (argc > 1)
    {
        parse_match(argv[1], &(msg->match));
    }
    else
    {
        make_all_match(&(msg->match));
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_aggr(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_flow *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_flow));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_AGGREGATE;
    msg->header.flags = 0x0000;
    msg->cookie = 0x0000000000000000ULL;
    msg->cookie_mask = 0x0000000000000000ULL;
    msg->table_id = 0xff;
    msg->out_port = OFPP_ANY;
    msg->out_group = OFPG_ANY;
    msg->match = NULL;

    if (argc > 0)
    {
        parse_flow_stat_args(argv[0], msg);
    }

    if (argc > 1)
    {
        parse_match(argv[1], &(msg->match));
    }
    else
    {
        make_all_match(&(msg->match));
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_table(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_header));
    msg->header.type = OFPT_MULTIPART_REQUEST;
    msg->type = OFPMP_TABLE;
    msg->flags = 0x0000;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_port(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_port *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_port));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_PORT_STATS;
    msg->header.flags = 0x0000;
    msg->port_no = OFPP_ANY;

    if (argc > 0 && parse_port(argv[0], &(msg->port_no)))
    {
        ofp_fatal(0, "Error parsing port: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_queue(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_queue *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_queue));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_QUEUE;
    msg->header.flags = 0x0000;
    msg->port_no = OFPP_ANY;
    msg->queue_id = OFPQ_ALL;

    if (argc > 0 && parse_port(argv[0], &(msg->port_no)))
    {
        ofp_fatal(0, "Error parsing port: %s.", argv[0]);
    }
    if (argc > 1 && parse_queue(argv[1], &(msg->queue_id)))
    {
        ofp_fatal(0, "Error parsing queue: %s.", argv[1]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_group(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_group *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_group));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_GROUP;
    msg->header.flags = 0x0000;
    msg->group_id = OFPG_ALL;

    if (argc > 0 && parse_group(argv[0], &(msg->group_id)))
    {
        ofp_fatal(0, "Error parsing group: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_group_desc(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_multipart_request_group *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_group));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_GROUP_DESC;
    msg->header.flags = 0x0000;
    msg->group_id = OFPG_ALL;

    if (argc > 0 && parse_group(argv[0], &(msg->group_id)))
    {
        ofp_fatal(0, "Error parsing group: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
set_config(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    struct ofl_msg_set_config *msg;
    msg = xmalloc(sizeof(struct ofl_msg_set_config));
    msg->header.type = OFPT_SET_CONFIG;
    msg->config = xmalloc(sizeof(struct ofl_config));
    msg->config->flags = OFPC_FRAG_NORMAL;
    msg->config->miss_send_len = OFP_DEFAULT_MISS_SEND_LEN;

    parse_config(argv[0], msg->config);

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
flow_mod(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_flow_mod *msg;
    msg = xmalloc(sizeof(struct ofl_msg_flow_mod));
    msg->header.type = OFPT_FLOW_MOD;
    msg->cookie = 0x0000000000000000ULL;
    msg->cookie_mask = 0x0000000000000000ULL;
    msg->table_id = 0xff;
    msg->command = OFPFC_ADD;
    msg->idle_timeout = OFP_FLOW_PERMANENT;
    msg->hard_timeout = OFP_FLOW_PERMANENT;
    msg->priority = OFP_DEFAULT_PRIORITY;
    msg->buffer_id = 0xffffffff;
    msg->out_port = OFPP_ANY;
    msg->out_group = OFPG_ANY;
    msg->flags = 0x0000;
    msg->match = NULL;
    msg->instructions_num = 0;
    msg->instructions = NULL;

    parse_flow_mod_args(argv[0], msg);

    if (argc > 1)
    {
        size_t i, j;
        size_t inst_num = 0;
        if (argc > 2)
        {
            inst_num = argc - 2;
            j = 2;
            parse_match(argv[1], &(msg->match));
        }
        else
        {
            if (msg->command == OFPFC_DELETE)
            {
                inst_num = 0;
                parse_match(argv[1], &(msg->match));
            }
            else
            {
                /* We copy the value because we don't know if it is an
                instruction or match. If the match is empty, the argv is
                modified causing errors to instructions parsing */
                char *cpy = malloc(strlen(argv[1]) + 1);
                memcpy(cpy, argv[1], strlen(argv[1]) + 1);
                parse_match(cpy, &(msg->match));
                free(cpy);
                if (msg->match->length <= 4)
                {
                    inst_num = argc - 1;
                    j = 1;
                }
            }
        }

        msg->instructions_num = inst_num;
        msg->instructions = xmalloc(sizeof(struct ofl_instruction_header *) * inst_num);
        for (i = 0; i < inst_num; i++)
        {
            parse_inst(argv[j + i], &(msg->instructions[i]));
        }
    }
    else
    {
        make_all_match(&(msg->match));
    }

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
group_mod(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_group_mod *msg;
    msg = xmalloc(sizeof(struct ofl_msg_group_mod));
    msg->header.type = OFPT_GROUP_MOD;
    msg->command = OFPGC_ADD;
    msg->type = OFPGT_ALL;
    msg->group_id = OFPG_ALL;
    msg->buckets_num = 0;
    msg->buckets = NULL;

    parse_group_mod_args(argv[0], msg);

    if (argc > 1)
    {
        size_t i;
        size_t buckets_num = (argc - 1) / 2;

        msg->buckets_num = buckets_num;
        msg->buckets = xmalloc(sizeof(struct ofl_bucket *) * buckets_num);

        for (i = 0; i < buckets_num; i++)
        {
            msg->buckets[i] = xmalloc(sizeof(struct ofl_bucket));
            msg->buckets[i]->weight = 0;
            msg->buckets[i]->watch_port = OFPP_ANY;
            msg->buckets[i]->watch_group = OFPG_ANY;
            msg->buckets[i]->actions_num = 0;
            msg->buckets[i]->actions = NULL;

            parse_bucket(argv[i * 2 + 1], msg->buckets[i]);
            parse_actions(argv[i * 2 + 2], &(msg->buckets[i]->actions_num), &(msg->buckets[i]->actions));
        }
    }

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
group_features(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_header));
    msg->header.type = OFPT_MULTIPART_REQUEST;
    msg->type = OFPMP_GROUP_FEATURES;
    msg->flags = 0x0000;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
meter_mod(struct vconn *vconn, int argc, char *argv[])
{
    struct ofl_msg_meter_mod *msg;
    msg = xmalloc(sizeof(struct ofl_msg_meter_mod));
    msg->header.type = OFPT_METER_MOD;
    msg->command = OFPMC_ADD;
    msg->flags = OFPMF_KBPS;
    msg->meter_id = 0;
    msg->meter_bands_num = 0;
    msg->bands = NULL;

    parse_meter_mod_args(argv[0], msg);

    if (argc > 1)
    {
        size_t i;
        size_t bands_num = argc - 1;
        msg->meter_bands_num = bands_num;
        msg->bands = xmalloc(sizeof(struct ofl_meter_band_header *) * bands_num);
        for (i = 0; i < bands_num; i++)
        {
            parse_band(argv[i + 1], msg, &(msg->bands[i]));
        }
    }

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
stats_meter(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    struct ofl_msg_multipart_meter_request *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_meter_request));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_METER;
    msg->header.flags = 0x0000;
    msg->meter_id = OFPM_ALL;

    if (argc > 0 && parse_meter(argv[0], &(msg->meter_id)))
    {
        ofp_fatal(0, "Error parsing meter: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
meter_config(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    struct ofl_msg_multipart_meter_request *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_meter_request));
    msg->header.header.type = OFPT_MULTIPART_REQUEST;
    msg->header.type = OFPMP_METER_CONFIG;
    msg->header.flags = 0x0000;
    msg->meter_id = OFPM_ALL;

    if (argc > 0 && parse_meter(argv[0], &(msg->meter_id)))
    {
        ofp_fatal(0, "Error parsing meter: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
meter_features(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_multipart_request_header *msg;
    msg = xmalloc(sizeof(struct ofl_msg_multipart_request_header));
    msg->header.type = OFPT_MULTIPART_REQUEST;
    msg->type = OFPMP_METER_FEATURES;
    msg->flags = 0x0000;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
port_mod(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    static uint8_t mask_all[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    struct ofl_msg_port_mod *msg;
    msg = xmalloc(sizeof(struct ofl_msg_port_mod));
    msg->header.type = OFPT_PORT_MOD;
    msg->port_no = OFPP_ANY;
    msg->config = 0x00000000;
    msg->mask = 0x00000000;
    msg->advertise = 0x00000000;

    memcpy(msg->hw_addr, mask_all, OFP_ETH_ALEN);
    parse_port_mod(argv[0], msg);

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
table_mod(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    struct ofl_msg_table_mod *msg;
    msg = xmalloc(sizeof(struct ofl_msg_table_mod));
    msg->header.type = OFPT_TABLE_MOD;
    msg->table_id = 0xff;
    msg->config = 0x00;

    parse_table_mod(argv[0], msg);

    dpctl_send_and_print(vconn, (struct ofl_msg_header *)msg);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
queue_get_config(struct vconn *vconn, int argc UNUSED, char *argv[])
{
    struct ofl_msg_queue_get_config_request *msg;
    msg = xmalloc(sizeof(struct ofl_msg_queue_get_config_request));
    msg->header.type = OFPT_QUEUE_GET_CONFIG_REQUEST;
    msg->port = OFPP_ALL;

    if (parse_port(argv[0], &(msg->port)))
    {
        ofp_fatal(0, "Error parsing queue_get_config port: %s.", argv[0]);
    }

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

static void
get_async(struct vconn *vconn, int argc UNUSED, char *argv[] UNUSED)
{
    struct ofl_msg_async_config *msg;
    msg = xmalloc(sizeof(struct ofl_msg_async_config));
    msg->header.type = OFPT_GET_ASYNC_REQUEST;
    msg->config = NULL;

    dpctl_transact_and_print(vconn, (struct ofl_msg_header *)msg, NULL);
    ofl_msg_free((struct ofl_msg_header *)msg, 0);
}

struct command
{
    char *name;
    int min_args;
    int max_args;
    void (*handler)(struct vconn *vconn, int argc, char *argv[]);
};

static struct command all_commands[] = {
    {"features", 0, 0, features},
    {"get-config", 0, 0, get_config},
    {"table-features", 0, 0, table_features},
    {"group-features", 0, 0, group_features},
    {"meter-features", 0, 0, meter_features},
    {"stats-desc", 0, 0, stats_desc},
    {"stats-flow", 0, 2, stats_flow},
    {"stats-aggr", 0, 2, stats_aggr},
    {"stats-table", 0, 0, stats_table},
    {"stats-port", 0, 1, stats_port},
    {"stats-queue", 0, 2, stats_queue},
    {"stats-group", 0, 1, stats_group},
    {"stats-group-desc", 0, 1, stats_group_desc},
    {"stats-meter", 0, 1, stats_meter},
    {"meter-config", 0, 1, meter_config},
    {"port-desc", 0, 0, port_desc},
    {"set-config", 1, 1, set_config},
    {"flow-mod", 1, 8, flow_mod},
    {"group-mod", 1, 99, group_mod},
    {"meter-mod", 1, 99, meter_mod},
    {"get-async", 0, 0, get_async},
    {"port-mod", 1, 1, port_mod},
    {"table-mod", 1, 1, table_mod},
    {"queue-get-config", 1, 1, queue_get_config}};

int dpctl_exec_ns3_command(void *ctrl, int argc, char *argv[])
{
    struct command *p;
    size_t i;

    // We are using struct vconn pointer to carry ns3 metadata. This
    // information will be send back to simulator in dpctl_transact_and_print
    // or dpctl_send_and_print functions, so it can find the controller object
    // to invoke proper fuction.

    if (argc < 1)
        ofp_fatal(0, "missing COMMAND; use dpctl --help for help");

    for (i = 0; i < NUM_ELEMS(all_commands); i++)
    {
        p = &all_commands[i];
        if (strcmp(p->name, argv[0]) == 0)
        {
            argc -= 1;
            argv += 1;
            if (argc < p->min_args)
                ofp_fatal(0, "'%s' command requires at least %d arguments",
                          p->name, p->min_args);
            else if (argc > p->max_args)
                ofp_fatal(0, "'%s' command takes at most %d arguments",
                          p->name, p->max_args);
            else
            {
                p->handler((struct vconn *)ctrl, argc, argv);
                if (ferror(stdout))
                {
                    ofp_fatal(0, "write to stdout failed");
                }
                if (ferror(stderr))
                {
                    ofp_fatal(0, "write to stderr failed");
                }
                return 0;
            }
        }
    }
    ofp_fatal(0, "unknown command '%s'", argv[0]);
    return 0;
}

static void
parse_match(char *str, struct ofl_match_header **match)
{
    // TODO parse masks
    char *token, *saveptr = NULL;
    struct ofl_match *m = xmalloc(sizeof(struct ofl_match));
    ofl_structs_match_init(m);

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, "apply", strlen("apply")) == 0 ||
            strncmp(token, "write", strlen("write")) == 0 ||
            strncmp(token, "goto", strlen("goto")) == 0)
        {
            break;
        }
        /* In_port */
        if (strncmp(token, MATCH_IN_PORT KEY_VAL_EQU, strlen(MATCH_IN_PORT KEY_VAL_EQU)) == 0)
        {
            uint32_t in_port;
            if (parse_port(token + strlen(MATCH_IN_PORT KEY_VAL_EQU), &in_port))
            {
                ofp_fatal(0, "Error parsing port: %s.", token);
            }
            else
                ofl_structs_match_put32(m, OXM_OF_IN_PORT, in_port);
            continue;
        }

        /* Ethernet Address*/
        if (strncmp(token, MATCH_DL_SRC KEY_VAL_EQU, strlen(MATCH_DL_SRC KEY_VAL_EQU)) == 0)
        {
            uint8_t eth_src[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_DL_SRC KEY_VAL_EQU), eth_src, &mask))
            {
                ofp_fatal(0, "Error parsing dl_src: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put_eth(m, OXM_OF_ETH_SRC, eth_src);
                else
                {
                    ofl_structs_match_put_eth_m(m, OXM_OF_ETH_SRC_W, eth_src, mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_DL_DST KEY_VAL_EQU, strlen(MATCH_DL_DST KEY_VAL_EQU)) == 0)
        {
            uint8_t eth_dst[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_DL_DST KEY_VAL_EQU), eth_dst, &mask))
            {
                ofp_fatal(0, "Error parsing dl_dst: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put_eth(m, OXM_OF_ETH_DST, eth_dst);
                else
                {
                    ofl_structs_match_put_eth_m(m, OXM_OF_ETH_DST_W, eth_dst, mask);
                    free(mask);
                }
            }
            continue;
        }
        /* ARP */
        if (strncmp(token, MATCH_ARP_SHA KEY_VAL_EQU, strlen(MATCH_ARP_SHA KEY_VAL_EQU)) == 0)
        {
            uint8_t arp_sha[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_ARP_SHA KEY_VAL_EQU), arp_sha, &mask))
            {
                ofp_fatal(0, "Error parsing arp_sha: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put_eth(m, OXM_OF_ARP_SHA, arp_sha);
                else
                {
                    ofl_structs_match_put_eth_m(m, OXM_OF_ARP_SHA_W, arp_sha, mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_ARP_THA KEY_VAL_EQU, strlen(MATCH_ARP_THA KEY_VAL_EQU)) == 0)
        {
            uint8_t arp_tha[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_ARP_THA KEY_VAL_EQU), arp_tha, &mask))
            {
                ofp_fatal(0, "Error parsing arp_tha %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put_eth(m, OXM_OF_ARP_THA, arp_tha);
                else
                {
                    ofl_structs_match_put_eth_m(m, OXM_OF_ARP_THA_W, arp_tha, mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_ARP_SPA KEY_VAL_EQU, strlen(MATCH_ARP_SPA KEY_VAL_EQU)) == 0)
        {
            uint32_t arp_src;
            uint32_t *mask;
            if (parse_nw_addr(token + strlen(MATCH_ARP_SPA KEY_VAL_EQU), &(arp_src), &mask))
            {
                ofp_fatal(0, "Error parsing arp_src: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put32(m, OXM_OF_ARP_SPA, arp_src);
                else
                {
                    ofl_structs_match_put32m(m, OXM_OF_ARP_SPA_W, arp_src, *mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_ARP_TPA KEY_VAL_EQU, strlen(MATCH_ARP_TPA KEY_VAL_EQU)) == 0)
        {
            uint32_t arp_target;
            uint32_t *mask;
            if (parse_nw_addr(token + strlen(MATCH_ARP_TPA KEY_VAL_EQU), &(arp_target), &mask))
            {
                ofp_fatal(0, "Error parsing arp_target: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put32(m, OXM_OF_ARP_TPA, arp_target);
                else
                {
                    ofl_structs_match_put32m(m, OXM_OF_ARP_TPA_W, arp_target, *mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_ARP_OP KEY_VAL_EQU, strlen(MATCH_ARP_OP KEY_VAL_EQU)) == 0)
        {
            uint16_t arp_op;
            if (parse16(token + strlen(MATCH_ARP_OP KEY_VAL_EQU), NULL, 0, 0x7, &arp_op))
            {
                ofp_fatal(0, "Error parsing arp_op: %s.", token);
            }
            else
            {
                ofl_structs_match_put16(m, OXM_OF_ARP_OP, arp_op);
            }
            continue;
        }

        /* VLAN */
        if (strncmp(token, MATCH_DL_VLAN KEY_VAL_EQU, strlen(MATCH_DL_VLAN KEY_VAL_EQU)) == 0)
        {
            uint16_t dl_vlan;
            char *str = token + strlen(MATCH_DL_VLAN KEY_VAL_EQU);

            if (strcmp(str, "any") == 0)
                ofl_structs_match_put16m(m, OXM_OF_VLAN_VID_W, OFPVID_PRESENT, OFPVID_PRESENT);
            else if (strcmp(str, "none") == 0)
                ofl_structs_match_put16(m, OXM_OF_VLAN_VID, OFPVID_NONE);
            else if (parse16(str, NULL, 0, 0xfff, &dl_vlan))
                ofp_fatal(0, "Error parsing vlan label: %s.", token);
            else
                ofl_structs_match_put16(m, OXM_OF_VLAN_VID, dl_vlan | OFPVID_PRESENT);

            continue;
        }
        if (strncmp(token, MATCH_DL_VLAN_PCP KEY_VAL_EQU, strlen(MATCH_DL_VLAN_PCP KEY_VAL_EQU)) == 0)
        {
            uint8_t pcp;
            if (parse8(token + strlen(MATCH_DL_VLAN_PCP KEY_VAL_EQU), NULL, 0, 0x7, &pcp))
            {
                ofp_fatal(0, "Error parsing vlan pcp: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_VLAN_PCP, pcp);
            continue;
        }

        /* Eth Type */
        if (strncmp(token, MATCH_DL_TYPE KEY_VAL_EQU, strlen(MATCH_DL_TYPE KEY_VAL_EQU)) == 0)
        {
            uint16_t dl_type;
            if (parse16(token + strlen(MATCH_DL_TYPE KEY_VAL_EQU), NULL, 0, 0xffff, &dl_type))
            {
                ofp_fatal(0, "Error parsing eth_type: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_ETH_TYPE, dl_type);
            continue;
        }

        /* IP */
        if (strncmp(token, MATCH_IP_ECN KEY_VAL_EQU, strlen(MATCH_IP_ECN KEY_VAL_EQU)) == 0)
        {
            uint8_t ip_ecn;
            if (parse8(token + strlen(MATCH_IP_ECN KEY_VAL_EQU), NULL, 0, 0x3f, &ip_ecn))
            {
                ofp_fatal(0, "Error parsing nw_tos: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_IP_ECN, ip_ecn);
            continue;
        }
        if (strncmp(token, MATCH_IP_DSCP KEY_VAL_EQU, strlen(MATCH_IP_DSCP KEY_VAL_EQU)) == 0)
        {
            uint8_t ip_dscp;
            if (parse8(token + strlen(MATCH_IP_DSCP KEY_VAL_EQU), NULL, 0, 0x3f, &ip_dscp))
            {
                ofp_fatal(0, "Error parsing nw_tos: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_IP_DSCP, ip_dscp);
            continue;
        }
        if (strncmp(token, MATCH_NW_PROTO KEY_VAL_EQU, strlen(MATCH_NW_PROTO KEY_VAL_EQU)) == 0)
        {
            uint8_t nw_proto;
            if (parse8(token + strlen(MATCH_NW_PROTO KEY_VAL_EQU), NULL, 0, 0xff, &nw_proto))
            {
                ofp_fatal(0, "Error parsing ip_proto: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_IP_PROTO, nw_proto);
            continue;
        }
        if (strncmp(token, MATCH_NW_SRC KEY_VAL_EQU, strlen(MATCH_NW_SRC KEY_VAL_EQU)) == 0)
        {
            uint32_t nw_src;
            uint32_t *mask;
            if (parse_nw_addr(token + strlen(MATCH_NW_SRC KEY_VAL_EQU), &(nw_src), &mask))
            {
                ofp_fatal(0, "Error parsing ip_src: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put32(m, OXM_OF_IPV4_SRC, nw_src);
                else
                {
                    ofl_structs_match_put32m(m, OXM_OF_IPV4_SRC_W, nw_src, *mask);
                    free(mask);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_NW_DST KEY_VAL_EQU, strlen(MATCH_NW_DST KEY_VAL_EQU)) == 0)
        {
            uint32_t nw_dst;
            uint32_t *mask;
            if (parse_nw_addr(token + strlen(MATCH_NW_DST KEY_VAL_EQU), &nw_dst, &mask))
            {
                ofp_fatal(0, "Error parsing ip_dst: %s.", token);
            }
            else
            {
                if (mask == NULL)
                    ofl_structs_match_put32(m, OXM_OF_IPV4_DST, nw_dst);
                else
                {
                    ofl_structs_match_put32m(m, OXM_OF_IPV4_DST_W, nw_dst, *mask);
                    free(mask);
                }
            }
            continue;
        }

        /* ICMP */
        if (strncmp(token, MATCH_ICMPV4_CODE KEY_VAL_EQU, strlen(MATCH_ICMPV4_CODE KEY_VAL_EQU)) == 0)
        {
            uint8_t icmpv4_code;
            if (parse8(token + strlen(MATCH_ICMPV4_CODE KEY_VAL_EQU), NULL, 0, 0x3f, &icmpv4_code))
            {
                ofp_fatal(0, "Error parsing icmpv4_code: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_ICMPV4_CODE, icmpv4_code);
            continue;
        }
        if (strncmp(token, MATCH_ICMPV4_TYPE KEY_VAL_EQU, strlen(MATCH_ICMPV4_TYPE KEY_VAL_EQU)) == 0)
        {
            uint8_t icmpv4_type;
            if (parse8(token + strlen(MATCH_ICMPV4_TYPE KEY_VAL_EQU), NULL, 0, 0x3f, &icmpv4_type))
            {
                ofp_fatal(0, "Error parsing icmpv4_type: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_ICMPV4_TYPE, icmpv4_type);
            continue;
        }

        /* TCP */
        if (strncmp(token, MATCH_TP_SRC KEY_VAL_EQU, strlen(MATCH_TP_SRC KEY_VAL_EQU)) == 0)
        {
            uint16_t tp_src;
            if (parse16(token + strlen(MATCH_TP_SRC KEY_VAL_EQU), NULL, 0, 0xffff, &tp_src))
            {
                ofp_fatal(0, "Error parsing tcp_src: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_TCP_SRC, tp_src);
            continue;
        }
        if (strncmp(token, MATCH_TP_DST KEY_VAL_EQU, strlen(MATCH_TP_DST KEY_VAL_EQU)) == 0)
        {
            uint16_t tp_dst;
            if (parse16(token + strlen(MATCH_TP_DST KEY_VAL_EQU), NULL, 0, 0xffff, &tp_dst))
            {
                ofp_fatal(0, "Error parsing tcp_dst: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_TCP_DST, tp_dst);
            continue;
        }

        /*UDP */
        if (strncmp(token, MATCH_UDP_SRC KEY_VAL_EQU, strlen(MATCH_UDP_SRC KEY_VAL_EQU)) == 0)
        {
            uint16_t udp_src;
            if (parse16(token + strlen(MATCH_UDP_SRC KEY_VAL_EQU), NULL, 0, 0xffff, &udp_src))
            {
                ofp_fatal(0, "Error parsing udp_src: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_UDP_SRC, udp_src);
            continue;
        }
        if (strncmp(token, MATCH_UDP_DST KEY_VAL_EQU, strlen(MATCH_UDP_DST KEY_VAL_EQU)) == 0)
        {
            uint16_t udp_dst;
            if (parse16(token + strlen(MATCH_UDP_DST KEY_VAL_EQU), NULL, 0, 0xffff, &udp_dst))
            {
                ofp_fatal(0, "Error parsing udp_dst: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_UDP_DST, udp_dst);
            continue;
        }

        /*SCTP*/
        if (strncmp(token, MATCH_SCTP_SRC KEY_VAL_EQU, strlen(MATCH_SCTP_SRC KEY_VAL_EQU)) == 0)
        {
            uint16_t sctp_src;
            if (parse16(token + strlen(MATCH_SCTP_SRC KEY_VAL_EQU), NULL, 0, 0xffff, &sctp_src))
            {
                ofp_fatal(0, "Error parsing sctp_src: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_SCTP_SRC, sctp_src);
            continue;
        }
        if (strncmp(token, MATCH_SCTP_DST KEY_VAL_EQU, strlen(MATCH_SCTP_DST KEY_VAL_EQU)) == 0)
        {
            uint16_t sctp_dst;
            if (parse16(token + strlen(MATCH_SCTP_DST KEY_VAL_EQU), NULL, 0, 0xffff, &sctp_dst))
            {
                ofp_fatal(0, "Error parsing sctp_dst: %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_SCTP_DST, sctp_dst);
            continue;
        }
        /* MPLS  */
        if (strncmp(token, MATCH_MPLS_LABEL KEY_VAL_EQU, strlen(MATCH_MPLS_LABEL KEY_VAL_EQU)) == 0)
        {
            uint32_t mpls_label;
            if (parse32(token + strlen(MATCH_MPLS_LABEL KEY_VAL_EQU), NULL, 0, 0xfffff, &mpls_label))
            {
                ofp_fatal(0, "Error parsing mpls_label: %s.", token);
            }
            else
                ofl_structs_match_put32(m, OXM_OF_MPLS_LABEL, mpls_label);
            continue;
        }
        if (strncmp(token, MATCH_MPLS_TC KEY_VAL_EQU, strlen(MATCH_MPLS_TC KEY_VAL_EQU)) == 0)
        {
            uint8_t mpls_tc;
            if (parse8(token + strlen(MATCH_MPLS_TC KEY_VAL_EQU), NULL, 0, 0x07, &mpls_tc))
            {
                ofp_fatal(0, "Error parsing mpls_tc: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_MPLS_TC, mpls_tc);
            continue;
        }
        if (strncmp(token, MATCH_MPLS_BOS KEY_VAL_EQU, strlen(MATCH_MPLS_BOS KEY_VAL_EQU)) == 0)
        {
            uint8_t mpls_bos;
            if (parse8(token + strlen(MATCH_MPLS_BOS KEY_VAL_EQU), NULL, 0, 0x1, &mpls_bos))
            {
                ofp_fatal(0, "Error parsing mpls_tc: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_MPLS_BOS, mpls_bos);
            continue;
        }
        /* IPv6 */
        if (strncmp(token, MATCH_NW_SRC_IPV6 KEY_VAL_EQU, strlen(MATCH_NW_SRC_IPV6 KEY_VAL_EQU)) == 0)
        {
            struct in6_addr addr, mask;
            struct in6_addr in6addr_zero = IN6ADDR_ZERO_INIT;
            if (str_to_ipv6(token + strlen(MATCH_NW_DST_IPV6) + 1, &addr, &mask) < 0)
            {
                ofp_fatal(0, "Error parsing nw_src_ipv6: %s.", token);
            }
            else
            {
                if (ipv6_addr_equals(&mask, &in6addr_zero))
                {
                    ofl_structs_match_put_ipv6(m, OXM_OF_IPV6_SRC, addr.s6_addr);
                }
                else
                {
                    ofl_structs_match_put_ipv6m(m, OXM_OF_IPV6_SRC_W, addr.s6_addr, mask.s6_addr);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_NW_DST_IPV6 KEY_VAL_EQU, strlen(MATCH_NW_DST_IPV6 KEY_VAL_EQU)) == 0)
        {
            struct in6_addr addr, mask;
            struct in6_addr in6addr_zero = IN6ADDR_ZERO_INIT;
            if (str_to_ipv6(token + strlen(MATCH_NW_DST_IPV6) + 1, &addr, &mask) < 0)
            {
                ofp_fatal(0, "Error parsing nw_src_ipv6: %s.", token);
            }
            else
            {
                if (ipv6_addr_equals(&mask, &in6addr_zero))
                {
                    ofl_structs_match_put_ipv6(m, OXM_OF_IPV6_DST, addr.s6_addr);
                }
                else
                {
                    ofl_structs_match_put_ipv6m(m, OXM_OF_IPV6_DST_W, addr.s6_addr, mask.s6_addr);
                }
            }
            continue;
        }
        if (strncmp(token, MATCH_IPV6_FLABEL KEY_VAL_EQU, strlen(MATCH_IPV6_FLABEL KEY_VAL_EQU)) == 0)
        {
            uint32_t ipv6_label;
            uint32_t *mask;
            if (parse32m(token + strlen(MATCH_IPV6_FLABEL KEY_VAL_EQU), NULL, 0, 0xfffff, &ipv6_label, &mask))
            {
                ofp_fatal(0, "Error parsing ipv6_label: %s.", token);
            }
            else if (mask == NULL)
                ofl_structs_match_put32(m, OXM_OF_IPV6_FLABEL, ipv6_label);
            else
            {
                ofl_structs_match_put32m(m, OXM_OF_IPV6_FLABEL_W, ipv6_label, *mask);
                free(mask);
            }
            continue;
        }

        /* ICMPv6 */
        if (strncmp(token, MATCH_ICMPV6_CODE KEY_VAL_EQU, strlen(MATCH_ICMPV6_CODE KEY_VAL_EQU)) == 0)
        {
            uint8_t icmpv6_code;
            if (parse8(token + strlen(MATCH_ICMPV6_CODE KEY_VAL_EQU), NULL, 0, 0x3f, &icmpv6_code))
            {
                ofp_fatal(0, "Error parsing icmpv6_code: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_ICMPV6_CODE, icmpv6_code);
            continue;
        }
        if (strncmp(token, MATCH_ICMPV6_TYPE KEY_VAL_EQU, strlen(MATCH_ICMPV6_TYPE KEY_VAL_EQU)) == 0)
        {
            uint8_t icmpv6_type;
            if (parse8(token + strlen(MATCH_ICMPV6_TYPE KEY_VAL_EQU), NULL, 0, 0x3f, &icmpv6_type))
            {
                ofp_fatal(0, "Error parsing icmpv6_type: %s.", token);
            }
            else
                ofl_structs_match_put8(m, OXM_OF_ICMPV6_TYPE, icmpv6_type);
            continue;
        }

        /* IPv6 ND */
        if (strncmp(token, MATCH_IPV6_ND_TARGET KEY_VAL_EQU, strlen(MATCH_IPV6_ND_TARGET KEY_VAL_EQU)) == 0)
        {
            struct in6_addr addr, mask;
            if (str_to_ipv6(token + strlen(MATCH_IPV6_ND_TARGET) + 1, &addr, &mask) < 0)
            {
                ofp_fatal(0, "Error parsing ipv6_nd_target %s.", token);
            }
            else
            {
                ofl_structs_match_put_ipv6(m, OXM_OF_IPV6_ND_TARGET, addr.s6_addr);
            }
            continue;
        }
        if (strncmp(token, MATCH_IPV6_ND_SLL KEY_VAL_EQU, strlen(MATCH_IPV6_ND_SLL KEY_VAL_EQU)) == 0)
        {
            uint8_t eth_src[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_IPV6_ND_SLL KEY_VAL_EQU), eth_src, &mask))
            {
                ofp_fatal(0, "Error parsing ipv6_nd_sll: %s.", token);
            }
            else
            {
                ofl_structs_match_put_eth(m, OXM_OF_IPV6_ND_SLL, eth_src);
            }
            if (mask != NULL)
                free(mask);
            continue;
        }
        if (strncmp(token, MATCH_IPV6_ND_TLL KEY_VAL_EQU, strlen(MATCH_IPV6_ND_TLL KEY_VAL_EQU)) == 0)
        {
            uint8_t eth_dst[6];
            uint8_t *mask;
            if (parse_dl_addr(token + strlen(MATCH_IPV6_ND_TLL KEY_VAL_EQU), eth_dst, &mask))
            {
                ofp_fatal(0, "Error parsing ipv_nd_tll: %s.", token);
            }
            else
            {
                ofl_structs_match_put_eth(m, OXM_OF_IPV6_ND_TLL, eth_dst);
            }
            if (mask != NULL)
                free(mask);
            continue;
        }

        /* Metadata */
        if (strncmp(token, MATCH_METADATA KEY_VAL_EQU, strlen(MATCH_METADATA KEY_VAL_EQU)) == 0)
        {
            uint64_t metadata;
            uint64_t *mask;
            if (parse64m(token + strlen(MATCH_METADATA KEY_VAL_EQU), 0xffffffffffffffffULL, &metadata, &mask))
            {
                ofp_fatal(0, "Error parsing meta: %s.", token);
            }
            else if (mask == NULL)
                ofl_structs_match_put64(m, OXM_OF_METADATA, metadata);
            else
            {
                ofl_structs_match_put64m(m, OXM_OF_METADATA_W, metadata, *mask);
                free(mask);
            }
            continue;
        }
        /*PBB ISID*/
        if (strncmp(token, MATCH_PBB_ISID KEY_VAL_EQU, strlen(MATCH_PBB_ISID KEY_VAL_EQU)) == 0)
        {
            uint32_t pbb_isid;
            if (parse32(token + strlen(MATCH_PBB_ISID KEY_VAL_EQU), NULL, 0, 0x1000000, &pbb_isid))
            {
                ofp_fatal(0, "Error parsing pbb_isid: %s.", token);
            }
            else
                ofl_structs_match_put32(m, OXM_OF_PBB_ISID, pbb_isid);
            continue;
        }
        /* Tunnel ID */
        if (strncmp(token, MATCH_TUNNEL_ID KEY_VAL_EQU, strlen(MATCH_TUNNEL_ID KEY_VAL_EQU)) == 0)
        {
            uint64_t tunn_id;
            uint64_t *mask;
            if (parse64m(token + strlen(MATCH_TUNNEL_ID KEY_VAL_EQU), 0xffffffffffffffffULL, &tunn_id, &mask))
            {
                ofp_fatal(0, "Error parsing tunn_id: %s.", token);
            }
            else if (mask == NULL)
                ofl_structs_match_put64(m, OXM_OF_TUNNEL_ID, tunn_id);
            else
            {
                ofl_structs_match_put64m(m, OXM_OF_TUNNEL_ID_W, tunn_id, *mask);
                free(mask);
            }
            continue;
        }
        /*Extension Headers */
        if (strncmp(token, MATCH_EXT_HDR KEY_VAL_EQU, strlen(MATCH_EXT_HDR KEY_VAL_EQU)) == 0)
        {
            uint16_t ext_hdr;
            if (parse_ext_hdr(token + strlen(MATCH_EXT_HDR KEY_VAL_EQU), &ext_hdr))
            {
                ofp_fatal(0, "Error parsing ext_hdr %s.", token);
            }
            else
                ofl_structs_match_put16(m, OXM_OF_IPV6_EXTHDR, ext_hdr);
            continue;
        }
        ofp_fatal(0, "Error parsing match arg: %s.", token);
    }

    (*match) = (struct ofl_match_header *)m;
}

static int
parse_set_field(char *token, struct ofl_action_set_field *act)
{

    if (strncmp(token, MATCH_DL_SRC KEY_VAL_COL, strlen(MATCH_DL_SRC KEY_VAL_COL)) == 0)
    {
        uint8_t *dl_src = xmalloc(6);
        uint8_t *mask = NULL;
        if (parse_dl_addr(token + strlen(MATCH_DL_SRC KEY_VAL_COL), dl_src, &mask))
        {
            ofp_fatal(0, "Error parsing dl_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ETH_SRC;
            act->field->value = (uint8_t *)dl_src;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_DL_DST KEY_VAL_COL, strlen(MATCH_DL_DST KEY_VAL_COL)) == 0)
    {
        uint8_t *dl_dst = xmalloc(6);
        uint8_t *mask = NULL;
        if (parse_dl_addr(token + strlen(MATCH_DL_DST KEY_VAL_COL), dl_dst, &mask))
        {
            ofp_fatal(0, "Error parsing dl_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ETH_DST;
            act->field->value = (uint8_t *)dl_dst;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_DL_TYPE KEY_VAL_COL, strlen(MATCH_DL_TYPE KEY_VAL_COL)) == 0)
    {
        uint16_t *dl_type = xmalloc(sizeof(uint16_t));
        if (parse16(token + strlen(MATCH_DL_TYPE KEY_VAL_COL), NULL, 0, 0xffff, dl_type))
        {
            ofp_fatal(0, "Error parsing dl_type: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ETH_TYPE;
            act->field->value = (uint8_t *)dl_type;
        }
        return 0;
    }

    /* ARP */
    if (strncmp(token, MATCH_ARP_SHA KEY_VAL_COL, strlen(MATCH_ARP_SHA KEY_VAL_COL)) == 0)
    {
        uint8_t arp_sha[6];
        uint8_t *mask;
        if (parse_dl_addr(token + strlen(MATCH_ARP_SHA KEY_VAL_COL), arp_sha, &mask))
        {
            ofp_fatal(0, "Error parsing arp_sha: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ARP_SHA;
            act->field->value = (uint8_t *)arp_sha;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_ARP_THA KEY_VAL_COL, strlen(MATCH_ARP_THA KEY_VAL_COL)) == 0)
    {
        uint8_t arp_tha[6];
        uint8_t *mask;
        if (parse_dl_addr(token + strlen(MATCH_ARP_THA KEY_VAL_COL), arp_tha, &mask))
        {
            ofp_fatal(0, "Error parsing arp_tha %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ARP_THA;
            act->field->value = (uint8_t *)arp_tha;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_ARP_SPA KEY_VAL_COL, strlen(MATCH_ARP_SPA KEY_VAL_COL)) == 0)
    {
        uint32_t *arp_src = malloc(sizeof(uint32_t));
        uint32_t *mask;
        if (parse_nw_addr(token + strlen(MATCH_ARP_SPA KEY_VAL_COL), arp_src, &mask))
        {
            ofp_fatal(0, "Error parsing arp_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ARP_SPA;
            act->field->value = (uint8_t *)arp_src;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_ARP_TPA KEY_VAL_COL, strlen(MATCH_ARP_TPA KEY_VAL_COL)) == 0)
    {
        uint32_t *arp_target = malloc(sizeof(uint32_t));
        uint32_t *mask;
        if (parse_nw_addr(token + strlen(MATCH_ARP_TPA KEY_VAL_COL), arp_target, &mask))
        {
            ofp_fatal(0, "Error parsing arp_target: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ARP_TPA;
            act->field->value = (uint8_t *)arp_target;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_ARP_OP KEY_VAL_COL, strlen(MATCH_ARP_OP KEY_VAL_COL)) == 0)
    {
        uint16_t *arp_op = xmalloc(sizeof(uint16_t));
        if (parse16(token + strlen(MATCH_ARP_OP KEY_VAL_COL), NULL, 0, 0x7, arp_op))
        {
            ofp_fatal(0, "Error parsing arp_op: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_ARP_OP;
            act->field->value = (uint8_t *)arp_op;
        }
        return 0;
    }
    if (strncmp(token, MATCH_DL_VLAN KEY_VAL_COL, strlen(MATCH_DL_VLAN KEY_VAL_COL)) == 0)
    {
        uint16_t *dl_vlan = malloc(sizeof(uint16_t));
        if (parse_vlan_vid(token + strlen(MATCH_DL_VLAN KEY_VAL_COL), dl_vlan))
        {
            ofp_fatal(0, "Error parsing vlan label: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_VLAN_VID;
            *dl_vlan = *dl_vlan | OFPVID_PRESENT;
            act->field->value = (uint8_t *)dl_vlan;
        }
        return 0;
    }
    if (strncmp(token, MATCH_DL_VLAN_PCP KEY_VAL_COL, strlen(MATCH_DL_VLAN_PCP KEY_VAL_COL)) == 0)
    {
        uint8_t *pcp = malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_DL_VLAN_PCP KEY_VAL_COL), NULL, 0, 0x7, pcp))
        {
            ofp_fatal(0, "Error parsing vlan pcp: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_VLAN_PCP;
            act->field->value = (uint8_t *)pcp;
        }
        return 0;
    }
    if (strncmp(token, MATCH_PBB_ISID KEY_VAL_COL, strlen(MATCH_PBB_ISID KEY_VAL_COL)) == 0)
    {
        uint32_t *pbb_isid = malloc(sizeof(uint32_t));
        if (parse32(token + strlen(MATCH_PBB_ISID KEY_VAL_COL), NULL, 0, 0x1000000, pbb_isid))
        {
            ofp_fatal(0, "Error parsing pbb service id: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_PBB_ISID;
            act->field->value = (uint8_t *)pbb_isid;
        }
        return 0;
    }
    if (strncmp(token, MATCH_MPLS_LABEL KEY_VAL_COL, strlen(MATCH_MPLS_LABEL KEY_VAL_COL)) == 0)
    {
        uint32_t *mpls_label = malloc(sizeof(uint32_t));
        if (parse32(token + strlen(MATCH_MPLS_LABEL KEY_VAL_COL), NULL, 0, 0x1000000, mpls_label))
        {
            ofp_fatal(0, "Error parsing mpls label id: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_MPLS_LABEL;
            act->field->value = (uint8_t *)mpls_label;
        }
        return 0;
    }
    if (strncmp(token, MATCH_MPLS_TC KEY_VAL_COL, strlen(MATCH_MPLS_TC KEY_VAL_COL)) == 0)
    {
        uint8_t *mpls_tc = (uint8_t *)malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_MPLS_TC KEY_VAL_COL), NULL, 0, 0x07, mpls_tc))
        {
            ofp_fatal(0, "Error parsing mpls_tc: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_MPLS_TC;
            act->field->value = mpls_tc;
        }
        return 0;
    }
    if (strncmp(token, MATCH_MPLS_BOS KEY_VAL_COL, strlen(MATCH_MPLS_BOS KEY_VAL_COL)) == 0)
    {
        uint8_t *mpls_bos = (uint8_t *)malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_MPLS_BOS KEY_VAL_COL), NULL, 0, 0x01, mpls_bos))
        {
            ofp_fatal(0, "Error parsing mpls_bos: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_MPLS_BOS;
            act->field->value = mpls_bos;
        }
        return 0;
    }
    if (strncmp(token, MATCH_DL_VLAN_PCP KEY_VAL_COL, strlen(MATCH_DL_VLAN_PCP KEY_VAL_COL)) == 0)
    {
        uint8_t *vlan_pcp = malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_DL_VLAN_PCP KEY_VAL_COL), NULL, 0, 0x7, vlan_pcp))
        {
            ofp_fatal(0, "Error parsing vlan pcp: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_VLAN_PCP;
            act->field->value = (uint8_t *)vlan_pcp;
        }
        return 0;
    }
    if (strncmp(token, MATCH_NW_SRC KEY_VAL_COL, strlen(MATCH_NW_SRC KEY_VAL_COL)) == 0)
    {
        uint32_t *nw_src = malloc(sizeof(uint32_t));
        uint32_t *mask;

        if (parse_nw_addr(token + strlen(MATCH_NW_SRC KEY_VAL_COL), nw_src, &mask))
        {
            ofp_fatal(0, "Error parsing ip_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IPV4_SRC;
            act->field->value = (uint8_t *)nw_src;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_NW_DST KEY_VAL_COL, strlen(MATCH_NW_DST KEY_VAL_COL)) == 0)
    {
        uint32_t *nw_dst = malloc(sizeof(uint32_t));
        uint32_t *mask;

        if (parse_nw_addr(token + strlen(MATCH_NW_DST KEY_VAL_COL), nw_dst, &mask))
        {
            ofp_fatal(0, "Error parsing ip_dst: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IPV4_DST;
            act->field->value = (uint8_t *)nw_dst;
        }
        if (mask != NULL)
            free(mask);
        return 0;
    }
    if (strncmp(token, MATCH_IP_ECN KEY_VAL_COL, strlen(MATCH_NW_DST KEY_VAL_COL)) == 0)
    {
        uint8_t *ip_ecn = malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_IP_ECN KEY_VAL_COL), NULL, 0, 0x3, ip_ecn))
        {
            ofp_fatal(0, "Error parsing nw_tos: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IP_ECN;
            act->field->value = (uint8_t *)ip_ecn;
        }
        return 0;
    }
    if (strncmp(token, MATCH_IP_DSCP KEY_VAL_COL, strlen(MATCH_NW_DST KEY_VAL_COL)) == 0)
    {
        uint8_t *dscp = malloc(sizeof(uint8_t));

        if (parse8(token + strlen(MATCH_IP_DSCP KEY_VAL_COL), NULL, 0, 0x40, dscp))
        {
            ofp_fatal(0, "Error parsing nw_tos: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IP_DSCP;
            act->field->value = (uint8_t *)dscp;
        }
        return 0;
    }
    if (strncmp(token, MATCH_NW_PROTO KEY_VAL_COL, strlen(MATCH_NW_PROTO KEY_VAL_COL)) == 0)
    {
        uint8_t *nw_proto = malloc(sizeof(uint8_t));
        if (parse8(token + strlen(MATCH_NW_PROTO KEY_VAL_COL), NULL, 0, 0xff, nw_proto))
        {
            ofp_fatal(0, "Error parsing ip_proto: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IP_PROTO;
            act->field->value = (uint8_t *)nw_proto;
        }
        return 0;
    }
    if (strncmp(token, MATCH_TP_SRC KEY_VAL_COL, strlen(MATCH_TP_SRC KEY_VAL_COL)) == 0)
    {
        uint16_t *tp_src = xmalloc(2);
        if (parse16(token + strlen(MATCH_TP_SRC KEY_VAL_COL), NULL, 0, 0xffff, tp_src))
        {
            ofp_fatal(0, "Error parsing tcp_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_TCP_SRC;
            act->field->value = (uint8_t *)tp_src;
        }
        return 0;
    }
    if (strncmp(token, MATCH_TP_DST KEY_VAL_COL, strlen(MATCH_TP_DST KEY_VAL_COL)) == 0)
    {
        uint16_t *tp_dst = xmalloc(2);
        if (parse16(token + strlen(MATCH_TP_DST KEY_VAL_COL), NULL, 0, 0xffff, tp_dst))
        {
            ofp_fatal(0, "Error parsing tcp_dst: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_TCP_DST;
            act->field->value = (uint8_t *)tp_dst;
        }
        return 0;
    }
    if (strncmp(token, MATCH_UDP_SRC KEY_VAL_COL, strlen(MATCH_UDP_SRC KEY_VAL_COL)) == 0)
    {
        uint16_t *udp_src = xmalloc(2);
        if (parse16(token + strlen(MATCH_UDP_SRC KEY_VAL_COL), NULL, 0, 0xffff, udp_src))
        {
            ofp_fatal(0, "Error parsing udp_src: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_UDP_SRC;
            act->field->value = (uint8_t *)udp_src;
        }
        return 0;
    }
    if (strncmp(token, MATCH_UDP_DST KEY_VAL_COL, strlen(MATCH_UDP_DST KEY_VAL_COL)) == 0)
    {
        uint16_t *udp_dst = xmalloc(2);
        if (parse16(token + strlen(MATCH_UDP_DST KEY_VAL_COL), NULL, 0, 0xffff, udp_dst))
        {
            ofp_fatal(0, "Error parsing udp_dst: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_UDP_DST;
            act->field->value = (uint8_t *)udp_dst;
        }
        return 0;
    }
    if (strncmp(token, MATCH_NW_SRC_IPV6 KEY_VAL_COL, strlen(MATCH_NW_SRC_IPV6 KEY_VAL_COL)) == 0)
    {
        struct in6_addr *addr = (struct in6_addr *)malloc(sizeof(struct in6_addr));
        struct in6_addr mask;
        if (str_to_ipv6(token + strlen(MATCH_NW_SRC_IPV6) + 1, addr, &mask) < 0)
        {
            ofp_fatal(0, "Error parsing nw_src_ipv6: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IPV6_SRC;
            act->field->value = (uint8_t *)addr->s6_addr;
        }
        return 0;
    }
    if (strncmp(token, MATCH_NW_DST_IPV6 KEY_VAL_COL, strlen(MATCH_NW_DST_IPV6 KEY_VAL_COL)) == 0)
    {
        struct in6_addr *addr = (struct in6_addr *)malloc(sizeof(struct in6_addr));
        struct in6_addr mask;
        if (str_to_ipv6(token + strlen(MATCH_NW_DST_IPV6) + 1, addr, &mask) < 0)
        {
            ofp_fatal(0, "Error parsing nw_src_ipv6: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IPV6_DST;
            act->field->value = (uint8_t *)addr->s6_addr;
        }
        return 0;
    }
    if (strncmp(token, MATCH_IPV6_FLABEL KEY_VAL_COL, strlen(MATCH_IPV6_FLABEL KEY_VAL_COL)) == 0)
    {
        uint32_t *ipv6_label = malloc(sizeof(uint32_t));
        if (parse32(token + strlen(MATCH_IPV6_FLABEL KEY_VAL_COL), NULL, 0, 0x000fffff, ipv6_label))
        {
            ofp_fatal(0, "Error parsing ipv6_label: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_IPV6_FLABEL;
            act->field->value = (uint8_t *)ipv6_label;
        }
        return 0;
    }
    /* Tunnel ID */
    if (strncmp(token, MATCH_TUNNEL_ID KEY_VAL_COL, strlen(MATCH_TUNNEL_ID KEY_VAL_COL)) == 0)
    {
        uint64_t *tunn_id = malloc(sizeof(uint64_t));
        if (parse64(token + strlen(MATCH_TUNNEL_ID KEY_VAL_COL), 0xffffffffffffffffULL, tunn_id))
        {
            ofp_fatal(0, "Error parsing tunn_id: %s.", token);
        }
        else
        {
            act->field = (struct ofl_match_tlv *)malloc(sizeof(struct ofl_match_tlv));
            act->field->header = OXM_OF_TUNNEL_ID;
            act->field->value = (uint8_t *)tunn_id;
        }
        return 0;
    }
    ofp_fatal(0, "Error parsing set_field arg: %s.", token);
}

static void
make_all_match(struct ofl_match_header **match)
{
    struct ofl_match *m = xmalloc(sizeof(struct ofl_match));

    ofl_structs_match_init(m);

    (*match) = (struct ofl_match_header *)m;
}

static void
parse_action(uint16_t type, char *str, struct ofl_action_header **act)
{
    switch (type)
    {
    case (OFPAT_OUTPUT):
    {
        char *token, *saveptr = NULL;
        struct ofl_action_output *a = xmalloc(sizeof(struct ofl_action_output));

        token = strtok_r(str, KEY_VAL_COL, &saveptr);
        if (parse_port(token, &(a->port)))
        {
            ofp_fatal(0, "Error parsing port in output action: %s.", str);
        }
        token = strtok_r(NULL, KEY_VAL_COL, &saveptr);
        if (token == NULL)
        {
            a->max_len = 0;
        }
        else
        {
            if (parse16(token, NULL, 0, 0xffff - sizeof(struct ofp_header), &(a->max_len)))
            {
                ofp_fatal(0, "Error parsing max_len in output action: %s.", str);
            }
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_SET_FIELD):
    {
        struct ofl_action_set_field *a = xmalloc(sizeof(struct ofl_action_set_field));
        if (parse_set_field(str, a))
        {
            ofp_fatal(0, "Error parsing field in set_field action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_COPY_TTL_OUT):
    case (OFPAT_COPY_TTL_IN):
    {
        struct ofl_action_header *a = xmalloc(sizeof(struct ofl_action_header));
        (*act) = a;
        break;
    }
    case (OFPAT_SET_MPLS_TTL):
    {
        struct ofl_action_mpls_ttl *a = xmalloc(sizeof(struct ofl_action_mpls_ttl));
        if (parse8(str, NULL, 0, 255, &(a->mpls_ttl)))
        {
            ofp_fatal(0, "Error parsing ttl in mpls_ttl action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_DEC_MPLS_TTL):
    {
        struct ofl_action_header *a = xmalloc(sizeof(struct ofl_action_header));
        (*act) = a;
        break;
    }
    case (OFPAT_PUSH_VLAN):
    case (OFPAT_PUSH_PBB):
    case (OFPAT_PUSH_MPLS):
    {
        struct ofl_action_push *a = xmalloc(sizeof(struct ofl_action_push));
        if (sscanf(str, "0x%" SCNx16 "", &(a->ethertype)) != 1)
        {
            ofp_fatal(0, "Error parsing ethertype in push_mpls/vlan/pbb action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_POP_VLAN):
    case (OFPAT_POP_PBB):
    {
        struct ofl_action_header *a = xmalloc(sizeof(struct ofl_action_header));
        (*act) = a;
        break;
    }
    case (OFPAT_POP_MPLS):
    {
        struct ofl_action_pop_mpls *a = xmalloc(sizeof(struct ofl_action_pop_mpls));
        if (sscanf(str, "0x%" SCNx16 "", &(a->ethertype)) != 1)
        {
            ofp_fatal(0, "Error parsing ethertype in pop_mpls action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_SET_QUEUE):
    {
        struct ofl_action_set_queue *a = xmalloc(sizeof(struct ofl_action_set_queue));
        if (parse32(str, NULL, 0, 0xffffffff, &(a->queue_id)))
        {
            ofp_fatal(0, "Error parsing queue in queue action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_GROUP):
    {
        struct ofl_action_group *a = xmalloc(sizeof(struct ofl_action_group));
        if (parse_group(str, &(a->group_id)))
        {
            ofp_fatal(0, "Error parsing group in group action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_SET_NW_TTL):
    {
        struct ofl_action_set_nw_ttl *a = xmalloc(sizeof(struct ofl_action_set_nw_ttl));
        if (parse8(str, NULL, 0, 255, &(a->nw_ttl)))
        {
            ofp_fatal(0, "Error parsing ttl in mpls_ttl action: %s.", str);
        }
        (*act) = (struct ofl_action_header *)a;
        break;
    }
    case (OFPAT_DEC_NW_TTL):
    {
        struct ofl_action_header *a = xmalloc(sizeof(struct ofl_action_header));
        (*act) = a;
        break;
    }
    default:
    {
        ofp_fatal(0, "Error parsing action: %s.", str);
    }
    }
    (*act)->type = type;
}

static void
parse_actions(char *str, size_t *acts_num, struct ofl_action_header ***acts)
{
    char *token, *saveptr = NULL;
    char *s;
    size_t i;
    bool found;
    struct ofl_action_header *act = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        found = false;

        for (i = 0; i < NUM_ELEMS(action_names); i++)
        {
            if (strncmp(token, action_names[i].name, strlen(action_names[i].name)) == 0)
            {
                s = token + strlen(action_names[i].name);

                if (strncmp(s, KEY_VAL_EQU, strlen(KEY_VAL_EQU)) == 0)
                {
                    s += strlen(KEY_VAL_EQU);
                }
                parse_action(action_names[i].code, s, &act);
                (*acts_num)++;
                (*acts) = xrealloc((*acts), sizeof(struct ofl_action_header *) * (*acts_num));
                (*acts)[(*acts_num) - 1] = act;
                found = true;
                break;
            }
        }
        if (!found)
        {
            ofp_fatal(0, "Error parsing action: %s.", token);
        }
    }
}

static void
parse_inst(char *str, struct ofl_instruction_header **inst)
{
    size_t i;
    char *s;
    for (i = 0; i < NUM_ELEMS(inst_names); i++)
    {
        if (strncmp(str, inst_names[i].name, strlen(inst_names[i].name)) == 0)
        {

            s = str + strlen(inst_names[i].name);
            if (strncmp(s, KEY_VAL_COL, strlen(KEY_VAL_COL)) != 0)
            {
                ofp_fatal(0, "Error parsing instruction: %s.", str);
            }
            s += strlen(KEY_VAL_COL);
            switch (inst_names[i].code)
            {
            case (OFPIT_GOTO_TABLE):
            {
                struct ofl_instruction_goto_table *i = xmalloc(sizeof(struct ofl_instruction_goto_table));
                i->header.type = OFPIT_GOTO_TABLE;
                if (parse_table(s, &(i->table_id)))
                {
                    ofp_fatal(0, "Error parsing table in goto instruction: %s.", s);
                }
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            case (OFPIT_WRITE_METADATA):
            {
                uint64_t *mask;
                struct ofl_instruction_write_metadata *i = xmalloc(sizeof(struct ofl_instruction_write_metadata));
                i->header.type = OFPIT_WRITE_METADATA;
                if (parse64m(s, 0xffffffffffffffffULL, &(i->metadata), &mask))
                {
                    ofp_fatal(0, "Error parsing metadata in write metadata instruction: %s.", s);
                }
                else
                {
                    if (mask == NULL)
                    {
                        i->metadata_mask = 0xffffffffffffffffULL;
                    }
                    else
                    {
                        i->metadata_mask = *mask;
                        free(mask);
                    }
                }
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            case (OFPIT_WRITE_ACTIONS):
            {
                struct ofl_instruction_actions *i = xmalloc(sizeof(struct ofl_instruction_actions));
                i->header.type = OFPIT_WRITE_ACTIONS;
                i->actions = NULL;
                i->actions_num = 0;
                parse_actions(s, &(i->actions_num), &(i->actions));
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            case (OFPIT_APPLY_ACTIONS):
            {
                struct ofl_instruction_actions *i = xmalloc(sizeof(struct ofl_instruction_actions));
                i->header.type = OFPIT_APPLY_ACTIONS;
                i->actions = NULL;
                i->actions_num = 0;
                parse_actions(s, &(i->actions_num), &(i->actions));
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            case (OFPIT_METER):
            {
                struct ofl_instruction_meter *i = xmalloc(sizeof(struct ofl_instruction_meter));
                i->header.type = OFPIT_METER;
                if (parse32(s, NULL, 0, OFPM_MAX, &i->meter_id))
                {
                    ofp_fatal(0, "Error parsing meter instruction: %s.", s);
                }
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            case (OFPIT_CLEAR_ACTIONS):
            {
                struct ofl_instruction_header *i = xmalloc(sizeof(struct ofl_instruction_header));
                i->type = OFPIT_CLEAR_ACTIONS;
                (*inst) = (struct ofl_instruction_header *)i;
                return;
            }
            }
        }
    }
    ofp_fatal(0, "Error parsing instruction: %s.", str);
}

static void
parse_flow_stat_args(char *str, struct ofl_msg_multipart_request_flow *req)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, FLOW_MOD_COOKIE KEY_VAL_EQU, strlen(FLOW_MOD_COOKIE KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_COOKIE KEY_VAL_EQU "0x%" SCNx64 "", &(req->cookie)) != 1)
            {
                ofp_fatal(0, "Error parsing flow_stat cookie: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_COOKIE_MASK KEY_VAL_EQU, strlen(FLOW_MOD_COOKIE_MASK KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_COOKIE_MASK KEY_VAL_EQU "0x%" SCNx64 "", &(req->cookie_mask)) != 1)
            {
                ofp_fatal(0, "Error parsing flow_stat cookie mask: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_TABLE_ID KEY_VAL_EQU, strlen(FLOW_MOD_TABLE_ID KEY_VAL_EQU)) == 0)
        {
            if (parse8(token + strlen(FLOW_MOD_TABLE_ID KEY_VAL_EQU), table_names, NUM_ELEMS(table_names), 254, &req->table_id))
            {
                ofp_fatal(0, "Error parsing flow_stat table: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_OUT_PORT KEY_VAL_EQU, strlen(FLOW_MOD_OUT_PORT KEY_VAL_EQU)) == 0)
        {
            if (parse_port(token + strlen(FLOW_MOD_OUT_PORT KEY_VAL_EQU), &req->out_port))
            {
                ofp_fatal(0, "Error parsing flow_stat port: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_OUT_GROUP KEY_VAL_EQU, strlen(FLOW_MOD_OUT_GROUP KEY_VAL_EQU)) == 0)
        {
            if (parse_group(token + strlen(FLOW_MOD_OUT_GROUP KEY_VAL_EQU), &req->out_port))
            {
                ofp_fatal(0, "Error parsing flow_stat group: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing flow_stat arg: %s.", token);
    }
}

static void
parse_flow_mod_args(char *str, struct ofl_msg_flow_mod *req)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, FLOW_MOD_COMMAND KEY_VAL_EQU, strlen(FLOW_MOD_COMMAND KEY_VAL_EQU)) == 0)
        {
            uint8_t command;
            if (parse8(token + strlen(FLOW_MOD_COMMAND KEY_VAL_EQU), flow_mod_cmd_names, NUM_ELEMS(flow_mod_cmd_names), 0, &command))
            {
                ofp_fatal(0, "Error parsing flow_mod command: %s.", token);
            }
            req->command = command;
            continue;
        }
        if (strncmp(token, FLOW_MOD_COOKIE KEY_VAL_EQU, strlen(FLOW_MOD_COOKIE KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_COOKIE KEY_VAL_EQU "0x%" SCNx64 "", &(req->cookie)) != 1)
            {
                ofp_fatal(0, "Error parsing flow_mod cookie: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_COOKIE_MASK KEY_VAL_EQU, strlen(FLOW_MOD_COOKIE_MASK KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_COOKIE_MASK KEY_VAL_EQU "0x%" SCNx64 "", &(req->cookie_mask)) != 1)
            {
                ofp_fatal(0, "Error parsing flow_mod cookie mask: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_TABLE_ID KEY_VAL_EQU, strlen(FLOW_MOD_TABLE_ID KEY_VAL_EQU)) == 0)
        {
            if (parse8(token + strlen(FLOW_MOD_TABLE_ID KEY_VAL_EQU), table_names, NUM_ELEMS(table_names), 254, &req->table_id))
            {
                ofp_fatal(0, "Error parsing flow_mod table: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_IDLE KEY_VAL_EQU, strlen(FLOW_MOD_IDLE KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_IDLE KEY_VAL_EQU "%" SCNu16 "", &(req->idle_timeout)) != 1)
            {
                ofp_fatal(0, "Error parsing %s: %s.", FLOW_MOD_IDLE, token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_HARD KEY_VAL_EQU, strlen(FLOW_MOD_HARD KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_HARD KEY_VAL_EQU "%" SCNu16 "", &(req->hard_timeout)) != 1)
            {
                ofp_fatal(0, "Error parsing %s: %s.", FLOW_MOD_HARD, token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_PRIO KEY_VAL_EQU, strlen(FLOW_MOD_PRIO KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token, FLOW_MOD_PRIO KEY_VAL_EQU "%" SCNu16 "", &(req->priority)) != 1)
            {
                ofp_fatal(0, "Error parsing %s: %s.", FLOW_MOD_PRIO, token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_BUFFER KEY_VAL_EQU, strlen(FLOW_MOD_BUFFER KEY_VAL_EQU)) == 0)
        {
            if (parse32(token + strlen(FLOW_MOD_BUFFER KEY_VAL_EQU), buffer_names, NUM_ELEMS(buffer_names), UINT32_MAX, &req->buffer_id))
            {
                ofp_fatal(0, "Error parsing flow_mod buffer: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_OUT_PORT KEY_VAL_EQU, strlen(FLOW_MOD_OUT_PORT KEY_VAL_EQU)) == 0)
        {
            if (parse_port(token + strlen(FLOW_MOD_OUT_PORT KEY_VAL_EQU), &req->out_port))
            {
                ofp_fatal(0, "Error parsing flow_mod port: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_OUT_GROUP KEY_VAL_EQU, strlen(FLOW_MOD_OUT_GROUP KEY_VAL_EQU)) == 0)
        {
            if (parse_group(token + strlen(FLOW_MOD_OUT_GROUP KEY_VAL_EQU), &req->out_port))
            {
                ofp_fatal(0, "Error parsing flow_mod group: %s.", token);
            }
            continue;
        }
        if (strncmp(token, FLOW_MOD_FLAGS KEY_VAL_EQU, strlen(FLOW_MOD_FLAGS KEY_VAL_EQU)) == 0)
        {
            if (parse16(token + strlen(FLOW_MOD_FLAGS KEY_VAL_EQU), NULL, 0, UINT16_MAX, &req->flags))
            {
                ofp_fatal(0, "Error parsing %s: %s.", FLOW_MOD_FLAGS, token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing flow_mod arg: %s.", token);
    }
}

static void
parse_group_mod_args(char *str, struct ofl_msg_group_mod *req)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, GROUP_MOD_COMMAND KEY_VAL_EQU, strlen(GROUP_MOD_COMMAND KEY_VAL_EQU)) == 0)
        {
            uint16_t command;
            if (parse16(token + strlen(GROUP_MOD_COMMAND KEY_VAL_EQU), group_mod_cmd_names, NUM_ELEMS(group_mod_cmd_names), 0, &command))
            {
                ofp_fatal(0, "Error parsing group_mod command: %s.", token);
            }
            req->command = command;
            continue;
        }
        if (strncmp(token, GROUP_MOD_GROUP KEY_VAL_EQU, strlen(GROUP_MOD_GROUP KEY_VAL_EQU)) == 0)
        {
            if (parse_group(token + strlen(GROUP_MOD_GROUP KEY_VAL_EQU), &req->group_id))
            {
                ofp_fatal(0, "Error parsing group_mod group: %s.", token);
            }
            continue;
        }
        if (strncmp(token, GROUP_MOD_TYPE KEY_VAL_EQU, strlen(GROUP_MOD_TYPE KEY_VAL_EQU)) == 0)
        {
            uint8_t type;
            if (parse8(token + strlen(GROUP_MOD_TYPE KEY_VAL_EQU), group_type_names, NUM_ELEMS(group_type_names), UINT8_MAX, &type))
            {
                ofp_fatal(0, "Error parsing group_mod type: %s.", token);
            }
            req->type = type;
            continue;
        }
        ofp_fatal(0, "Error parsing group_mod arg: %s.", token);
    }
}

static void
parse_bucket(char *str, struct ofl_bucket *b)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, BUCKET_WEIGHT KEY_VAL_EQU, strlen(BUCKET_WEIGHT KEY_VAL_EQU)) == 0)
        {
            if (parse16(token + strlen(BUCKET_WEIGHT KEY_VAL_EQU), NULL, 0, UINT16_MAX, &b->weight))
            {
                ofp_fatal(0, "Error parsing bucket_weight: %s.", token);
            }
            continue;
        }
        if (strncmp(token, BUCKET_WATCH_PORT KEY_VAL_EQU, strlen(BUCKET_WATCH_PORT KEY_VAL_EQU)) == 0)
        {
            if (parse_port(token + strlen(BUCKET_WATCH_PORT KEY_VAL_EQU), &b->watch_port))
            {
                ofp_fatal(0, "Error parsing bucket watch port: %s.", token);
            }
            continue;
        }
        if (strncmp(token, BUCKET_WATCH_GROUP KEY_VAL_EQU, strlen(BUCKET_WATCH_GROUP KEY_VAL_EQU)) == 0)
        {
            if (parse_group(token + strlen(BUCKET_WATCH_GROUP KEY_VAL_EQU), &b->watch_group))
            {
                ofp_fatal(0, "Error parsing bucket watch group: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing bucket arg: %s.", token);
    }
}

static void
parse_meter_mod_args(char *str, struct ofl_msg_meter_mod *req)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, METER_MOD_COMMAND KEY_VAL_EQU, strlen(METER_MOD_COMMAND KEY_VAL_EQU)) == 0)
        {
            uint16_t command;
            if (parse16(token + strlen(METER_MOD_COMMAND KEY_VAL_EQU), meter_mod_cmd_names, NUM_ELEMS(meter_mod_cmd_names), 0, &command))
            {
                ofp_fatal(0, "Error parsing meter_mod command: %s.", token);
            }
            req->command = command;
            continue;
        }
        if (strncmp(token, METER_MOD_FLAGS KEY_VAL_EQU, strlen(METER_MOD_FLAGS KEY_VAL_EQU)) == 0)
        {
            if (parse16(token + strlen(METER_MOD_FLAGS KEY_VAL_EQU), NULL, 0, 0xffff, &req->flags))
            {
                ofp_fatal(0, "Error parsing meter_mod flags: %s.", token);
            }
            continue;
        }
        if (strncmp(token, METER_MOD_METER KEY_VAL_EQU, strlen(METER_MOD_METER KEY_VAL_EQU)) == 0)
        {
            uint32_t meter_id;
            if (parse32(token + strlen(METER_MOD_METER KEY_VAL_EQU), NULL, 0, OFPM_MAX, &meter_id))
            {
                ofp_fatal(0, "Error parsing meter_mod id: %s.", token);
            }
            req->meter_id = meter_id;
            continue;
        }
        ofp_fatal(0, "Error parsing group_mod arg: %s.", token);
    }
}

static void
parse_band_args(char *str, struct ofl_msg_meter_mod *m, struct ofl_meter_band_header *b)
{
    char *token, *saveptr = NULL;
    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, BAND_RATE KEY_VAL_EQU, strlen(BAND_RATE KEY_VAL_EQU)) == 0)
        {
            if (parse32(token + strlen(BAND_RATE KEY_VAL_EQU), NULL, 0, UINT32_MAX, &b->rate))
            {
                ofp_fatal(0, "Error parsing band rate: %s.", token);
            }
            continue;
        }
        if (strncmp(token, BAND_BURST_SIZE KEY_VAL_EQU, strlen(BAND_BURST_SIZE KEY_VAL_EQU)) == 0)
        {
            if (m->flags & OFPMF_BURST)
            {
                if (parse32(token + strlen(BAND_BURST_SIZE KEY_VAL_EQU), NULL, 0, UINT32_MAX, &b->burst_size))
                {
                    ofp_fatal(0, "Error parsing band burst_size: %s.", token);
                }
                continue;
            }
            else
                ofp_fatal(0, "Error parsing burst size. Meter flags should contain %x.", OFPMF_BURST);
        }
        if (strncmp(token, BAND_PREC_LEVEL KEY_VAL_EQU, strlen(BAND_PREC_LEVEL KEY_VAL_EQU)) == 0)
        {
            struct ofl_meter_band_dscp_remark *d = (struct ofl_meter_band_dscp_remark *)b;
            if (parse8(token + strlen(BAND_PREC_LEVEL KEY_VAL_EQU), NULL, 0, UINT8_MAX, &d->prec_level))
            {
                ofp_fatal(0, "Error parsing band rate: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing band arg: %s.", token);
    }
}

static void
parse_band(char *str, struct ofl_msg_meter_mod *m, struct ofl_meter_band_header **b)
{
    char *s;
    size_t i;
    for (i = 0; i < NUM_ELEMS(band_names); i++)
    {

        if (strncmp(str, band_names[i].name, strlen(band_names[i].name)) == 0)
        {
            s = str + strlen(band_names[i].name);

            if (strncmp(s, KEY_VAL_COL, strlen(KEY_VAL_COL)) != 0)
            {
                ofp_fatal(0, "Error parsing meter band: %s.", str);
            }

            s += strlen(KEY_VAL_COL);
            switch (band_names[i].code)
            {
            case (OFPMBT_DROP):
            {
                struct ofl_meter_band_drop *d = (struct ofl_meter_band_drop *)xmalloc(sizeof(struct ofl_meter_band_drop));
                d->type = OFPMBT_DROP;
                d->rate = 0;
                d->burst_size = 0;
                parse_band_args(s, m, (struct ofl_meter_band_header *)d);
                *b = (struct ofl_meter_band_header *)d;
                break;
            }
            case (OFPMBT_DSCP_REMARK):
            {
                struct ofl_meter_band_dscp_remark *d = (struct ofl_meter_band_dscp_remark *)xmalloc(sizeof(struct ofl_meter_band_dscp_remark));
                d->type = OFPMBT_DSCP_REMARK;
                d->rate = 0;
                d->burst_size = 0;
                d->prec_level = 0;
                parse_band_args(s, m, (struct ofl_meter_band_header *)d);
                *b = (struct ofl_meter_band_header *)d;
                break;
            }
            }
        }
    }
}

static void
parse_config(char *str, struct ofl_config *c)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, CONFIG_FLAGS KEY_VAL_EQU, strlen(CONFIG_FLAGS KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token + strlen(CONFIG_FLAGS KEY_VAL_EQU), "0x%" SCNx16 "", &c->flags) != 1)
            {
                ofp_fatal(0, "Error parsing config flags: %s.", token);
            }
            continue;
        }
        if (strncmp(token, CONFIG_MISS KEY_VAL_EQU, strlen(CONFIG_MISS KEY_VAL_EQU)) == 0)
        {
            if (parse16(token + strlen(CONFIG_MISS KEY_VAL_EQU), NULL, 0, UINT16_MAX - sizeof(struct ofp_packet_in), &c->miss_send_len))
            {
                ofp_fatal(0, "Error parsing config miss send len: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing config arg: %s.", token);
    }
}

static void
parse_port_mod(char *str, struct ofl_msg_port_mod *msg)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, PORT_MOD_PORT KEY_VAL_EQU, strlen(PORT_MOD_PORT KEY_VAL_EQU)) == 0)
        {
            if (parse_port(token + strlen(PORT_MOD_PORT KEY_VAL_EQU), &msg->port_no))
            {
                ofp_fatal(0, "Error parsing port_mod port: %s.", token);
            }
            continue;
        }
        if (strncmp(token, PORT_MOD_HW_ADDR KEY_VAL_EQU, strlen(PORT_MOD_HW_ADDR KEY_VAL_EQU)) == 0)
        {
            uint8_t *mask = NULL;
            if (parse_dl_addr(token + strlen(PORT_MOD_HW_ADDR KEY_VAL_EQU), msg->hw_addr, &mask))
            {
                ofp_fatal(0, "Error parsing port_mod hw_addr: %s.", token);
            }
            if (mask != NULL)
                free(mask);
            continue;
        }
        if (strncmp(token, PORT_MOD_HW_CONFIG KEY_VAL_EQU, strlen(PORT_MOD_HW_CONFIG KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token + strlen(PORT_MOD_HW_CONFIG KEY_VAL_EQU), "0x%" SCNx32 "", &msg->config) != 1)
            {
                ofp_fatal(0, "Error parsing port_mod conf: %s.", token);
            }
            continue;
        }
        if (strncmp(token, PORT_MOD_MASK KEY_VAL_EQU, strlen(PORT_MOD_MASK KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token + strlen(PORT_MOD_MASK KEY_VAL_EQU), "0x%" SCNx32 "", &msg->mask) != 1)
            {
                ofp_fatal(0, "Error parsing port_mod mask: %s.", token);
            }
            continue;
        }
        if (strncmp(token, PORT_MOD_ADVERTISE KEY_VAL_EQU, strlen(PORT_MOD_ADVERTISE KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token + strlen(PORT_MOD_ADVERTISE KEY_VAL_EQU), "0x%" SCNx32 "", &msg->advertise) != 1)
            {
                ofp_fatal(0, "Error parsing port_mod advertise: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing port_mod arg: %s.", token);
    }
}

static void
parse_table_mod(char *str, struct ofl_msg_table_mod *msg)
{
    char *token, *saveptr = NULL;

    for (token = strtok_r(str, KEY_SEP, &saveptr); token != NULL; token = strtok_r(NULL, KEY_SEP, &saveptr))
    {
        if (strncmp(token, TABLE_MOD_TABLE KEY_VAL_EQU, strlen(TABLE_MOD_TABLE KEY_VAL_EQU)) == 0)
        {
            if (parse_table(token + strlen(TABLE_MOD_TABLE KEY_VAL_EQU), &msg->table_id))
            {
                ofp_fatal(0, "Error parsing table_mod table: %s.", token);
            }
            continue;
        }
        if (strncmp(token, TABLE_MOD_CONFIG KEY_VAL_EQU, strlen(TABLE_MOD_CONFIG KEY_VAL_EQU)) == 0)
        {
            if (sscanf(token + strlen(TABLE_MOD_CONFIG KEY_VAL_EQU), "0x%" SCNx32 "", &msg->config) != 1)
            {
                ofp_fatal(0, "Error parsing table_mod conf: %s.", token);
            }
            continue;
        }
        ofp_fatal(0, "Error parsing table_mod arg: %s.", token);
    }
}

static int
parse_port(char *str, uint32_t *port)
{
    return parse32(str, port_names, NUM_ELEMS(port_names), OFPP_MAX, port);
}

static int
parse_queue(char *str, uint32_t *port)
{
    return parse32(str, queue_names, NUM_ELEMS(queue_names), 0xfffffffe, port);
}

static int
parse_group(char *str, uint32_t *group)
{
    return parse32(str, group_names, NUM_ELEMS(group_names), OFPG_MAX, group);
}

static int
parse_meter(char *str, uint32_t *meter)
{
    return parse32(str, NULL, 0, OFPM_MAX, meter);
}

static int
parse_table(char *str, uint8_t *table)
{
    return parse8(str, table_names, NUM_ELEMS(table_names), 0xfe, table);
}

static int
parse_dl_addr(char *str, uint8_t *addr, uint8_t **mask)
{
    char *saveptr = NULL;
    if (sscanf(str, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8,
               addr, addr + 1, addr + 2, addr + 3, addr + 4, addr + 5) != 6)
    {
        return -1;
    }
    strtok_r(str, MASK_SEP, &saveptr);

    if (strcmp(saveptr, "") == 0)
    {
        *mask = NULL;
        return 0;
    }
    else
    {
        *mask = (uint8_t *)malloc(sizeof(OFP_ETH_ALEN));
        if (sscanf(saveptr, "%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8 ":%" SCNx8,
                   *mask, *mask + 1, *mask + 2, *mask + 3, *mask + 4, *mask + 5) != 6)
        {
            return -1;
        }
    }
    return 0;
}

static int
parse_nw_addr(char *str, uint32_t *addr, uint32_t **mask)
{
    // TODO Zoltan: DNS lookup ?
    uint8_t a[4], b[4];
    uint32_t netmask;
    char *saveptr = NULL;

    if (sscanf(str, "%" SCNu8 ".%" SCNu8 ".%" SCNu8 ".%" SCNu8,
               &a[0], &a[1], &a[2], &a[3]) == 4)
    {
        *addr = (a[3] << 24) | (a[2] << 16) | (a[1] << 8) | a[0];
    }
    else
    {
        return -1;
    }
    strtok_r(str, MASK_SEP, &saveptr);
    if (strcmp(saveptr, "") == 0)
    {
        *mask = NULL;
        return 0;
    }
    *mask = (uint32_t *)malloc(sizeof(uint32_t));
    netmask = 0xffffffff;
    if (strlen(saveptr) <= 2)
    {
        /* Subnet mask*/
        uint8_t subnet_mask;
        sscanf(saveptr, "%" SCNu8 "",
               &subnet_mask);
        if (subnet_mask > 32)
            return -1;
        if (subnet_mask == 0)
            netmask = 0x00000000;
        else
            netmask = netmask << (32 - subnet_mask);
        **mask = htonl(netmask);
    }
    else
    {
        /*Arbitrary mask*/
        if (sscanf(saveptr, "%" SCNu8 ".%" SCNu8 ".%" SCNu8 ".%" SCNu8,
                   &b[0], &b[1], &b[2], &b[3]) == 4)
        {
            **mask = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

static int
parse_vlan_vid(char *str, uint16_t *vid)
{
    return parse16(str, vlan_vid_names, NUM_ELEMS(vlan_vid_names), 0xfff, vid);
}

static int
parse_ext_hdr(char *str, uint16_t *ext_hdr)
{
    char *token, *saveptr = NULL;
    size_t i;
    memset(ext_hdr, 0x0, sizeof(uint16_t));
    for (token = strtok_r(str, ADD, &saveptr); token != NULL; token = strtok_r(NULL, ADD, &saveptr))
    {
        for (i = 0; i < 9; i++)
        {
            if (strcmp(token, ext_header_names[i].name) == 0)
            {
                *ext_hdr = *ext_hdr ^ ext_header_names[i].code;
                break;
            }
        }
        if (i == 9)
            return -1;
    }
    return 0;
}

static int
parse8(char *str, struct names8 *names, size_t names_num, uint8_t max, uint8_t *val)
{
    size_t i;

    for (i = 0; i < names_num; i++)
    {
        if (strcmp(str, names[i].name) == 0)
        {
            *val = names[i].code;
            return 0;
        }
    }

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        if ((max > 0) && (sscanf(str, "0x%" SCNx8 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    else
    {
        if ((max > 0) && (sscanf(str, "%" SCNu8 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    return -1;
}

static int
parse16(char *str, struct names16 *names, size_t names_num, uint16_t max, uint16_t *val)
{
    size_t i;

    for (i = 0; i < names_num; i++)
    {
        if (strcmp(str, names[i].name) == 0)
        {
            *val = names[i].code;
            return 0;
        }
    }

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        if ((max > 0) && (sscanf(str, "0x%" SCNx16 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    else
    {
        if ((max > 0) && (sscanf(str, "%" SCNu16 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    return -1;
}

static int
parse16m(char *str, struct names16 *names, size_t names_num, uint16_t max, uint16_t *val, uint16_t **mask)
{

    size_t i;
    int read;
    char *saveptr = NULL;

    for (i = 0; i < names_num; i++)
    {
        if (strcmp(str, names[i].name) == 0)
        {
            *val = names[i].code;
            return 0;
        }
    }

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        read = sscanf(str, "0x%" SCNx16 "", val);
    }
    else
    {
        read = sscanf(str, "%" SCNu16 "", val);
    }
    if ((read == 0) || (max == 0) || (*val > max))
    {
        return -1;
    }

    strtok_r(str, MASK_SEP, &saveptr);
    if (strcmp(saveptr, "") == 0)
    {
        *mask = NULL;
        return 0;
    }
    *mask = (uint16_t *)malloc(sizeof(uint16_t));

    /* Checks for mask in hexadecimal. */
    if (saveptr[0] == '0' && saveptr[1] == 'x')
    {
        read = sscanf(saveptr, "0x%" SCNx16 "", *mask);
    }
    else
    {
        read = sscanf(saveptr, "%" SCNu16 "", *mask);
    }
    if (read == 0)
    {
        return -1;
    }
    return 0;
}

static int
parse32(char *str, struct names32 *names, size_t names_num, uint32_t max, uint32_t *val)
{
    size_t i;

    for (i = 0; i < names_num; i++)
    {
        if (strcmp(str, names[i].name) == 0)
        {
            *val = names[i].code;
            return 0;
        }
    }

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        if ((max > 0) && (sscanf(str, "0x%" SCNx32 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    else
    {
        if ((max > 0) && (sscanf(str, "%" SCNu32 "", val)) == 1 && (*val <= max))
        {
            return 0;
        }
    }
    return -1;
}

static int
parse32m(char *str, struct names32 *names, size_t names_num, uint32_t max, uint32_t *val, uint32_t **mask)
{

    size_t i;
    int read;
    char *saveptr = NULL;

    for (i = 0; i < names_num; i++)
    {
        if (strcmp(str, names[i].name) == 0)
        {
            *val = names[i].code;
            return 0;
        }
    }

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        read = sscanf(str, "0x%" SCNx32 "", val);
    }
    else
    {
        read = sscanf(str, "%" SCNu32 "", val);
    }
    if ((read == 0) || (max == 0) || (*val > max))
    {
        return -1;
    }

    strtok_r(str, MASK_SEP, &saveptr);
    if (strcmp(saveptr, "") == 0)
    {
        *mask = NULL;
        return 0;
    }
    *mask = (uint32_t *)malloc(sizeof(uint32_t));

    /* Checks for mask in hexadecimal. */
    if (saveptr[0] == '0' && saveptr[1] == 'x')
    {
        read = sscanf(saveptr, "0x%" SCNx32 "", *mask);
    }
    else
    {
        read = sscanf(saveptr, "%" SCNu32 "", *mask);
    }
    if (read == 0)
    {
        return -1;
    }
    return 0;
}

static int
parse64(char *str, uint64_t max, uint64_t *val)
{
    int read;

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        read = sscanf(str, "0x%" SCNx64 "", val);
    }
    else
    {
        read = sscanf(str, "%" SCNu64 "", val);
    }
    if ((read == 0) || (max == 0) || (*val > max))
    {
        return -1;
    }
    return 0;
}

static int
parse64m(char *str, uint64_t max, uint64_t *val, uint64_t **mask)
{
    int read;
    char *saveptr = NULL;

    /* Checks for value in hexadecimal. */
    if (str[0] == '0' && str[1] == 'x')
    {
        read = sscanf(str, "0x%" SCNx64 "", val);
    }
    else
    {
        read = sscanf(str, "%" SCNu64 "", val);
    }
    if ((read == 0) || (max == 0) || (*val > max))
    {
        return -1;
    }

    strtok_r(str, MASK_SEP, &saveptr);
    if (strcmp(saveptr, "") == 0)
    {
        *mask = NULL;
        return 0;
    }
    *mask = (uint64_t *)malloc(sizeof(uint64_t));

    /* Checks for mask in hexadecimal. */
    if (saveptr[0] == '0' && saveptr[1] == 'x')
    {
        read = sscanf(saveptr, "0x%" SCNx64 "", *mask);
    }
    else
    {
        read = sscanf(saveptr, "%" SCNu64 "", *mask);
    }
    if (read == 0)
    {
        return -1;
    }
    return 0;
}
