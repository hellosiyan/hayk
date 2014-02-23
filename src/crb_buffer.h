#ifndef __CRB_BUFFER_H__
#define __CRB_BUFFER_H__ 1

#include <unistd.h>


typedef struct crb_buffer_s crb_buffer_t;

struct crb_buffer_s {
	char *ptr;
	char *rpos;
	
	size_t used;
	size_t size;
};

crb_buffer_t *crb_buffer_init(size_t size);
crb_buffer_t *crb_buffer_copy(crb_buffer_t *orig);
char *crb_buffer_copy_string(crb_buffer_t *buffer);
char *crb_buffer_copy_string_without_sentinel(crb_buffer_t *buffer, size_t *size);
int crb_buffer_append_string(crb_buffer_t *buffer, const char *str, size_t str_len);
void crb_buffer_trim_left(crb_buffer_t *buffer);
void crb_buffer_clear(crb_buffer_t *buffer);
void crb_buffer_free(crb_buffer_t *buffer);

#endif /* __CRB_BUFFER_H__ */
