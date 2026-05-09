/* Copyright (c) 2011, TrafficLab, Ericsson Research, Hungary
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
 *
 *
 * Author: Zoltán Lajos Kis <zoltan.lajos.kis@ericsson.com>
 */
#include <stdlib.h>
#include <string.h>

#include <bofuss/dp_exp.h>
#include <bofuss/openflow.h>
#include <bofuss/vlog.h>
#include <bofuss/ofl-actions.h>
#include <bofuss/ofl-exp.h>
#include <bofuss/datapath.h>
#include <bofuss/packet.h>

#define LOG_MODULE VLM_dp_exp

void
dp_exp_action(struct packet * pkt UNUSED, struct ofl_action_experimenter *act) {
	VLOG_WARN(LOG_MODULE, "Trying to execute unknown experimenter action (%u).", act->experimenter_id);
}

void
dp_exp_inst(struct packet *pkt UNUSED, struct ofl_instruction_experimenter *inst) {
	VLOG_WARN(LOG_MODULE, "Trying to execute unknown experimenter instruction (%u).", inst->experimenter_id);
}

ofl_err
dp_exp_stats(struct datapath *dp UNUSED,
             struct ofl_msg_multipart_request_experimenter *msg,
             const struct sender *sender UNUSED) {
	VLOG_WARN(LOG_MODULE, "Trying to handle unknown experimenter stats (%u).", msg->experimenter_id);
    return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
}

ofl_err
dp_exp_message(struct datapath *dp UNUSED,
               struct ofl_msg_experimenter *msg,
               const struct sender *sender UNUSED) {
    VLOG_WARN(LOG_MODULE, "Trying to handle unknown experimenter message (%u).", msg->experimenter_id);
    return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
}
