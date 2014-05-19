#ifndef __CRB_REQUEST_H__
#define __CRB_REQUEST_H__ 1

#include <stdint.h>

#include "hk_hash.h"

typedef struct hk_http_header_s hk_http_header_t;
struct hk_http_header_s {
    char *name;
    char *value;
};

typedef struct hk_http_request_s hk_http_request_t;
struct hk_http_request_s {
    char *uri;
    hk_hash_t *headers;
	
	uint32_t ref;
};

hk_http_header_t *hk_header_init();
void hk_header_free(hk_http_header_t *header);

hk_http_request_t *hk_request_init();
void hk_request_ref(hk_http_request_t *request);
void hk_request_unref(hk_http_request_t *request);

void hk_request_set_uri(hk_http_request_t *request, char *uri, size_t length);
void hk_request_add_header(hk_http_request_t *request, char *name, size_t name_length, char *value, size_t value_length);
char *hk_request_get_headers_string(hk_http_request_t *request, int *size);
hk_http_header_t *hk_request_get_header(hk_http_request_t *request, char *name, size_t name_length);

#endif /* __CRB_REQUEST_H__ */
