#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "hk_worker.h"
#include "hk_atomic.h"
#include "hk_reader.h"
#include "hk_buffer.h"
#include "hk_task.h"
#include "hk_channel.h"
#include "hk_request.h"
#include "hk_ws.h"


#define hk_strcmp2(s, c0, c1) \
    s[0] == c0 && s[1] == c1
    
#define hk_strcmp3(s, c0, c1, c2) \
    s[0] == c0 && s[1] == c1 && s[2] == c2

#define hk_strcmp8(s, c0, c1, c2, c3, c4, c5, c6, c7) \
    s[0] == c0 && s[1] == c1 && s[2] == c2 && s[3] == c3 \
        && s[4] == c4 && s[5] == c5 && s[6] == c6 && s[7] == c7

static void hk_reader_on_data(hk_reader_t *reader, hk_client_t *client);
static int hk_reader_parse_handshake_request(hk_http_request_t *request, hk_buffer_t *buffer);
static int hk_reader_validate_handshake_request(hk_http_request_t *request);
static int hk_reader_handle_data_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame);
static int hk_reader_handle_ping_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame);
static hk_channel_t * hk_reader_parse_data_frame(hk_ws_frame_t *frame);
static int hk_reader_handle_control_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame);
static hk_list_t * hk_reader_parse_control_frame(hk_ws_frame_t *frame);

hk_reader_t *
hk_reader_init()
{
    hk_reader_t *reader;

    reader = malloc(sizeof(hk_reader_t));
    if (reader == NULL) {
        return NULL;
    }
    
	reader->clients = hk_hash_init(CRB_READER_CLIENTS_SCALE);
    reader->running = 0;
    reader->client_count = 0;
    reader->cid = 0;
    
    reader->epoll_fd = epoll_create1 (0);
	if (reader->epoll_fd == -1) {
		hk_log_error("Cannot create epoll descriptor");
		free(reader);
		return NULL;
	}

    return reader;
}

void 
hk_reader_free(hk_reader_t *reader)
{
	hk_reader_stop(reader);
	hk_hash_free(reader->clients);
	free(reader);
}

static void* 
hk_reader_loop(void *data)
{
	int n,i,ci, chars_read;
	struct epoll_event *events;
	hk_reader_t *reader;
	hk_client_t *client;
	char *buf[4096];
	buf[4095] = '\0';
	
	reader = (hk_reader_t *) data;
	events = calloc (CRB_READER_EPOLL_MAX_EVENTS, sizeof (struct epoll_event));
	if ( events == NULL ) {
		hk_log_error("Cannot allocate epoll events array");
		return 0;
	}

	reader->running = 1;
	while(reader->running) {
		n = epoll_wait (reader->epoll_fd, events, CRB_READER_EPOLL_MAX_EVENTS, 250);
		
		for (i = 0; i < n; i += 1) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP) || (!(events[i].events & EPOLLIN))) {
				/* Client closed or error occured */
				hk_log_debug("Client closed connection");
				
				client = (hk_client_t *) events[i].data.ptr;
				hk_reader_drop_client(reader, client);
				continue;
			} else if (events[i].events & EPOLLIN) {
				client = (hk_client_t *) events[i].data.ptr;
				
				chars_read = read(client->sock_fd, buf, 1024);
				
				if ( chars_read == 0 ) {
					continue;
				}

				while ( chars_read > 0 ) {
					hk_buffer_append_string(client->buffer_in, (const char*)buf, chars_read);
					chars_read = read(client->sock_fd, buf, 1024);
				}

				hk_reader_on_data(reader, client);
			}
		}
	}
	
	free(events);

	return 0;
}

void
hk_reader_run(hk_reader_t *reader)
{
	if ( reader->running ) {
		return;
	}
	
	pthread_create( &(reader->thread_id), NULL, hk_reader_loop, (void*) reader );
}

void
hk_reader_stop(hk_reader_t *reader)
{
	if ( !reader->running ) {
		return;
	}
	
 	hk_reader_drop_all_clients(reader);
	
	reader->running = 0;
	
	pthread_join(reader->thread_id, NULL);
 	
	reader->running = 0;
}

void 
hk_reader_add_client(hk_reader_t *reader, hk_client_t *client)
{
	struct epoll_event event;
	int result;
	
	client->id = hk_atomic_add_fetch( &(reader->cid), 1 );
	
	hk_hash_insert(reader->clients, client, &(client->id), sizeof(int));
	
	event.data.ptr = client;
	event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
	
	result = epoll_ctl (reader->epoll_fd, EPOLL_CTL_ADD, client->sock_fd, &event);
	if ( result == -1 && (errno == ENOMEM || errno == ENOSPC) ) {
		hk_log_error("Cannot add client to epoll - not enough memory");
	}
}

void 
hk_reader_drop_client(hk_reader_t *reader, hk_client_t *client)
{
	hk_worker_on_client_disconnect(client);
	int result;
	
	result = epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL);
	if ( result == -1 && (errno == ENOMEM || errno == ENOSPC) ) {
		hk_log_error("Cannot add client to epoll - not enough memory");
	}
	
	hk_hash_remove(reader->clients, &(client->id), sizeof(int));

	hk_client_mark_as_closing(client);
	
	hk_atomic_fetch_sub( &(reader->client_count), 1 );
	hk_client_unref(client);
}

void 
hk_reader_drop_all_clients(hk_reader_t *reader)
{
	hk_client_t *client;
	hk_hash_cursor_t *cursor = hk_hash_cursor_init(reader->clients);

	while ( (client = hk_hash_cursor_next(cursor)) != NULL ) {
		hk_reader_drop_client(reader, client);
	}

	hk_hash_cursor_free(cursor);
}

/*
 * Static functions
 */
 
static void
hk_reader_on_data(hk_reader_t *reader, hk_client_t *client)
{
	int result;
	hk_ws_frame_t *frame;
	hk_http_request_t *handshake_request;
	
	frame = NULL;
	
	switch(client->data_state) {
		case CRB_DATA_STATE_HANDSHAKE:
			handshake_request = client->request;

			if ( handshake_request == NULL ) {
				handshake_request = hk_request_init();
				hk_client_set_handshake_request(client, handshake_request);
			}

			result = hk_reader_parse_handshake_request(handshake_request, client->buffer_in);
			
			if ( result == CRB_ERROR_INVALID_METHOD || result == CRB_ERROR_INVALID_REQUEST ) {
				// Close client
				hk_reader_drop_client(reader, client);
			} else if ( result == CRB_PARSE_INCOMPLETE ) {
				// Wait for more data, reset read position and free request
				hk_client_set_handshake_request(client, NULL);
				client->buffer_in->rpos = client->buffer_in->ptr;
			} else if( result == CRB_PARSE_DONE ) {
				// Validate handshake
				result = hk_reader_validate_handshake_request(handshake_request);
				if ( result != CRB_VALIDATE_DONE ) {
					// Invalid WebSocket request
					hk_reader_drop_client(reader, client);
					break;
				}
				
				// Send handshake response
				hk_task_t *task;
				task = hk_task_init();
				hk_task_set_client(task, client);
				hk_task_set_type(task, CRB_TASK_HANDSHAKE);
				
				client->state = CRB_STATE_OPEN;
				client->data_state = CRB_DATA_STATE_FRAME;
				hk_buffer_clear(client->buffer_in);
				
				hk_worker_queue_task(task);
			}
			break;
		case CRB_DATA_STATE_FRAME:
		case CRB_DATA_STATE_FRAME_FRAGMENT:
			parse_frame_from_buffer:
			frame = hk_ws_frame_init();
			if ( client->data_state == CRB_DATA_STATE_FRAME_FRAGMENT ) {
				frame->utf8_state = client->fragmented_frame->utf8_state;
			}
			result = hk_ws_frame_parse_buffer(frame, client->buffer_in);
			
			if ( result == CRB_PARSE_INCOMPLETE ) {
				// Wait for more data
				hk_ws_frame_free_with_data(frame);
			} else if ( result == CRB_ERROR_INVALID_OPCODE ) {
				// skip frame
				hk_ws_frame_free_with_data(frame);
				hk_reader_drop_client(reader, client);
			} else if (result == CRB_ERROR_CRITICAL) {
				// Close client to try to recover
				hk_ws_frame_free_with_data(frame);
				hk_reader_drop_client(reader, client);
			} else if ( result == CRB_PARSE_DONE ) {
				// Valid frame structure
				if( frame->is_masked == 0 ) {
					hk_log_error("Received non-masked frame from client");
					hk_ws_frame_free_with_data(frame);
					break;
				} else if ( frame->rsv > 0 ) {
					hk_log_error("Received rsv!=0 with no extension negotiated");
					hk_ws_frame_free_with_data(frame);
					hk_reader_drop_client(reader, client);
					break;
				} else if ( (frame->opcode & CRB_WS_IS_CONTROL_FRAME) != 0 && !frame->is_fin ) {
					hk_log_error("Received fragmented control frame");
					hk_ws_frame_free_with_data(frame);
					hk_reader_drop_client(reader, client);
					break;
				} else if ( frame->opcode == CRB_WS_CONT_FRAME && client->data_state == CRB_DATA_STATE_FRAME ) {
					hk_log_error("No message to continue");
					hk_ws_frame_free_with_data(frame);
					hk_reader_drop_client(reader, client);
					break;
				} else if ( client->data_state == CRB_DATA_STATE_FRAME_FRAGMENT && (frame->opcode != CRB_WS_CONT_FRAME && (frame->opcode & CRB_WS_IS_CONTROL_FRAME) == 0) ) {
					hk_log_error("Data frame injected in fragmanted message");
					hk_ws_frame_free_with_data(frame);
					hk_reader_drop_client(reader, client);
					break;
				}

				if ( !frame->is_fin ) {
					// Handle fragmentation
					client->data_state = CRB_DATA_STATE_FRAME_FRAGMENT;
					hk_client_add_fragment(client, frame);
					hk_ws_frame_free_with_data(frame);
				} else {
					// Replace last continuation frame with the original fragmented frame header
					if ( frame->opcode == CRB_WS_CONT_FRAME ) {
						hk_client_add_fragment(client, frame);
						hk_ws_frame_free_with_data(frame);

						frame = hk_client_compile_fragments(client);
						client->data_state = CRB_DATA_STATE_FRAME;
					}

					// take action based on frame type
					if ( frame->opcode == CRB_WS_TEXT_FRAME || frame->opcode == CRB_WS_BIN_FRAME ) {
						if ( frame->hk_type == CRB_WS_TYPE_DATA ) {
							hk_reader_handle_data_frame(reader, client, frame);
						} else if ( frame->hk_type == CRB_WS_TYPE_CONTROL ) {
							hk_reader_handle_control_frame(reader, client, frame);
						}
					} else if ( frame->opcode == CRB_WS_PING_FRAME ) {
						hk_reader_handle_ping_frame(reader, client, frame);
					} else if ( frame->opcode == CRB_WS_CLOSE_FRAME ) {
						// Close frame - end the connection
						hk_ws_frame_free_with_data(frame);
						hk_reader_drop_client(reader, client);
						break;
					} else {
						// Unsupported frame type
						hk_ws_frame_free_with_data(frame);
					}
				}

				goto parse_frame_from_buffer;
			}
			
			break;
		default:
			// pass
			break;
	}
	
}

static int
hk_reader_parse_handshake_request(hk_http_request_t *request, hk_buffer_t *buffer)
{
	char c, ch, *p, *last;
	char *left, *right, *header_name;
	int is_crlf = 0, trailing_ws = 0;
	size_t header_name_length;
	
    enum {
        sw_start = 0,
        sw_method,					// 1
        sw_spaces_before_uri,		// 2
        sw_after_slash_in_uri,		// 3
        sw_spaces_before_http,		// 4
        sw_http,					// 5
        sw_header_name,				// 6
        sw_header_separator,		// 7
        sw_header_crlf,				// 8
        sw_header_lws,				// 9
        sw_header_value,			// 10
    } state;
    
    state = sw_start;
    last = buffer->ptr + buffer->used;
    
    if ( buffer->rpos >= last ) {
    	return CRB_PARSE_INCOMPLETE;
    }
    
    for (; buffer->rpos < last; buffer->rpos++) {
    	p = buffer->rpos;
    	ch = *p;
    	
    	switch(state) {
    		case sw_start:
    			left = p;
    			
		        if (ch == '\n' || ch == '\r' || ch == 0) {
		            break;
		        }

		        if (ch < 'A' || ch > 'Z') {
		            return CRB_ERROR_INVALID_METHOD;
		        }

		        state = sw_method;
		        break;
    		case sw_method:
    			if ( ch == ' ' ) {
    				if (p - left == 3 && hk_strcmp3(left, 'G', 'E', 'T')) {
				        state = sw_spaces_before_uri;
				        break;
    				}
    				
					return CRB_ERROR_INVALID_METHOD;
    			} else if( ch < 'A' || ch > 'Z' ) {
	        		// Method name must be GET
					return CRB_ERROR_INVALID_METHOD;
    			}
    			break;
    		case sw_spaces_before_uri:
		        if (ch == '/') {
		            left = p;
		            state = sw_after_slash_in_uri;
		            break;
		        }

		        c = (char) (ch | 0x20);
		        if (c >= 'a' && c <= 'z') {
		            break;
		        }

		        switch (ch) {
				    case ' ':
				        break;
				    default:
				    	// character not allowed here
				        return CRB_ERROR_INVALID_REQUEST;
		        }
		        break;
		        
			case sw_after_slash_in_uri:
		        switch (ch) {
				    case ' ':
				        right = p;
				        
				        hk_request_set_uri(request, left, right-left);
				        state = sw_spaces_before_http;
				        break;
				    case '\n':
				    case '\r':
				    case '\0':
				        return CRB_ERROR_INVALID_REQUEST;
		        }
		        break;
			case sw_spaces_before_http:
				switch(ch) {
					case 'H':
						left = p;
						state = sw_http;
						break;
					case ' ':
						break;
					default:
				        return CRB_ERROR_INVALID_REQUEST;
				}
		    	break;
		    case sw_http:
		    	switch(ch) {
					case 'H':
					case 'T':
					case 'P':
					case '/':
					case '1':
					case '.':
						if ( p - left == 7 && hk_strcmp8(left, 'H', 'T', 'T', 'P', '/', '1', '.', '1') ) {
							state = sw_header_crlf;
						} else if ( p - left > 7 ) {
							return CRB_ERROR_INVALID_REQUEST;
						}
						break;
					default:
						// character not part of "HTTP/1.1" identificator
					    return CRB_ERROR_INVALID_REQUEST;
		    	}
		    	break;
		    case sw_header_crlf:
		    	trailing_ws = 0;
    			switch(ch) {
    				case '\r':
    				case '\n':
    					break;
    				case ' ':
    				case '\t':
    					state = sw_header_lws;
    					break;
    				default:
    					left = p;
    					state = sw_header_name;
    					break;
    			}
    			break;
		    case sw_header_lws:
		    	trailing_ws = 0;
    			switch(ch) {
    				case '\r':
    					break;
    				case '\n':
    					if ( is_crlf ) {
							return CRB_PARSE_DONE;
    					} else {				
							is_crlf = 1;
    					}
    					break;
    				case ' ':
    				case '\t':
    					is_crlf = 0;
    					state = sw_header_value;
    					break;
    				default:
    					is_crlf = 0;
    					left = p;
    					state = sw_header_name;
    					break;
    			}
    			break;
		    case sw_header_name:
    			switch(ch) {
    				case '(':
    				case ')':
    				case '<':
    				case '>':
    				case '@':
    				case ',':
    				case ';':
    				case '\\':
    				case '"':
    				case '/':
    				case '[':
    				case ']':
    				case '?':
    				case '=':
    				case '{':
    				case '}':
    				case '\r':
    				case '\n':
    				case ' ':
    				case '\t':
    					// separators, not allowed in header name token
    					hk_log_debug("Invalid header name format");
					    return CRB_ERROR_INVALID_REQUEST;
    				case ':':
    					// end of name
    					right = p;
    					header_name = left;
    					header_name_length = right-left;
    					
				        state = sw_header_separator;
				        break;
				    default:
				    	*(buffer->rpos) = toupper(ch);
    			}
    			break;
		    case sw_header_separator:
    			switch(ch) {
    				case ' ':
    				case '\t':
    					break;
    				case '\r':
    					break;
    				case '\n':
    					is_crlf = 1;
				        state = sw_header_lws;
    					break;
    				default:
    					left = p;
    					state = sw_header_value;
    					break;
    			}
    			break;
		    case sw_header_value:
    			switch(ch) {
    				case '\r':
    					trailing_ws++;
    					break;
    				case '\n':
						if(is_crlf) {
							right = p - trailing_ws;
							trailing_ws = 0;
						    
						    hk_request_add_header(request, header_name, header_name_length, left, right-left);
							return CRB_PARSE_DONE;
						} else {
    						is_crlf = 1;
						}
    					trailing_ws++;
    					break;
    				case ' ':
    				case '\t':
    					if ( is_crlf ) {
    						// lws
    						is_crlf = 0;
    						trailing_ws = 0;
    					}
    					break;
    				default:
    					if ( is_crlf ) {
    						is_crlf = 0;
							right = p-trailing_ws;
							trailing_ws = 0;
							
						    if ( right-left > 0 ) {
						    	hk_request_add_header(request, header_name, header_name_length, left, right-left);
						    } else {
								hk_request_add_header(request, header_name, header_name_length, NULL, 0);
						    }
						    left = p;
						    state = sw_header_name;
    					}
    					break;
    			}
    			break;
		    default:
		    	break;
    	}
    }
    
    return CRB_PARSE_INCOMPLETE;
}

static int
hk_reader_validate_handshake_request(hk_http_request_t *request)
{
	hk_http_header_t *header;
	hk_worker_t *worker;
	
	worker = hk_worker_get();
	
	header = hk_request_get_header(request, CRB_WS_KEY, -1);
	if ( header == NULL || header->value == NULL ) {
		hk_log_debug("Invalid or missing Sec-WebSocket-Key header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	
	header = hk_request_get_header(request, CRB_WS_VERSION, -1);
	
	if ( header == NULL || header->value == NULL ) {
		hk_log_debug("Missing Sec-WebSocket-Version header");
		return CRB_ERROR_INVALID_REQUEST;
	} else if ( !(hk_strcmp2(header->value, '1', '3')) ) {
		printf("version: %s\n", header->value);
		hk_log_debug("Invalid Sec-WebSocket-Version header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	
	if ( worker->config->origins != NULL ) {
		// Same-Origin policy
		header = hk_request_get_header(request, CRB_WS_ORIGIN, -1);
		if ( header == NULL || header->value == NULL ) {
			hk_log_debug("Invalid or missing Origin header");
			return CRB_ERROR_INVALID_REQUEST;
		} else if( !hk_hash_exists_key(worker->config->origins, header->value, strlen(header->value)) ) {
			hk_log_debug("Restricted origin");
			return CRB_ERROR_INVALID_REQUEST;
		}
	}
	
	/*
	header = hk_request_get_header(request, CRB_WS_PROTOCOL, -1);
	if ( header == NULL || header->value == NULL || strstr(header->value, "hayk") == NULL ) {
		hk_log_debug("Invalid or missing Sec-WebSocket-Protocol header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	*/
	
	return CRB_VALIDATE_DONE;
}

static int
hk_reader_handle_data_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame)
{
	hk_task_t *task;
	hk_channel_t *channel;
	
	channel = hk_reader_parse_data_frame(frame);
	/*if ( channel == NULL ) {
		// unknown channel name
		hk_ws_frame_free_with_data(frame);
		return;
	} else if ( !hk_channel_client_is_subscribed(channel, client) ) {
		// not subscribed for this channel
		hk_log_debug("Not subscribed");
		hk_ws_frame_free_with_data(frame);
		return;
	}*/
	
	// hk_log_info(frame->data);
	
	task = hk_task_init();
	hk_task_set_client(task, client);
	hk_task_set_type(task, CRB_TASK_BROADCAST);
	hk_task_set_data(task, channel);
	hk_task_set_data2(task, frame);

	hk_worker_queue_task(task);
}

static int
hk_reader_handle_ping_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame)
{
	hk_task_t *task;
	
	task = hk_task_init();
	hk_task_set_client(task, client);
	hk_task_set_type(task, CRB_TASK_PONG);
	hk_task_set_data(task, NULL);
	hk_task_set_data2(task, frame);

	hk_worker_queue_task(task);
}

static hk_channel_t *
hk_reader_parse_data_frame(hk_ws_frame_t *frame)
{
	char *pos;
	char c, ch, *last;
	char *channel_name;
	size_t channel_name_length;
	return hk_worker_register_channel("test");
	
    enum {
        sw_start = 0,		// 1
        sw_channel_name,	// 2
    } state;
    
    state = sw_start;
    last = frame->data + frame->data_length;
    
    for (pos = frame->data; pos < last; pos++) {
    	ch = *pos;
    	
    	switch(state) {
    		case sw_start:
    			channel_name = pos;
    			
		        if (ch == '\n' || ch == '\r' || ch == 0) {
		            break;
		        }

		        if ( (ch < 'A' || 0x20 > 'Z') && (ch < 'a' || 0x20 > 'z') ) {
		        	hk_log_debug("Invalid channel name");
		            return NULL;
		        }

		        state = sw_channel_name;
		        break;
		    case sw_channel_name:
    			switch(ch) {
    				case '(': case ')':
    				case '<': case '>':
    				case '@': case ',':
    				case ';': case '\\':
    				case '"': case '[':
    				case ']': case '?':
    				case '=': case '{':
    				case '}': case '\r':
    				case ' ': case '\t':
    					// separators, not allowed in channel name token
		        		hk_log_debug("Wrong channel name format");
					    return NULL;
    				case '\n':
    					// end of name
    					channel_name_length = pos-channel_name;
					    
				        return hk_worker_get_channel(channel_name, channel_name_length);
    			}
    			break;
    	}
	}
	
	return NULL;
}

static int
hk_reader_handle_control_frame(hk_reader_t *reader, hk_client_t *client, hk_ws_frame_t *frame)
{
	hk_list_t *commands;
	hk_http_header_t *command;
	hk_channel_t *channel;
	
	commands = hk_reader_parse_control_frame(frame);
	
	command = (hk_http_header_t *) hk_list_pop(commands);
	while ( command != NULL ) {
		if ( strcmp(command->name, "SUBSCRIBE") == 0 ) {
			channel = hk_worker_register_channel( command->value );
			hk_channel_subscribe(channel, client);
		} else if ( strcmp(command->name, "UNSUBSCRIBE") == 0 ) {
			channel = hk_worker_register_channel( command->value );
			hk_channel_unsubscribe(channel, client);
		}
		
		hk_header_free(command);
		
		command = (hk_http_header_t *) hk_list_pop(commands);
	}
	
	hk_list_free(commands);
	hk_ws_frame_free_with_data(frame);
}

static hk_list_t *
hk_reader_parse_control_frame(hk_ws_frame_t *frame)
{
	char * pos;
	char c, ch, *last;
	char *left, *right, *header_name;
	size_t header_name_length;
    
    hk_list_t *commands;
    hk_http_header_t *tmp_header;
	
    enum {
        sw_start = 0,				// 1
        sw_header_name,				// 2
        sw_header_separator,		// 3
        sw_header_value,			// 5
    } state;
    
    commands = hk_list_init();
    
    frame = frame;
    
    state = sw_start;
    last = frame->data + frame->data_length;
    
    for (pos = frame->data; pos < last; pos++) {
    	ch = *pos;
    	
    	switch(state) {
    		case sw_start:
    			left = pos;
    			
		        if (ch == '\n' || ch == '\r' || ch == 0) {
		            break;
		        }

		        if (ch < 'A' || ch > 'Z') {
		            return commands;
		        }

		        state = sw_header_name;
		        break;
		    case sw_header_name:
    			switch(ch) {
    				case '(': case ')':
    				case '<': case '>':
    				case '@': case ',':
    				case ';': case '\\':
    				case '"': case '/':
    				case '[': case ']':
    				case '?': case '=':
    				case '{': case '}':
    				case '\r': case '\n':
    				case ' ': case '\t':
    					// separators, not allowed in header name token
					    return commands;
    				case ':':
    					// end of name
    					right = pos;
    					header_name = left;
    					header_name_length = right-left;
					    
				        state = sw_header_separator;
				        break;
				    default:
				    	*(pos) = toupper(ch);
    			}
    			break;
		    case sw_header_separator:
    			switch(ch) {
    				case ' ':
    				case '\t':
    				case '\r':
    				case '\n':
    					break;
    				default:
    					left = pos;
    					state = sw_header_value;
    					break;
    			}
    			break;
		    case sw_header_value:
    			switch(ch) {
    				case '\n':
						right = pos;
					    
					    tmp_header = hk_header_init();
					    
					    tmp_header->name = malloc(header_name_length+1);
					    memcpy(tmp_header->name, header_name, header_name_length);
					    tmp_header->name[header_name_length] = '\0';
					    
					    tmp_header->value = malloc(right-left+1);
					    memcpy(tmp_header->value, left, right-left);
					    tmp_header->value[right-left] = '\0';
					    
					    hk_list_push(commands, tmp_header);
					    
					    left = pos + 1;
					    state = sw_header_name;
    					break;
    				default:
    					break;
    			}
    			break;
    	}
	}
	
	return commands;
}


