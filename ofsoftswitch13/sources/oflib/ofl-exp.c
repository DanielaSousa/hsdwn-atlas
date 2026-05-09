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
#include <bofuss/ofl-exp.h>
#include <bofuss/ofl-messages.h>
#include <bofuss/vlog.h>
#include <bofuss/openflow.h>

#define LOG_MODULE VLM_ofl_exp

int
ofl_exp_msg_pack(struct ofl_msg_experimenter *msg, uint8_t **buf UNUSED, size_t *buf_len UNUSED) {
    VLOG_WARN(LOG_MODULE, "Trying to pack unknown EXPERIMENTER message (%u).", msg->experimenter_id);
    return -1;
}

ofl_err
ofl_exp_msg_unpack(struct ofp_header *oh, size_t *len, struct ofl_msg_experimenter **msg UNUSED) {
    struct ofp_experimenter_header *exp;

    if (*len < sizeof(struct ofp_experimenter_header)) {
        VLOG_WARN(LOG_MODULE, "Received EXPERIMENTER message is shorter than ofp_experimenter_header.");
        return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_LEN);
    }

    exp = (struct ofp_experimenter_header *)oh;
    VLOG_WARN(LOG_MODULE, "Trying to unpack unknown EXPERIMENTER message (%u).", htonl(exp->experimenter));
            return ofl_error(OFPET_BAD_REQUEST, OFPBRC_BAD_EXPERIMENTER);
}

int
ofl_exp_msg_free(struct ofl_msg_experimenter *msg) {
    VLOG_WARN(LOG_MODULE, "Trying to free unknown EXPERIMENTER message (%u).", msg->experimenter_id);
    free(msg);
    return -1;
}

char *
ofl_exp_msg_to_string(struct ofl_msg_experimenter *msg) {
    char *str;
    size_t str_size;

    FILE *stream = open_memstream(&str, &str_size);
    VLOG_WARN(LOG_MODULE, "Trying to convert to string unknown EXPERIMENTER message (%u).", msg->experimenter_id);
    fprintf(stream, "exp{id=\"0x%"PRIx32"\"}", msg->experimenter_id);
    fclose(stream);
    return str;
}
