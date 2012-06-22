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
	
	buffer->rpos = buffer->ptr;
	
	return buffer;
}

crb_buffer_t *
crb_buffer_copy(crb_buffer_t *orig)
{
	crb_buffer_t *buffer;
	
	buffer = malloc(sizeof(crb_buffer_t));
	if ( buffer == NULL ) {
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
	
	memcpy(buffer->ptr, orig->ptr, orig->size);
	buffer->ptr[buffer->used - 1] = '\0';
	
	return buffer;
}

char *
crb_buffer_copy_string(crb_buffer_t *buffer)
{
	char *string;
	
	string = malloc(buffer->used+1);
	if ( string == NULL ) {
		return NULL;
	}
	
	memcpy(string, buffer->ptr, buffer->used);
	string[buffer->used - 1] = '\0';
	
	return string;
}


int 
crb_buffer_append_string(crb_buffer_t *buffer, const char *str, size_t str_len)
{
	char *new_ptr;
	int rpos_offset;
	if ( !str ) {
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
crb_buffer_trim_left(crb_buffer_t *buffer)
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
crb_buffer_clear(crb_buffer_t *buffer)
{
	buffer->used = 0;
	buffer->rpos = buffer->ptr;
}

void 
crb_buffer_free(crb_buffer_t *buffer)
{
	free(buffer->ptr);
	buffer->ptr = NULL;
	buffer->size = 0;
	buffer->used = 0;
	free(buffer);
}


