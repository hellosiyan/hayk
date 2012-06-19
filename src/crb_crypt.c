#include <unistd.h>
#include <stdlib.h>
#include <openssl/sha.h>

#include "crb_crypt.h"

int
crb_encode_base64(u_char *dst, u_char *src, int src_length)
{
    u_char *d, *s;
    size_t len;
    static u_char basis64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    len = src_length;
    s = src;
    d = dst;

    while (len > 2) {
        *d++ = basis64[(s[0] >> 2) & 0x3f];
        *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
        *d++ = basis64[((s[1] & 0x0f) << 2) | (s[2] >> 6)];
        *d++ = basis64[s[2] & 0x3f];

        s += 3;
        len -= 3;
    }

    if (len) {
        *d++ = basis64[(s[0] >> 2) & 0x3f];

        if (len == 1) {
            *d++ = basis64[(s[0] & 3) << 4];
            *d++ = '=';

        } else {
            *d++ = basis64[((s[0] & 3) << 4) | (s[1] >> 4)];
            *d++ = basis64[(s[1] & 0x0f) << 2];
        }

        *d++ = '=';
    }

    return d - dst;
}
