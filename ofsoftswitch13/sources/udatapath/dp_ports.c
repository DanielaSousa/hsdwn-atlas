/* Copyright (c) 2008 The Board of Trustees of The Leland Stanford
 * Junior University
 *
 * We are making the OpenFlow specification and associated documentation
 * (Software) available for public use and benefit with the expectation
 * that others will use, modify and enhance the Software and contribute
 * those enhancements back to the community. However, since we would
 * like to make the Software available for broadest use, with as few
 * restrictions as possible permission is hereby granted, free of
 * charge, to any person obtaining a copy of this Software to deal in
 * the Software under the copyrights without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * The name and trademarks of copyright holder(s) may NOT be used in
 * advertising or publicity pertaining to the Software or any
 * derivatives without specific, written prior permission.
 */

/* The original Stanford code has been modified during the implementation of
 * the OpenFlow 1.1 userspace switch.
 *
 * Author: Zoltán Lajos Kis <zoltan.lajos.kis@ericsson.com>
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#include <bofuss/datapath.h>
#include <bofuss/dp_exp.h>
#include <bofuss/dp_ports.h>
#include <bofuss/packet-utils.h>
#include <bofuss/pipeline.h>
#include <bofuss/timeval.h>
#include <bofuss/util.h>
#include <bofuss/vlog.h>
#include <bofuss/vlog.h>

#define LOG_MODULE VLM_dp_ports

/* Returns the speed value in kbps of the highest bit set in the bitfield. */
uint32_t
port_speed(uint32_t conf) {
    if ((conf & OFPPF_1TB_FD) != 0)   return 1024 * 1024 * 1024;
    if ((conf & OFPPF_100GB_FD) != 0) return  100 * 1024 * 1024;
    if ((conf & OFPPF_40GB_FD) != 0)  return   40 * 1024 * 1024;
    if ((conf & OFPPF_10GB_FD) != 0)  return   10 * 1024 * 1024;
    if ((conf & OFPPF_1GB_FD) != 0)   return        1024 * 1024;
    if ((conf & OFPPF_1GB_HD) != 0)   return        1024 * 1024;
    if ((conf & OFPPF_100MB_FD) != 0) return         100 * 1024;
    if ((conf & OFPPF_100MB_HD) != 0) return         100 * 1024;
    if ((conf & OFPPF_10MB_FD) != 0)  return          10 * 1024;
    if ((conf & OFPPF_10MB_HD) != 0)  return          10 * 1024;

    return 0;
}




struct sw_port *
dp_ports_lookup(struct datapath *dp, uint32_t port_no) {

    // exclude local port from ports_num
    uint32_t ports_num = dp->local_port ? dp->ports_num -1 : dp->ports_num;

    if (port_no == OFPP_LOCAL) {
        return dp->local_port;
    }
    /* Local port already checked, so dp->ports -1 */
    if (port_no < 1 || port_no > ports_num) {
        return NULL;
    }

    return &dp->ports[port_no];
}

struct sw_queue *
dp_ports_lookup_queue(struct sw_port *p, uint32_t queue_id)
{
    struct sw_queue *q;

    if (queue_id < p->max_queues) {
        q = &(p->queues[queue_id]);

        if (q->port != NULL) {
            return q;
        }
    }

    return NULL;
}

ofl_err
dp_ports_handle_port_mod(struct datapath *dp, struct ofl_msg_port_mod *msg,
                                                const struct sender *sender) {

    struct sw_port *p;
    struct ofl_msg_port_status rep_msg;

    if(sender->remote->role == OFPCR_ROLE_SLAVE)
        return ofl_error(OFPET_BAD_REQUEST, OFPBRC_IS_SLAVE);

    p = dp_ports_lookup(dp, msg->port_no);

    if (p == NULL) {
        return ofl_error(OFPET_PORT_MOD_FAILED,OFPPMFC_BAD_PORT);
    }

    if (msg->mask) {
        p->conf->config &= ~msg->mask;
        p->conf->config |= msg->config & msg->mask;
        dp_port_live_update(p);
    }

    /*Notify all controllers that the port status has changed*/
    rep_msg.header.type = OFPT_PORT_STATUS;
    rep_msg.reason =   OFPPR_MODIFY;
    rep_msg.desc = p->conf;      
    dp_send_message(dp, (struct ofl_msg_header *)&rep_msg, NULL/*sender*/);
    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
    return 0;
}

static void
dp_port_stats_update(struct sw_port *port) {
    port->stats->duration_sec  =  (time_msec() - port->created) / 1000;
    port->stats->duration_nsec = ((time_msec() - port->created) % 1000) * 1000000;
}

void
dp_port_live_update(struct sw_port *p) {

  if((p->conf->state & OFPPS_LINK_DOWN)
     || (p->conf->config & OFPPC_PORT_DOWN)) {
      /* Port not live */
      p->conf->state &= ~OFPPS_LIVE;
  } else {
      /* Port is live */
      p->conf->state |= OFPPS_LIVE;
  }
}

ofl_err
dp_ports_handle_stats_request_port(struct datapath *dp,
                                  struct ofl_msg_multipart_request_port *msg,
                                  const struct sender *sender UNUSED) {
    struct sw_port *port;

    struct ofl_msg_multipart_reply_port reply =
            {{{.type = OFPT_MULTIPART_REPLY},
              .type = OFPMP_PORT_STATS, .flags = 0x0000},
             .stats_num   = 0,
             .stats       = NULL};

    if (msg->port_no == OFPP_ANY) {
        size_t i = 0;

        reply.stats_num = dp->ports_num;
        reply.stats     = xmalloc(sizeof(struct ofl_port_stats *) * dp->ports_num);

        LIST_FOR_EACH(port, struct sw_port, node, &dp->port_list) {
            dp_port_stats_update(port);
            reply.stats[i] = port->stats;
            i++;
        }

    } else {
        port = dp_ports_lookup(dp, msg->port_no);

        if (port != NULL && port->netdev != NULL) {
            reply.stats_num = 1;
            reply.stats = xmalloc(sizeof(struct ofl_port_stats *));
            dp_port_stats_update(port);
            reply.stats[0] = port->stats;
        }
    }

    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);

    free(reply.stats);
    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);

    return 0;
}

ofl_err
dp_ports_handle_port_desc_request(struct datapath *dp,
                                  struct ofl_msg_multipart_request_header *msg UNUSED,
                                  const struct sender *sender UNUSED){
    struct sw_port *port;
    size_t i = 0;


    struct ofl_msg_multipart_reply_port_desc reply =
            {{{.type = OFPT_MULTIPART_REPLY},
             .type = OFPMP_PORT_DESC, .flags = 0x0000},
             .stats_num   = 0,
             .stats       = NULL};

    reply.stats_num = dp->ports_num;
    reply.stats     = xmalloc(sizeof(struct ofl_port *) * dp->ports_num);

    LIST_FOR_EACH(port, struct sw_port, node, &dp->port_list) {
        reply.stats[i] = port->conf;
        i++;
    }

    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);

    free(reply.stats);
    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);

    return 0;
}

static void
dp_ports_queue_update(struct sw_queue *queue) {
    queue->stats->duration_sec  =  (time_msec() - queue->created) / 1000;
    queue->stats->duration_nsec = ((time_msec() - queue->created) % 1000) * 1000000;
}

ofl_err
dp_ports_handle_stats_request_queue(struct datapath *dp,
                                  struct ofl_msg_multipart_request_queue *msg,
                                  const struct sender *sender) {
    struct sw_port *port;

    struct ofl_msg_multipart_reply_queue reply =
            {{{.type = OFPT_MULTIPART_REPLY},
              .type = OFPMP_QUEUE, .flags = 0x0000},
             .stats_num   = 0,
             .stats       = NULL};

    if (msg->port_no == OFPP_ANY) {
        size_t i,idx = 0, num = 0;

        LIST_FOR_EACH(port, struct sw_port, node, &dp->port_list) {
            if (msg->queue_id == OFPQ_ALL) {
                num += port->num_queues;
            } else {
                if (msg->queue_id < port->max_queues) {
                    if (port->queues[msg->queue_id].port != NULL) {
                        num++;
                    }
                }
            }
        }

        reply.stats_num = num;
        reply.stats     = xmalloc(sizeof(struct ofl_port_stats *) * num);

        LIST_FOR_EACH(port, struct sw_port, node, &dp->port_list) {
            if (msg->queue_id == OFPQ_ALL) {
                for(i=0; i<port->max_queues; i++) {
                    if (port->queues[i].port != NULL) {
                        dp_ports_queue_update(&port->queues[i]);
                        reply.stats[idx] = port->queues[i].stats;
                        idx++;
                    }
                }
            } else {
                if (msg->queue_id < port->max_queues) {
                    if (port->queues[msg->queue_id].port != NULL) {
                        dp_ports_queue_update(&port->queues[msg->queue_id]);
                        reply.stats[idx] = port->queues[msg->queue_id].stats;
                        idx++;
                    }
                }
            }
        }

    } else {
        port = dp_ports_lookup(dp, msg->port_no);

        if (port != NULL && port->netdev != NULL) {
            size_t i, idx = 0;

            if (msg->queue_id == OFPQ_ALL) {
                reply.stats_num = port->num_queues;
                reply.stats = xmalloc(sizeof(struct ofl_port_stats *) * port->num_queues);

                for(i=0; i<port->max_queues; i++) {
                    if (port->queues[i].port != NULL) {
                        dp_ports_queue_update(&port->queues[i]);
                        reply.stats[idx] = port->queues[i].stats;
                        idx++;
                    }
                }
            } else {
                if (msg->queue_id < port->max_queues) {
                    if (port->queues[msg->queue_id].port != NULL) {
                        reply.stats_num = 1;
                        reply.stats = xmalloc(sizeof(struct ofl_port_stats *));
                        dp_ports_queue_update(&port->queues[msg->queue_id]);
                        reply.stats[0] = port->queues[msg->queue_id].stats;
                    }
                }
            }
        }
    }

    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);

    free(reply.stats);
    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);

    return 0;
}

ofl_err
dp_ports_handle_queue_get_config_request(struct datapath *dp,
                              struct ofl_msg_queue_get_config_request *msg,
                                                const struct sender *sender) {
    struct sw_port *p;

    struct ofl_msg_queue_get_config_reply reply =
            {{.type = OFPT_QUEUE_GET_CONFIG_REPLY},
             .queues = NULL};

    if (msg->port == OFPP_ANY) {
        size_t i, idx = 0, num = 0;

        LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list) {
            num += p->num_queues;
        }

        reply.port       = OFPP_ANY;
        reply.queues_num = num;
        reply.queues     = xmalloc(sizeof(struct ofl_packet_queue *) * num);

        LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list) {
            for (i=0; i<p->max_queues; i++) {
                if (p->queues[i].port != NULL) {
                    reply.queues[idx] = p->queues[i].props;
                    idx++;
                }
             }
         }
    } else {
        p = dp_ports_lookup(dp, msg->port);

        if (p == NULL || (p->stats->port_no != msg->port)) {
            free(reply.queues);
            ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
            return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_PORT);
        } else {
            size_t i, idx = 0;

            reply.port       = msg->port;
            reply.queues_num = p->num_queues;
            reply.queues     = xmalloc(sizeof(struct ofl_packet_queue *) * p->num_queues);

            for (i=0; i<p->max_queues; i++) {
                if (p->queues[i].port != NULL) {
                    reply.queues[idx] = p->queues[i].props;
                    idx++;
                }
            }
        }
    }

    dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);

    free(reply.queues);
    ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);
    return 0;
}
