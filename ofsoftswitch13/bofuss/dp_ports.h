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

#ifndef DP_PORTS_H
#define DP_PORTS_H 1

#include <bofuss/list.h>
#include <bofuss/util.h>
#include <bofuss/openflow.h>
#include <bofuss/ofl-err.h>
#include <bofuss/ofl-messages.h>

struct sender;
struct netdev;

/****************************************************************************
 * Datapath port related functions.
 ****************************************************************************/
struct sw_queue {
    struct sw_port *port; /* reference to the parent port */
    uint16_t class_id; /* internal mapping from OF queue_id to tc class_id */
    uint64_t created;
    struct ofl_queue_stats *stats;
    struct ofl_packet_queue *props;
};


#define MAX_HW_NAME_LEN 32
enum sw_port_flags {
    SWP_USED             = 1 << 0,    /* Is port being used */
    SWP_HW_DRV_PORT      = 1 << 1,    /* Port controlled by HW driver */
};

#define IS_HW_PORT(p) 0

#define PORT_IN_USE(p) (((p) != NULL) && (p)->flags & SWP_USED)

#define PORT_MAX_QUEUES 32

struct sw_port {
    struct list node; /* Element in datapath.ports. */

    uint32_t flags;             /* SWP_* flags above */
    struct datapath *dp;
    struct netdev *netdev;
    struct ofl_port *conf;
    struct ofl_port_stats *stats;
    /* port queues */
    uint16_t max_queues;
    uint16_t num_queues;
    uint64_t created;
    struct sw_queue queues[PORT_MAX_QUEUES];
};


#define DP_MAX_PORTS 4096
BUILD_ASSERT_DECL(DP_MAX_PORTS <= OFPP_MAX);

/* Returns the speed value in kbps of the highest bit set in the bitfield. */
uint32_t
port_speed(uint32_t conf);

/* Returns the given port. */
struct sw_port *
dp_ports_lookup(struct datapath *, uint32_t);

/* Returns the given queue of the given port. */
struct sw_queue *
dp_ports_lookup_queue(struct sw_port *, uint32_t);


/* Handles a port mod message. */
ofl_err
dp_ports_handle_port_mod(struct datapath *dp, struct ofl_msg_port_mod *msg,
                                               const struct sender *sender);

/* Update Live flag on a port/ */
void
dp_port_live_update(struct sw_port *port);

/* Handles a port stats request message. */
ofl_err
dp_ports_handle_stats_request_port(struct datapath *dp,
                                  struct ofl_msg_multipart_request_port *msg,
                                  const struct sender *sender);
                                  
/* Handles a port desc request message. */
ofl_err
dp_ports_handle_port_desc_request(struct datapath *dp,
                                  struct ofl_msg_multipart_request_header *msg UNUSED,
                                  const struct sender *sender UNUSED);

/* Handles a queue stats request message. */
ofl_err
dp_ports_handle_stats_request_queue(struct datapath *dp,
                                    struct ofl_msg_multipart_request_queue *msg,
                                    const struct sender *sender);

/* Handles a queue get config request message. */
ofl_err
dp_ports_handle_queue_get_config_request(struct datapath *dp,
                                         struct ofl_msg_queue_get_config_request *msg,
                                         const struct sender *sender);


#endif /* DP_PORTS_H */
