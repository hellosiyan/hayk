#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hk_request.h"
#include "hk_atomic.h"
#include "hk_buffer.h"
#include "hk_log.h"

static void hk_request_free(hk_http_request_t *request);

hk_http_header_t *
hk_header_init()
{
	hk_http_header_t *header;
	
	header = malloc(sizeof(hk_http_header_t));
	if ( header == NULL ) {
		return NULL;
	}
	
	header->name = NULL;
	header->value = NULL;
	
	return header;
}

void
hk_header_free(hk_http_header_t *header)
{
	if ( header->name != NULL ) {
		free(header->name);
	}
	
	if ( header->value != NULL ) {
		free(header->value);
	}
	
	free(header);
}

hk_http_request_t *
hk_request_init()
{
	hk_http_request_t *request;
	
	request = malloc(sizeof(hk_http_request_t));
	if ( request == NULL ) {
		return NULL;
	}
	
	request->uri = NULL;
	request->headers = hk_hash_init(10);
	
	request->ref = 0;
	
	return request;
}

void
hk_request_set_uri(hk_http_request_t *request, char *uri, size_t length)
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
hk_request_add_header(hk_http_request_t *request, char *name, size_t name_length, char *value, size_t value_length)
{
	hk_http_header_t *header = hk_header_init();
	
	if ( name == NULL || value == NULL ) {
		return;
	}
	
	if ( value_length == -1 ) {
		value_length = strlen(value);
	}
	
	header->value = malloc(value_length+1);
	if ( header->value == NULL ) {
		hk_log_error("Cannot allocate memory for request header value");
		return;
	}
	
	header->value = memcpy(header->value, value, value_length);
	header->value[value_length] = '\0';
	
	if ( name_length == -1 ) {
		name_length = strlen(name);
	}
	header->name = malloc(name_length+1);
	if ( header->name == NULL ) {
		hk_log_error("Cannot allocate memory for request header name");
		return;
	}
	
	header->name = memcpy(header->name, name, name_length);
	header->name[name_length] = '\0';
	
	hk_hash_insert(request->headers, header, header->name, name_length+1);
}

hk_http_header_t *
hk_request_get_header(hk_http_request_t *request, char *name, size_t name_length)
{
	hk_http_header_t *header;
	
	if ( name_length == -1 ) {
		name_length = strlen(name)+1;
	}
	
	header = (hk_http_header_t *) hk_hash_exists_key(request->headers, name, name_length);
	
	return header;
}

char *
hk_request_get_headers_string(hk_http_request_t *request, int *size)
{
	hk_buffer_t *buffer;
	hk_http_header_t *header;
	char *string;
	
	buffer = hk_buffer_init(1024);
	
	hk_hash_cursor_t *cursor = hk_hash_cursor_init(request->headers);

	while ( (header = hk_hash_cursor_next(cursor)) != NULL ) {
		hk_buffer_append_string(buffer, (const char*)header->name, -1);
		hk_buffer_append_string(buffer, ": ", 2);
		hk_buffer_append_string(buffer, (const char*)header->value, -1);
		hk_buffer_append_string(buffer, "\r\n", 2);
	}
	hk_buffer_append_string(buffer, "\r\n", 2);

	hk_hash_cursor_free(cursor);
	
	string = hk_buffer_copy_string(buffer);
	if ( string == NULL ) {
		return NULL;
	}
	
	if ( size != NULL ) {
		*size = buffer->used-1;
	}
	
	hk_buffer_free(buffer);
	
	return string;
}

void 
hk_request_ref(hk_http_request_t *request)
{
	hk_atomic_fetch_add( &(request->ref), 1 );
}

void 
hk_request_unref(hk_http_request_t *request)
{
	uint32_t old_ref;
	old_ref = hk_atomic_sub_fetch( &(request->ref), 1 );
	
	if ( old_ref == 0 ) {
		hk_request_free(request);
	}
}

static void
hk_request_free(hk_http_request_t *request)
{
	if ( request->uri != NULL ) {
		free(request->uri);
		request->uri = NULL;
	}
	
	/* Free headers */
	hk_http_header_t *header;
	hk_hash_cursor_t *cursor = hk_hash_cursor_init(request->headers);

	while ( (header = hk_hash_cursor_next(cursor)) != NULL ) {
		hk_header_free(header);
	}

	hk_hash_cursor_free(cursor);
	hk_hash_free(request->headers);
	request->headers = NULL;
	
	free(request);
}

