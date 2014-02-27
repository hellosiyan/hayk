#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crb_atomic.h"
#include "crb_request.h"


static void crb_request_free(crb_http_request_t *request);

crb_http_header_t *
crb_header_init()
{
	crb_http_header_t *header;
	
	header = malloc(sizeof(crb_http_header_t));
	if ( header == NULL ) {
		return NULL;
	}
	
	header->name = NULL;
	header->value = NULL;
	
	return header;
}

void
crb_header_free(crb_http_header_t *header)
{
	if ( header->name != NULL ) {
		free(header->name);
	}
	
	if ( header->value != NULL ) {
		free(header->value);
	}
	
	free(header);
}

crb_http_request_t *
crb_request_init()
{
	crb_http_request_t *request;
	
	request = malloc(sizeof(crb_http_request_t));
	if ( request == NULL ) {
		return NULL;
	}
	
	request->uri = NULL;
	request->headers = crb_hash_init(10);
	
	request->ref = 0;
	
	return request;
}

void
crb_request_set_uri(crb_http_request_t *request, char *uri, size_t length)
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
crb_request_add_header(crb_http_request_t *request, char *name, size_t name_length, char *value, size_t value_length)
{
	crb_http_header_t *header = crb_header_init();
	
	if ( name == NULL || value == NULL ) {
		return;
	}
	
	if ( value_length == -1 ) {
		value_length = strlen(value);
	}
	
	header->value = malloc(value_length+1);
	if ( header->value == NULL ) {
		crb_log_error("Cannot allocate memory for request header value");
		return;
	}
	
	header->value = memcpy(header->value, value, value_length);
	header->value[value_length] = '\0';
	
	if ( name_length == -1 ) {
		name_length = strlen(name);
	}
	header->name = malloc(name_length+1);
	if ( header->name == NULL ) {
		crb_log_error("Cannot allocate memory for request header name");
		return;
	}
	
	header->name = memcpy(header->name, name, name_length);
	header->name[name_length] = '\0';
	
	crb_hash_insert(request->headers, header, header->name, name_length+1);
}

crb_http_header_t *
crb_request_get_header(crb_http_request_t *request, char *name, size_t name_length)
{
	crb_http_header_t *header;
	
	if ( name_length == -1 ) {
		name_length = strlen(name)+1;
	}
	
	header = (crb_http_header_t *) crb_hash_exists_key(request->headers, name, name_length);
	
	return header;
}

char *
crb_request_get_headers_string(crb_http_request_t *request, int *size)
{
	crb_buffer_t *buffer;
	crb_http_header_t *header;
	char *string;
	
	buffer = crb_buffer_init(1024);
	
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(request->headers);

	while ( (header = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_buffer_append_string(buffer, (const char*)header->name, -1);
		crb_buffer_append_string(buffer, ": ", 2);
		crb_buffer_append_string(buffer, (const char*)header->value, -1);
		crb_buffer_append_string(buffer, "\r\n", 2);
	}
	crb_buffer_append_string(buffer, "\r\n", 2);

	crb_hash_cursor_free(cursor);
	
	string = crb_buffer_copy_string(buffer);
	if ( string == NULL ) {
		return NULL;
	}
	
	if ( size != NULL ) {
		*size = buffer->used-1;
	}
	
	crb_buffer_free(buffer);
	
	return string;
}

void 
crb_request_ref(crb_http_request_t *request)
{
	crb_atomic_fetch_add( &(request->ref), 1 );
}

void 
crb_request_unref(crb_http_request_t *request)
{
	uint32_t old_ref;
	old_ref = crb_atomic_sub_fetch( &(request->ref), 1 );
	
	if ( old_ref == 0 ) {
		crb_request_free(request);
	}
}

static void
crb_request_free(crb_http_request_t *request)
{
	if ( request->uri != NULL ) {
		free(request->uri);
		request->uri = NULL;
	}
	
	/* Free headers */
	crb_http_header_t *header;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(request->headers);

	while ( (header = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_header_free(header);
	}

	crb_hash_cursor_free(cursor);
	crb_hash_free(request->headers);
	request->headers = NULL;
	
	free(request);
}

