#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crb_atomic.h"
#include "crb_request.h"


void crb_request_free(crb_request_t *request);

crb_header_t *
crb_header_init()
{
	crb_header_t *header;
	
	header = malloc(sizeof(crb_header_t));
	if ( header == NULL ) {
		return NULL;
	}
	
	header->name = NULL;
	header->value = NULL;
	
	return header;
}

void
crb_header_free(crb_header_t *header)
{
	if ( header->name != NULL ) {
		free(header->name);
	}
	
	if ( header->value != NULL ) {
		free(header->value);
	}
	
	free(header);
}

crb_request_t *
crb_request_init()
{
	crb_request_t *request;
	
	request = malloc(sizeof(crb_request_t));
	if ( request == NULL ) {
		return NULL;
	}
	
	request->uri = NULL;
	request->headers = crb_hash_init(10);
	
	request->ref = 0;
	
	return request;
}

void
crb_request_set_uri(crb_request_t *request, char *uri, ssize_t length)
{
	if ( request->uri != NULL ) {
		free(request->uri);
	}
	
	if ( uri == NULL || length == 0 ) {
		request->uri = NULL;
		return;
	}
	
	request->uri = malloc(length+1);
	memcpy(request->uri, uri, length);
	request->uri[length] = '\0';
}

void 
crb_request_add_header(crb_request_t *request, char *name, ssize_t name_length, char *value, ssize_t value_length)
{
	crb_header_t *header = crb_header_init();
	
	if ( name == NULL || value == NULL ) {
		return;
	}
	
	header->value = malloc(value_length+1);
	header->value = memcpy(header->value, value, value_length);
	header->value[value_length] = '\0';
	
	header->name = malloc(name_length+1);
	header->name = memcpy(header->name, name, name_length);
	header->name[name_length] = '\0';
	
	// printf("ADD:\t{{%s}} %li\n\t{{%s}}\n", header->name, (name_length+1), header->value);
	
	crb_hash_insert(request->headers, header, header->name, name_length+1);
}

crb_header_t *
crb_request_get_header(crb_request_t *request, char *name, ssize_t name_length)
{
	crb_header_t *header;
	
	if ( name_length == -1 ) {
		name_length = strlen(name)+1;
	}
	
	header = (crb_header_t *) crb_hash_exists_key(request->headers, name, name_length);
	
	return header;
}

void 
crb_request_ref(crb_request_t *request)
{
	crb_atomic_fetch_add( &(request->ref), 1 );
}

void 
crb_request_unref(crb_request_t *request)
{
	int old_ref;
	old_ref = __sync_sub_and_fetch( &(request->ref), 1 );
	
	if ( old_ref == 0 ) {
		crb_request_free(request);
	}
}

void
crb_request_free(crb_request_t *request)
{
	if ( request->uri != NULL ) {
		free(request->uri);
		request->uri = NULL;
	}
	
	
	/* Free headers */
	crb_header_t *header;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(request->headers);

	while ( (header = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_header_free(header);
	}

	crb_hash_cursor_free(cursor);
	crb_hash_free(request->headers);
	request->headers = NULL;
	
	free(request);
}

