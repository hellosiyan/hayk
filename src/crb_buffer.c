#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crb_buffer.h"

#define CRB_BUFFER_PIECE_SIZE 2048

crb_buffer_t *
crb_buffer_init(size_t size)
{
	crb_buffer_t *buffer;
	
	buffer = malloc(sizeof(crb_buffer_t));
	if ( buffer == NULL ) {
		return NULL;
	}
	
	buffer->used = 0;
	buffer->size = size;
	
	buffer->ptr = malloc(size);
	if ( buffer->ptr == NULL ) {
		free(buffer);
		return NULL;
	}
	
	buffer->ptr[0] = '\0';
	
	return buffer;
}


int 
crb_buffer_append_string(crb_buffer_t *buffer, const char *str, size_t str_len)
{
	char *new_ptr;
	if ( !str ) {
		return -1;
	} else if ( str_len == 0 ) {
		return 0;
	}
	
	if ( buffer->used + str_len > buffer->size  ) {
		buffer->size = buffer->used + str_len;
		buffer->size += CRB_BUFFER_PIECE_SIZE - (buffer->size%CRB_BUFFER_PIECE_SIZE);
		new_ptr = realloc(buffer->ptr, buffer->size);
		if ( new_ptr == NULL ) {
			return -1;
		}
		buffer->ptr = new_ptr;
	}
	
	if (buffer->used == 0) {
		buffer->used++;
	}
	
	memcpy(buffer->ptr + buffer->used - 1, str, str_len);
	
	buffer->used += str_len;
	buffer->ptr[buffer->used - 1] = '\0';
	
	return 0;
}

void 
crb_buffer_clear(crb_buffer_t *buffer)
{
	buffer->used = 0;
}

void 
crb_buffer_free(crb_buffer_t *buffer)
{
	free(buffer->ptr);
	buffer->ptr = NULL;
	buffer->size = 0;
	buffer->used = 0;
}


