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
#include <bofuss/ipv6-util.h>

const struct in6_addr in6addr_exact = IN6ADDR_EXACT_INIT;

const struct in6_addr in6addr_zero = IN6ADDR_ZERO_INIT;

/* Translates 'host_name', which must be a string representation of an IPv6
 * address, into a numeric IPv6 address in '*addr'.  Returns 0 if successful,
 * otherwise a positive errno value. */
static int
lookup_ipv6(const char *host_name, struct in6_addr *addr)
{
    if (inet_pton(AF_INET6, host_name, addr) != 1) {
        printf("%s is not a valid IPv6 address \n", host_name);
        return ENOENT;
    }
    return 0;
}

/* Returns an in6_addr consisting of 'mask' high-order 1-bits and 128-N
 * low-order 0-bits. */

static struct in6_addr
ipv6_create_mask(int mask)
{
    struct in6_addr netmask;
    uint8_t *netmaskp = &netmask.s6_addr[0];

    memset(&netmask, 0, sizeof netmask);
    while (mask > 8) {
        *netmaskp = 0xff;
        netmaskp++;
        mask -= 8;
    }

    if (mask) {
        *netmaskp = 0xff << (8 - mask);
    }

    return netmask;
}

static
struct in6_addr ipv6_addr_bitand(const struct in6_addr *a,
                                 const struct in6_addr *b)
{
    int i;
    struct in6_addr dst;

#ifdef s6_addr32
    for (i=0; i<4; i++) {
        dst.s6_addr32[i] = a->s6_addr32[i] & b->s6_addr32[i];
    }
#else
    for (i=0; i<16; i++) {
        dst.s6_addr[i] = a->s6_addr[i] & b->s6_addr[i];
    }
#endif

    return dst;
}


int
str_to_ipv6(const char *str_, struct in6_addr *addrp, struct in6_addr *maskp)
{

    char *str = xstrdup(str_);
    char *save_ptr = NULL;
    const char *name, *netmask;
    struct in6_addr addr;
    int retval;

    name = strtok_r(str, "/", &save_ptr);
    retval = name ? lookup_ipv6(name, &addr) : EINVAL;
    if (retval) {
        printf("%s: could not convert to IPv6 address\n", str);
        return -1;
    }
    
    netmask = strtok_r(NULL, "/", &save_ptr);
    if (netmask) {
        int prefix = atoi(netmask);
        if (prefix <= 0 || prefix > 128) {
            printf("%s: network prefix bits not between 1 and 128\n", str);
            return -1;
        } else {
            *maskp = ipv6_create_mask(prefix);
            *addrp = ipv6_addr_bitand(&addr, &in6addr_exact);

        }
    } else {
        *maskp = in6addr_zero ;
        *addrp = ipv6_addr_bitand(&addr, &in6addr_exact);
    }

    free(str);
    return 1;
}

      

