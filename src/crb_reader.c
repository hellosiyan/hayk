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

#include "crb_worker.h"
#include "crb_atomic.h"
#include "crb_reader.h"
#include "crb_buffer.h"
#include "crb_task.h"
#include "crb_channel.h"
#include "crb_request.h"
#include "crb_ws.h"


#define crb_strcmp2(s, c0, c1) \
    s[0] == c0 && s[1] == c1
    
#define crb_strcmp3(s, c0, c1, c2) \
    s[0] == c0 && s[1] == c1 && s[2] == c2

#define crb_strcmp8(s, c0, c1, c2, c3, c4, c5, c6, c7) \
    s[0] == c0 && s[1] == c1 && s[2] == c2 && s[3] == c3 \
        && s[4] == c4 && s[5] == c5 && s[6] == c6 && s[7] == c7

static void crb_reader_on_data(crb_reader_t *reader, crb_client_t *client);
static int crb_reader_parse_request(crb_client_t *client);
static int crb_reader_validate_request(crb_client_t *client);
static int crb_reader_handle_data_frame(crb_reader_t *reader, crb_client_t *client, crb_ws_frame_t *frame);
static crb_channel_t * crb_reader_parse_data_frame(crb_ws_frame_t *frame);
static int crb_reader_handle_control_frame(crb_reader_t *reader, crb_client_t *client, crb_ws_frame_t *frame);
static crb_list_t * crb_reader_parse_control_frame(crb_ws_frame_t *frame);

crb_reader_t *
crb_reader_init()
{
    crb_reader_t *reader;

    reader = malloc(sizeof(crb_reader_t));
    if (reader == NULL) {
        return NULL;
    }
    
	reader->clients = crb_hash_init(CRB_READER_CLIENTS_SCALE);
    reader->running = 0;
    reader->client_count = 0;
    
    reader->epoll_fd = epoll_create1 (0);
	if (reader->epoll_fd == -1) {
		crb_log_error("Cannot create epoll descriptor");
		free(reader);
		return NULL;
	}

    return reader;
}

void 
crb_reader_free(crb_reader_t *reader)
{
	crb_reader_stop(reader);
	crb_hash_free(reader->clients);
	free(reader);
}

static void* 
crb_reader_loop(void *data)
{
	int n,i,ci, chars_read;
	struct epoll_event *events;
	crb_reader_t *reader;
	crb_client_t *client;
	char *buf[4096];
	buf[4095] = '\0';
	
	reader = (crb_reader_t *) data;
	events = calloc (CRB_READER_EPOLL_MAX_EVENTS, sizeof (struct epoll_event));
	if ( events == NULL ) {
		crb_log_error("Cannot allocate epoll events array");
		return 0;
	}
	
	reader->running = 1;
	while(reader->running) {
		n = epoll_wait (reader->epoll_fd, events, CRB_READER_EPOLL_MAX_EVENTS, 250);
		
		for (i = 0; i < n; i += 1) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP) || (!(events[i].events & EPOLLIN))) {
				/* Client closed or error occured */
				crb_log_debug("Client closed connection");
				
				client = (crb_client_t *) events[i].data.ptr;
				crb_reader_drop_client(reader, client);
				continue;
			} else if (events[i].events & EPOLLIN) {
				client = (crb_client_t *) events[i].data.ptr;
				
				chars_read = read(client->sock_fd, buf, 1024);
				
				if ( chars_read == 0 ) {
					continue;
				}
				
				while ( chars_read > 0 ) {
					crb_buffer_append_string(client->buffer_in, (const char*)buf, chars_read);
					chars_read = read(client->sock_fd, buf, 1024);
				}
				
				crb_reader_on_data(reader, client);
			}
		}
	}
	
	free(events);

	return 0;
}

void
crb_reader_run(crb_reader_t *reader)
{
	if ( reader->running ) {
		return;
	}
	
	pthread_create( &(reader->thread_id), NULL, crb_reader_loop, (void*) reader );
}

void
crb_reader_stop(crb_reader_t *reader)
{
	if ( !reader->running ) {
		return;
	}
	
 	crb_reader_drop_all_clients(reader);
	
	reader->running = 0;
	
	pthread_join(reader->thread_id, NULL);
 	
	reader->running = 0;
}

void 
crb_reader_add_client(crb_reader_t *reader, crb_client_t *client)
{
	struct epoll_event event;
	int result;
	
	client->id = crb_atomic_add_fetch( &(reader->client_count), 1 );
	
	crb_hash_insert(reader->clients, client, &(client->id), sizeof(int));
	
	event.data.ptr = client;
	event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
	
	result = epoll_ctl (reader->epoll_fd, EPOLL_CTL_ADD, client->sock_fd, &event);
	if ( result == -1 && (errno == ENOMEM || errno == ENOSPC) ) {
		crb_log_error("Cannot add client to epoll - not enough memory");
	}
}

void 
crb_reader_drop_client(crb_reader_t *reader, crb_client_t *client)
{
	crb_worker_on_client_disconnect(client);
	int result;
	
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL);
	if ( result == -1 && (errno == ENOMEM || errno == ENOSPC) ) {
		crb_log_error("Cannot add client to epoll - not enough memory");
	}
	
	crb_hash_remove(reader->clients, &(client->id), sizeof(int));
		
	if ( client->state == CRB_STATE_OPEN ) {
		uint8_t *close_frame;
		int close_frame_length;
		
		client->state == CRB_STATE_CLOSING;
	
		close_frame = crb_ws_frame_close(&close_frame_length, 0);
		
		write(client->sock_fd, (char*)close_frame, close_frame_length);
		free(close_frame);
	} else {
		client->state == CRB_STATE_CLOSING;
	}
	
	crb_atomic_fetch_sub( &(reader->client_count), 1 );
	crb_client_unref(client);
}

void 
crb_reader_drop_all_clients(crb_reader_t *reader)
{
	crb_client_t *client;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(reader->clients);

	while ( (client = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_reader_drop_client(reader, client);
	}

	crb_hash_cursor_free(cursor);
}

/*
 * Static functions
 */
 
static void
crb_reader_on_data(crb_reader_t *reader, crb_client_t *client)
{
	int result;
	crb_ws_frame_t *frame;
	
	frame = NULL;
	
	switch(client->data_state) {
		case CRB_DATA_STATE_HANDSHAKE:
			result = crb_reader_parse_request(client);
			
			if ( result == CRB_ERROR_INVALID_METHOD || result == CRB_ERROR_INVALID_REQUEST ) {
				// Close client
				crb_reader_drop_client(reader, client);
			} else if ( result == CRB_PARSE_INCOMPLETE ) {
				// Wait for more data, reset read position and free request
				crb_client_set_request(client, NULL);
				client->buffer_in->rpos = client->buffer_in->ptr;
			} else if( result == CRB_PARSE_DONE ) {
				// Validate handshake
				result = crb_reader_validate_request(client);
				if ( result != CRB_VALIDATE_DONE ) {
					// Invalid WebSocket request
					crb_reader_drop_client(reader, client);
					break;
				}
				
				// Send handshake response
				crb_task_t *task;
				task = crb_task_init();
				crb_task_set_client(task, client);
				crb_task_set_type(task, CRB_TASK_HANDSHAKE);
				
				client->state = CRB_STATE_OPEN;
				client->data_state = CRB_DATA_STATE_FRAME_BEGIN;
				crb_buffer_clear(client->buffer_in);
				
				crb_worker_queue_task(task);
			}
			break;
		case CRB_DATA_STATE_FRAME_BEGIN:
			frame = crb_ws_frame_init();
			result = crb_ws_frame_parse_buffer(frame, client->buffer_in);
			
			if ( result == CRB_PARSE_INCOMPLETE ) {
				// Wait for more data
				crb_ws_frame_free_with_data(frame);
			} else if ( result == CRB_ERROR_INVALID_OPCODE ) {
				// TODO: skip frame
				crb_ws_frame_free_with_data(frame);
			} else if ( result == CRB_PARSE_DONE ) {
				// Valid frame
				if( frame->is_masked == 0 ) {
					crb_log_error("Received non-masked frame from client");
					crb_ws_frame_free_with_data(frame);
					break;
				}
				
				crb_buffer_trim_left(client->buffer_in);
				
				// take action based on frame type
				if ( frame->opcode == CRB_WS_TEXT_FRAME ) {
					if ( frame->crb_type == CRB_WS_TYPE_DATA ) {
						crb_reader_handle_data_frame(reader, client, frame);
					} else if ( frame->crb_type == CRB_WS_TYPE_CONTROL ) {
						crb_reader_handle_control_frame(reader, client, frame);
					}
				} else if ( frame->opcode == CRB_WS_CLOSE_FRAME ) {
					// Close frame - end the connection
					crb_ws_frame_free_with_data(frame);
					crb_reader_drop_client(reader, client);
				} else {
					// Unsupported frame type
					crb_ws_frame_free_with_data(frame);
				}
			} else if (result == CRB_ERROR_CRITICAL) {
				// Close client to try to recover
				crb_ws_frame_free_with_data(frame);
				crb_reader_drop_client(reader, client);
			}
			
			break;
		default:
			// pass
			break;
	}
	
}

static int
crb_reader_parse_request(crb_client_t *client)
{
	crb_buffer_t *b;
	char c, ch, *p, *last;
	char *left, *right, *header_name;
	crb_request_t *request;
	int is_cr, is_crlf, trailing_ws = 0;
	size_t header_name_length;
	
	if ( client->request == NULL ) {
		request = crb_request_init();
		crb_client_set_request(client, request);
	} else {
		request = client->request;
	}
	
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
    
    b = client->buffer_in;
    
    state = sw_start;
    last = b->ptr + b->used;
    
    if ( b->rpos >= last ) {
    	return CRB_PARSE_INCOMPLETE;
    }
    
    for (; b->rpos < last; b->rpos++) {
    	p = b->rpos;
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
    				if (p - left == 3 && crb_strcmp3(left, 'G', 'E', 'T')) {
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
				        
				        crb_request_set_uri(request, left, right-left);
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
						if ( p - left == 7 && crb_strcmp8(left, 'H', 'T', 'T', 'P', '/', '1', '.', '1') ) {
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
    					crb_log_debug("Invalid header name format");
					    return CRB_ERROR_INVALID_REQUEST;
    				case ':':
    					// end of name
    					right = p;
    					header_name = left;
    					header_name_length = right-left;
    					
				        state = sw_header_separator;
				        break;
				    default:
				    	*(b->rpos) = toupper(ch);
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
						    
						    crb_request_add_header(request, header_name, header_name_length, left, right-left);
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
						    	crb_request_add_header(request, header_name, header_name_length, left, right-left);
						    } else {
								crb_request_add_header(request, header_name, header_name_length, NULL, 0);
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
crb_reader_validate_request(crb_client_t *client)
{
	crb_request_t *request;
	crb_header_t *header;
	
	request = client->request;
	
	header = crb_request_get_header(request, CRB_WS_KEY, -1);
	if ( header == NULL || header->value == NULL ) {
		crb_log_debug("Invalid or missing Sec-WebSocket-Key header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	
	header = crb_request_get_header(request, CRB_WS_VERSION, -1);
	if ( header == NULL || header->value == NULL || !(crb_strcmp2(header->value, '1', '3')) ) {
		crb_log_debug("Invalid or missing Sec-WebSocket-Version header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	
	/*
	header = crb_request_get_header(request, CRB_WS_PROTOCOL, -1);
	if ( header == NULL || header->value == NULL || strstr(header->value, "caribou") == NULL ) {
		crb_log_debug("Invalid or missing Sec-WebSocket-Protocol header");
		return CRB_ERROR_INVALID_REQUEST;
	}
	*/
	
	return CRB_VALIDATE_DONE;
}

static int
crb_reader_handle_data_frame(crb_reader_t *reader, crb_client_t *client, crb_ws_frame_t *frame)
{
	crb_task_t *task;
	crb_channel_t *channel;
	
	channel = crb_reader_parse_data_frame(frame);
	if ( channel == NULL ) {
		// unknown channel name
		crb_ws_frame_free_with_data(frame);
		return;
	} else if ( !crb_channel_client_is_subscribed(channel, client) ) {
		// not subscribed for this channel
		crb_ws_frame_free_with_data(frame);
		return;
	}
	
	task = crb_task_init();
	crb_task_set_client(task, client);
	crb_task_set_type(task, CRB_TASK_BROADCAST);
	crb_task_set_data(task, channel);
	crb_task_set_data2(task, frame);

	crb_worker_queue_task(task);
}

static crb_channel_t *
crb_reader_parse_data_frame(crb_ws_frame_t *frame)
{
	char *pos;
	char c, ch, *last;
	char *channel_name;
	size_t channel_name_length;
	
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
		        	crb_log_debug("Invalid channel name");
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
		        		crb_log_debug("Wrong channel name format");
					    return NULL;
    				case '\n':
    					// end of name
    					channel_name_length = pos-channel_name;
					    
				        return crb_worker_get_channel(channel_name, channel_name_length);
    			}
    			break;
    	}
	}
	
	return NULL;
}

static int
crb_reader_handle_control_frame(crb_reader_t *reader, crb_client_t *client, crb_ws_frame_t *frame)
{
	crb_list_t *commands;
	crb_header_t *command;
	crb_channel_t *channel;
	
	commands = crb_reader_parse_control_frame(frame);
	
	command = (crb_header_t *) crb_list_pop(commands);
	while ( command != NULL ) {
		if ( strcmp(command->name, "SUBSCRIBE") == 0 ) {
			channel = crb_worker_register_channel( command->value );
			crb_channel_subscribe(channel, client);
		} else if ( strcmp(command->name, "UNSUBSCRIBE") == 0 ) {
			channel = crb_worker_register_channel( command->value );
			crb_channel_unsubscribe(channel, client);
		}
		
		crb_header_free(command);
		
		command = (crb_header_t *) crb_list_pop(commands);
	}
	
	crb_list_free(commands);
	crb_ws_frame_free_with_data(frame);
}

static crb_list_t *
crb_reader_parse_control_frame(crb_ws_frame_t *frame)
{
	char * pos;
	char c, ch, *last;
	char *left, *right, *header_name;
	size_t header_name_length;
    
    crb_list_t *commands;
    crb_header_t *tmp_header;
	
    enum {
        sw_start = 0,				// 1
        sw_header_name,				// 2
        sw_header_separator,		// 3
        sw_header_value,			// 5
    } state;
    
    commands = crb_list_init();
    
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
					    
					    tmp_header = crb_header_init();
					    
					    tmp_header->name = malloc(header_name_length+1);
					    memcpy(tmp_header->name, header_name, header_name_length);
					    tmp_header->name[header_name_length] = '\0';
					    
					    tmp_header->value = malloc(right-left+1);
					    memcpy(tmp_header->value, left, right-left);
					    tmp_header->value[right-left] = '\0';
					    
					    crb_list_push(commands, tmp_header);
					    
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


