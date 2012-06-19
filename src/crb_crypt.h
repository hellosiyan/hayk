#ifndef __CRB_CRYPT_H__
#define __CRB_CRYPT_H__ 1

#include <unistd.h>
#include <stdlib.h>
#include <openssl/sha.h>

int crb_encode_base64(u_char *dst, u_char *src, int src_length);

#endif /* __CRB_CRYPT_H__ */
