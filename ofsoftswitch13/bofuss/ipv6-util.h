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
#ifndef IPV6_UTIL_H
#define IPV6_UTIL_H 1

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <bofuss/util.h>

#define IN6ADDR_EXACT_INIT { { { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
                                 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff } } }
                                  
#define IN6ADDR_ZERO_INIT { { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, \
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } } }
                                  
extern const struct in6_addr in6addr_exact;

static inline bool
ipv6_addr_equals(const struct in6_addr *a, const struct in6_addr *b) {
#ifdef IN6_ARE_ADDR_EQUAL
    return IN6_ARE_ADDR_EQUAL(a, b);
#else
    return !memcmp(a, b, sizeof(*a));
#endif
}

static inline bool
ipv6_mask_is_exact(const struct in6_addr *mask) {
    return ipv6_addr_equals(mask, &in6addr_exact);
}

int
str_to_ipv6(const char *str_, struct in6_addr *addrp, struct in6_addr *maskp);

#endif
