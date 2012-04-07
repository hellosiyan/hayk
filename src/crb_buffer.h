#ifndef __CRB_BUFFER_H__
#define __CRB_BUFFER_H__ 1

#include <unistd.h>


typedef struct crb_buffer_s crb_buffer_t;

struct crb_buffer_s {
	char *ptr;
	
	size_t used;
	size_t size;
};

crb_buffer_t *crb_buffer_init(size_t size);
int crb_buffer_append_string(crb_buffer_t *b, const char *s, size_t s_len);

#endif /* __CRB_BUFFER_H__ */
