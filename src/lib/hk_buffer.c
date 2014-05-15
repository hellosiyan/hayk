#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hk_buffer.h"

#define CRB_BUFFER_PIECE_SIZE 2048

hk_buffer_t *
hk_buffer_init(size_t size)
{
	hk_buffer_t *buffer;
	
	buffer = malloc(sizeof(hk_buffer_t));
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
	
	buffer->rpos = buffer->ptr;
	
	return buffer;
}

hk_buffer_t *
hk_buffer_copy(hk_buffer_t *orig)
{
	hk_buffer_t *buffer;
	
	buffer = malloc(sizeof(hk_buffer_t));
	if ( buffer == NULL || orig == NULL ) {
		return NULL;
	}
	
	buffer->used = orig->used;
	buffer->size = orig->size;
	buffer->size += CRB_BUFFER_PIECE_SIZE - (buffer->size%CRB_BUFFER_PIECE_SIZE);
	
	buffer->ptr = malloc(buffer->size);
	if ( buffer->ptr == NULL ) {
		free(buffer);
		return NULL;
	}
	
	buffer->ptr = memcpy(buffer->ptr, orig->ptr, orig->size);
	buffer->ptr[buffer->used - 1] = '\0';
	
	return buffer;
}

char *
hk_buffer_copy_string(hk_buffer_t *buffer)
{
	char *string;
	
	string = malloc(buffer->used+1);
	if ( string == NULL ) {
		return NULL;
	}
	
	string = memcpy(string, buffer->ptr, buffer->used);
	string[buffer->used - 1] = '\0';
	
	return string;
}

char *
hk_buffer_copy_string_without_sentinel(hk_buffer_t *buffer, size_t *size)
{
	char *string;

	if ( buffer->used < 1 ) {
		return NULL;
	}
	
	string = malloc(buffer->used-1);
	if ( string == NULL ) {
		return NULL;
	}
	
	string = memcpy(string, buffer->ptr, buffer->used-1);

	if ( size != NULL ) {
		*size = buffer->used-1;
	}
	
	return string;
}


int 
hk_buffer_append_string(hk_buffer_t *buffer, const char *str, size_t str_len)
{
	char *new_ptr;
	int rpos_offset;
	
	if ( buffer == NULL || str == NULL ) {
		return -1;
	} else if ( str_len == 0 ) {
		return 0;
	}
	
	if ( str_len == -1 ) {
		str_len = strlen(str);
	}
	
	if ( buffer->used + str_len > buffer->size  ) {
		buffer->size = buffer->used + str_len;
		buffer->size += CRB_BUFFER_PIECE_SIZE - (buffer->size%CRB_BUFFER_PIECE_SIZE);
		
		rpos_offset = buffer->rpos - buffer->ptr;
		
		new_ptr = realloc(buffer->ptr, buffer->size);
		if ( new_ptr == NULL ) {
			printf("cannot alloc!\n");
			return -1;
		}
		
		buffer->ptr = new_ptr;
		buffer->rpos = buffer->ptr + rpos_offset;
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
hk_buffer_trim_left(hk_buffer_t *buffer)
{
	int size;
	if ( buffer->ptr >= buffer->rpos ) {
		// Nothing to trim
		return;
	}
	
	size = buffer->used - (buffer->rpos - buffer->ptr);
	if ( size <= 0 ) {
		return;
	}
	
	buffer->ptr = memmove(buffer->ptr, buffer->rpos, size);
	buffer->rpos = buffer->ptr;
	buffer->used = size;
}

void 
hk_buffer_clear(hk_buffer_t *buffer)
{
	buffer->used = 0;
	buffer->rpos = buffer->ptr;
}

void 
hk_buffer_free(hk_buffer_t *buffer)
{
	free(buffer->ptr);
	buffer->ptr = NULL;
	buffer->size = 0;
	buffer->used = 0;
	free(buffer);
}


