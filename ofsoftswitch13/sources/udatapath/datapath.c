/* Copyright (c) 2008, 2009 The Board of Trustees of The Leland Stanford
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

/*
 * The original Stanford code has been modified during the implementation of
 * the OpenFlow 1.3 userspace switch.
 */

#include <bofuss/datapath.h>
#include <bofuss/ofpbuf.h>
#include <bofuss/util.h>
#include <bofuss/vlog.h>

#if defined(__GNUC__)
// Define send_openflow_buffer_to_remote functions as weak,
// so ns3 can override it and send the buffer over simulated channel.
#pragma weak send_openflow_buffer_to_remote
#endif

#define LOG_MODULE VLM_dp

#define MAIN_CONNECTION 0
#define PTIN_CONNECTION 1

struct remote *
remote_create(struct datapath *dp)
{
    size_t i;
    struct remote *remote = xmalloc(sizeof *remote);
    list_push_back(&dp->remotes, &remote->node);
    remote->dp = dp;
    remote->cb_dump = NULL;
    remote->mp_req_msg = NULL;
    remote->mp_req_xid = 0;
    remote->role = OFPCR_ROLE_EQUAL;
    /* Set the remote configuration to receive any asynchronous message*/
    for (i = 0; i < 2; i++)
    {
        memset(&remote->config.packet_in_mask[i], 0x7, sizeof(uint32_t));
        memset(&remote->config.port_status_mask[i], 0x7, sizeof(uint32_t));
        memset(&remote->config.flow_removed_mask[i], 0x1f, sizeof(uint32_t));
    }
    return remote;
}

void dp_set_dpid(struct datapath *dp, uint64_t dpid)
{
    dp->id = dpid;
}

void dp_set_mfr_desc(struct datapath *dp, char *mfr_desc)
{
    strncpy(dp->mfr_desc, mfr_desc, DESC_STR_LEN);
    dp->mfr_desc[DESC_STR_LEN - 1] = 0x00;
}

void dp_set_hw_desc(struct datapath *dp, char *hw_desc)
{
    strncpy(dp->hw_desc, hw_desc, DESC_STR_LEN);
    dp->hw_desc[DESC_STR_LEN - 1] = 0x00;
}

void dp_set_sw_desc(struct datapath *dp, char *sw_desc)
{
    strncpy(dp->sw_desc, sw_desc, DESC_STR_LEN);
    dp->sw_desc[DESC_STR_LEN - 1] = 0x00;
}

void dp_set_dp_desc(struct datapath *dp, char *dp_desc)
{
    strncpy(dp->dp_desc, dp_desc, DESC_STR_LEN);
    dp->dp_desc[DESC_STR_LEN - 1] = 0x00;
}

void dp_set_serial_num(struct datapath *dp, char *serial_num)
{
    strncpy(dp->serial_num, serial_num, SERIAL_NUM_LEN);
    dp->serial_num[SERIAL_NUM_LEN - 1] = 0x00;
}

void dp_set_max_queues(struct datapath *dp, uint32_t max_queues)
{
    dp->max_queues = max_queues;
}

int send_openflow_buffer_to_remote(struct ofpbuf *buffer UNUSED,
                                   struct remote *remote UNUSED)
{
    NOT_IMPLEMENTED();
    return 0;
}

static int
send_openflow_buffer(struct datapath *dp, struct ofpbuf *buffer,
                     const struct sender *sender)
{

    // Update ofbuf length
    struct ofp_header *oh = ofpbuf_at_assert(buffer, 0, sizeof *oh);
    oh->length = htons(buffer->size);

    if (sender)
    {
        /* Send back to the sender. */
        return send_openflow_buffer_to_remote(buffer, sender->remote);
    }
    else
    {
        /* Broadcast to all remotes. */
        struct remote *r, *prev = NULL;
        uint8_t msg_type;
        /* Get the type of the message */
        memcpy(&msg_type, ((char *)buffer->data) + 1, sizeof(uint8_t));
        LIST_FOR_EACH(r, struct remote, node, &dp->remotes)
        {
            /* do not send to remotes with slave role apart from port status */
            if (r->role == OFPCR_ROLE_EQUAL || r->role == OFPCR_ROLE_MASTER)
            {
                /*Check if the message is enabled in the asynchronous configuration*/
                switch (msg_type)
                {
                case (OFPT_PACKET_IN):
                {
                    struct ofp_packet_in *p = (struct ofp_packet_in *)buffer->data;
                    /* Do not send message if the reason is not enabled */
                    if ((p->reason == OFPR_NO_MATCH) && !(r->config.packet_in_mask[0] & 0x1))
                        continue;
                    if ((p->reason == OFPR_ACTION) && !(r->config.packet_in_mask[0] & 0x2))
                        continue;
                    if ((p->reason == OFPR_INVALID_TTL) && !(r->config.packet_in_mask[0] & 0x4))
                        continue;
                    break;
                }
                case (OFPT_PORT_STATUS):
                {
                    struct ofp_port_status *p = (struct ofp_port_status *)buffer->data;
                    if ((p->reason == OFPPR_ADD) && !(r->config.port_status_mask[0] & 0x1))
                        continue;
                    if ((p->reason == OFPPR_DELETE) && !(r->config.port_status_mask[0] & 0x2))
                        continue;
                    if ((p->reason == OFPPR_MODIFY) && !(r->config.port_status_mask[0] & 0x4))
                        continue;
                    break;
                }
                case (OFPT_FLOW_REMOVED):
                {
                    struct ofp_flow_removed *p = (struct ofp_flow_removed *)buffer->data;
                    if ((p->reason == OFPRR_IDLE_TIMEOUT) && !(r->config.flow_removed_mask[0] & 0x1))
                        continue;
                    if ((p->reason == OFPRR_HARD_TIMEOUT) && !(r->config.flow_removed_mask[0] & 0x2))
                        continue;
                    if ((p->reason == OFPRR_DELETE) && !(r->config.flow_removed_mask[0] & 0x4))
                        continue;
                    if ((p->reason == OFPRR_GROUP_DELETE) && !(r->config.flow_removed_mask[0] & 0x8))
                        continue;
                    if ((p->reason == OFPRR_METER_DELETE) && !(r->config.flow_removed_mask[0] & 0x10))
                        continue;
                    break;
                }
                }
            }
            else
            {
                /* In this implementation we assume that a controller with role slave
                   can is able to receive only port stats messages */
                if (r->role == OFPCR_ROLE_SLAVE && msg_type != OFPT_PORT_STATUS)
                {
                    continue;
                }
                else
                {
                    struct ofp_port_status *p = (struct ofp_port_status *)buffer->data;
                    if ((p->reason == OFPPR_ADD) && !(r->config.port_status_mask[1] & 0x1))
                        continue;
                    if ((p->reason == OFPPR_DELETE) && !(r->config.port_status_mask[1] & 0x2))
                        continue;
                    if ((p->reason == OFPPR_MODIFY) && !(r->config.port_status_mask[1] & 0x4))
                        continue;
                }
            }
            if (prev)
            {
                send_openflow_buffer_to_remote(ofpbuf_clone(buffer), prev);
            }
            prev = r;
        }
        if (prev)
        {
            send_openflow_buffer_to_remote(buffer, prev);
        }
        else
        {
            ofpbuf_delete(buffer);
        }
        return 0;
    }
}

int dp_send_message(struct datapath *dp, struct ofl_msg_header *msg,
                    const struct sender *sender)
{
    struct ofpbuf *ofpbuf;
    uint8_t *buf;
    size_t buf_size;
    int error;

    if (VLOG_IS_DBG_ENABLED(LOG_MODULE))
    {
        char *msg_str = ofl_msg_to_string(msg, dp->exp);
        VLOG_DBG(LOG_MODULE, "Datapath \"%" PRIu64 "\" sending: %.400s", dp->id, msg_str);
        free(msg_str);
    }

    error = ofl_msg_pack(msg, sender == NULL ? 0 : sender->xid, &buf, &buf_size, dp->exp);
    if (error)
    {
        VLOG_WARN(LOG_MODULE, "There was an error packing the message!");
        return error;
    }
    ofpbuf = ofpbuf_new(0);
    ofpbuf_use(ofpbuf, buf, buf_size);
    ofpbuf_put_uninit(ofpbuf, buf_size);

    /* Choose the connection to send the packet to.
       1) By default, we send it to the main connection
       2) If there's an associated sender, send the response to the same
          connection the request came from
       3) If it's a packet in, use the auxiliary connection
    */
    ofpbuf->conn_id = MAIN_CONNECTION;
    if (sender != NULL)
        ofpbuf->conn_id = sender->conn_id;
    if (msg->type == OFPT_PACKET_IN)
        ofpbuf->conn_id = PTIN_CONNECTION;

    error = send_openflow_buffer(dp, ofpbuf, sender);
    if (error)
    {
        VLOG_WARN(LOG_MODULE, "There was an error sending the message!");
        return error;
    }
    return 0;
}

static ofl_err
dp_check_generation_id(struct datapath *dp, uint64_t new_gen_id)
{

    if (dp->generation_id >= 0 && ((int64_t)(new_gen_id - dp->generation_id) < 0))
    {
        return ofl_error(OFPET_ROLE_REQUEST_FAILED, OFPRRFC_STALE);
    }
    else
        dp->generation_id = new_gen_id;
    return 0;
}

ofl_err
dp_handle_role_request(struct datapath *dp, struct ofl_msg_role_request *msg,
                       const struct sender *sender)
{
    uint32_t role = msg->role;
    uint64_t generation_id = msg->generation_id;
    switch (msg->role)
    {
    case OFPCR_ROLE_NOCHANGE:
    {
        role = sender->remote->role;
        generation_id = dp->generation_id;
        break;
    }
    case OFPCR_ROLE_EQUAL:
    {
        sender->remote->role = OFPCR_ROLE_EQUAL;
        break;
    }
    case OFPCR_ROLE_MASTER:
    {
        struct remote *r;
        int error = dp_check_generation_id(dp, msg->generation_id);
        if (error)
        {
            VLOG_WARN(LOG_MODULE, "Role message generation id is smaller than the current id!");
            return error;
        }
        /* Old master(s) must be changed to slave(s) */
        LIST_FOR_EACH(r, struct remote, node, &dp->remotes)
        {
            if (r->role == OFPCR_ROLE_MASTER)
            {
                r->role = OFPCR_ROLE_SLAVE;
            }
        }
        sender->remote->role = OFPCR_ROLE_MASTER;
        break;
    }
    case OFPCR_ROLE_SLAVE:
    {
        int error = dp_check_generation_id(dp, msg->generation_id);
        if (error)
        {
            VLOG_WARN(LOG_MODULE, "Role message generation id is smaller than the current id!");
            return error;
        }
        sender->remote->role = OFPCR_ROLE_SLAVE;
        break;
    }
    default:
    {
        VLOG_WARN(LOG_MODULE, "Role request with unknown role (%u).", msg->role);
        return ofl_error(OFPET_ROLE_REQUEST_FAILED, OFPRRFC_BAD_ROLE);
    }
    }

    {
        struct ofl_msg_role_request reply =
            {{.type = OFPT_ROLE_REPLY},
             .role = role,
             .generation_id = generation_id};

        dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);
    }
    return 0;
}

ofl_err
dp_handle_async_request(struct datapath *dp, struct ofl_msg_async_config *msg,
                        const struct sender *sender)
{

    uint16_t async_type = msg->header.type;
    switch (async_type)
    {
    case (OFPT_GET_ASYNC_REQUEST):
    {
        struct ofl_msg_async_config reply =
            {{.type = OFPT_GET_ASYNC_REPLY},
             .config = &sender->remote->config};
        dp_send_message(dp, (struct ofl_msg_header *)&reply, sender);

        ofl_msg_free((struct ofl_msg_header *)msg, dp->exp);

        break;
    }
    case (OFPT_SET_ASYNC):
    {
        memcpy(&sender->remote->config, msg->config, sizeof(struct ofl_async_config));
        break;
    }
    }
    return 0;
}
