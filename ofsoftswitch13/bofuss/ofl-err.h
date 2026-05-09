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

#ifndef OFL_H
#define OFL_H 1

#include <sys/types.h>
#include <stdint.h>

/* ofl_err is used to return OpenFlow error type/code's from functions.
 * See ofl_error function for details.
 */
typedef uint32_t ofl_err;

/* OFL_ERROR should be returned as ofl_err, when there was an error, but
 * there is no appropriate OpenFlow error type defined, or when no error
 * message should be generated because of the error. */
#define OFL_ERROR 0xffffffff

/* Creates an ofl_err from an OpenFlow error type and code */
static inline ofl_err
ofl_error(uint16_t type, uint16_t code) {
    /* NOTE: highest bit is always set to one, so no error value is zero */
    uint32_t ret = type;
    return 0x80000000 | ret << 16 | code;
}

/* Returns the error type of an ofl_err */
static inline uint16_t
ofl_error_type(ofl_err error) {
    return (0x7fff0000 & error) >> 16;
}

/* Returns the error code of an ofl_err */
static inline uint16_t
ofl_error_code(ofl_err error) {
    return error & 0x0000ffff;
}



#endif /* OFL_H 1 */
