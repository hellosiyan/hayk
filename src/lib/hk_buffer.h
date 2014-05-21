#ifndef __HK_BUFFER_H__
#define __HK_BUFFER_H__ 1

#include <unistd.h>


typedef struct hk_buffer_s hk_buffer_t;

struct hk_buffer_s {
	char *ptr;
	char *rpos;
	
	size_t used;
	size_t size;
};

hk_buffer_t *hk_buffer_init(size_t size);
hk_buffer_t *hk_buffer_copy(hk_buffer_t *orig);
char *hk_buffer_copy_string(hk_buffer_t *buffer);
char *hk_buffer_copy_string_without_sentinel(hk_buffer_t *buffer, size_t *size);
int hk_buffer_append_string(hk_buffer_t *buffer, const char *str, size_t str_len);
void hk_buffer_trim_left(hk_buffer_t *buffer);
void hk_buffer_clear(hk_buffer_t *buffer);
void hk_buffer_free(hk_buffer_t *buffer);

#endif /* __HK_BUFFER_H__ */
