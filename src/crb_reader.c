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


#define crb_strcmp3(s, c0, c1, c2) \
    s[0] == c0 && s[1] == c1 && s[2] == c2
    

#define crb_strcmp8(s, c0, c1, c2, c3, c4, c5, c6, c7) \
    s[0] == c0 && s[1] == c1 && s[2] == c2 && s[3] == c3 \
        && s[4] == c4 && s[5] == c5 && s[6] == c6 && s[7] == c7

static void crb_reader_on_data(crb_reader_t *reader, crb_client_t *client);
static int crb_reader_parse_request(crb_client_t *client);

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
	events = calloc (10, sizeof (struct epoll_event));
	
	reader->running = 1;
	while(reader->running) {
		n = epoll_wait (reader->epoll_fd, events, 10, 250);
		for (i = 0; i < n; i += 1) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP) || (!(events[i].events & EPOLLIN))) {
				/* Client closed or error occured */
				printf("Drop client\n");
				client = (crb_client_t *) events[i].data.ptr;
				crb_reader_drop_client(reader, client);
				crb_worker_on_client_disconnect(client);
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
				
				if ( strcmp(client->buffer_in->rpos, "exit") == 0 || strcmp(client->buffer_in->rpos, "exit\n") == 0 ) {
					printf("STOP commmand recieved\n");
					crb_worker_stop();
					break;
				}
				
				crb_reader_on_data(reader, client);
				
				/*
				// create task
				{
					crb_task_t *task;
					task = crb_task_init();
					crb_task_set_client(task, client);
					crb_task_set_type(task, CRB_TASK_BROADCAST);
					crb_task_set_buffer(task, crb_buffer_copy(client->buffer_in));
					// TODO: replace this line
					
					crb_task_set_data(task, (void*)crb_worker_register_channel("test"));
				
					crb_worker_queue_task(task);
				
					crb_buffer_clear(client->buffer_in);
				}
				*/
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
	
	client->id = reader->client_count;
	crb_atomic_fetch_add( &(reader->client_count), 1 );
	
	crb_hash_insert(reader->clients, client, &(client->id), sizeof(int));
	
	event.data.ptr = client;
	event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
	
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_ADD, client->sock_fd, &event);
}

void 
crb_reader_drop_client(crb_reader_t *reader, crb_client_t *client)
{
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL);
	crb_client_unref(client);
	
	crb_hash_remove(reader->clients, &(client->id), sizeof(int));
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
	if ( client->data_state != CRB_DATA_STATE_HANDSHAKE ) {
		printf("Wrong data status\n");
		return;
	}
	
	switch(client->data_state) {
		case CRB_DATA_STATE_HANDSHAKE:
			result = crb_reader_parse_request(client);
			
			if ( result == CRB_ERROR_INVALID_METHOD || result == CRB_ERROR_INVALID_REQUEST ) {
				// Close client
				crb_reader_drop_client(reader, client);
				crb_worker_on_client_disconnect(client);
			} else if ( result == CRB_PARSE_HEADER_INCOMPLETE ) {
				// Wait for more data, reset read position and free request
				crb_client_set_request(client, NULL);
				client->buffer_in->rpos = client->buffer_in->ptr;
			} else if( result == CRB_PARSE_HEADER_DONE ) {
				// Send handshake response
				crb_task_t *task;
				task = crb_task_init();
				crb_task_set_client(task, client);
				crb_task_set_type(task, CRB_TASK_HANDSHAKE);
				
				crb_worker_queue_task(task);
				
				client->state = CRB_STATE_OPEN;
				client->data_state = CRB_DATA_STATE_FRAME_BEGIN;
			}
			break;
		default:
			printf("Unknown data status\n");
	}
	
}

int
crb_reader_parse_request(crb_client_t *client)
{
	crb_buffer_t *b;
	char c, ch, *p, *last;
	char *left, *right;
	char *header_name, *header_value;
	crb_request_t *request;
	int is_cr, is_crlf, trailing_ws = 0;
	
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
    	printf("nothing new\n");
    	return CRB_PARSE_HEADER_INCOMPLETE;
    }
    
    printf("begin parse\n");
    
    for (; b->rpos < last; b->rpos++) {
    	p = b->rpos;
    	ch = *p;
    	printf("( %c ) %i\n", ch, state);
    	
    	switch(state) {
    		case sw_start:
    			left = p;
    			
		        if (ch == 10 || ch == 13 || ch == 0) {
		            break;
		        }

		        if (ch < 'A' || ch > 'Z') {
		        	printf("INVALID METHOD\n");
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
    				
	        		printf("METHOD NOT GET\n");
					return CRB_ERROR_INVALID_METHOD;
    			} else if( ch < 'A' || ch > 'Z' ) {
	        		printf("METHOD NOT GET\n");
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
				    	printf("INVALID REQUEST\n");
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
				    case 10:
				    case 13:
				    case '\0':
				    	printf("INVALID REQUEST 2\n");
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
				    	printf("INVALID REQUEST 3\n");
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
							printf("INVALID REQUEST 4\n");
							return CRB_ERROR_INVALID_REQUEST;
						}
						break;
					default:
						printf("INVALID REQUEST 5\n");
					    return CRB_ERROR_INVALID_REQUEST;
		    	}
		    	break;
		    case sw_header_crlf:
		    	trailing_ws = 0;
    			switch(ch) {
    				case 13: // CR
    				case 10: // LF
    					break;
    				case 32: // SP
    				case 9: // HT
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
    				case 13: // CR
    					// is_cr = 1;break;
    					break;
    				case 10: // LF
    					if ( is_crlf ) {
							printf("END OF TRANSMISSION\n");
							return;
    					} else {				
							is_crlf = 1;
    					}
    					break;
    				case 32: // SP
    				case 9: // HT
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
    				case 13: // CR
    				case 10: // LF
    				case 32: // SP
    				case 9: // HT
    					// separators, not allowed in header name token
						printf("INVALID REQUEST: Wrong header name format\n");
					    return CRB_ERROR_INVALID_REQUEST;
    				case ':':
    					// end of name
    					right = p;
				        header_name = calloc(1, right-left+1);
				        memcpy(header_name, left, right-left);
				        printf("HEADER NAME:\t\t{{%s}}\n", header_name);
				        state = sw_header_separator;
				        break;
    			}
    			break;
		    case sw_header_separator:
    			switch(ch) {
    				case 32: // SP
    				case 9: // HT
    					break;
    				case 13: // CR
    					break;
    				case 10: // LF
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
    				case 13: // CR
    					trailing_ws++;
    					//is_cr = 1;
    					break;
    				case 10: // LF
						if(is_crlf) {
							right = p - trailing_ws;
							trailing_ws = 0;
							
						    header_value = calloc(1, right-left+1);
						    memcpy(header_value, left, right-left);
						    printf("HEADER VALUE:\t\t{{%s}}\n", header_value);
						    
						    crb_request_add_header(request, header_name, header_value);
						    
						    if ( header_name != NULL ) {
								free(header_name);
								header_name = NULL;
						    }
						    
						    free(header_value);
						    header_value = NULL;
							
							printf("END OF TRANSMISSION\n");
							return CRB_PARSE_HEADER_DONE;
						} else {
    						is_crlf = 1;
						}
    					trailing_ws++;
    					break;
    				case 32: // SP
    				case 9: // HT
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
							
						    header_value = calloc(1, right-left+1);
						    if ( header_value != NULL && right-left > 0 ) {
								memcpy(header_value, left, right-left);
								printf("HEADER VALUE:\t\t{{%s}}\n", header_value);
						    
								crb_request_add_header(request, header_name, header_value);
								
								if ( header_name != NULL ) {
									free(header_name);
									header_name = NULL;
								}
								
								free(header_value);
								header_value = NULL;
						    } else {
								printf("HEADER VALUE:\t\t{{}}\n");
								
								crb_request_add_header(request, header_name, "");
								
								if ( header_name != NULL ) {
									free(header_name);
									header_name = NULL;
								}
								
								header_value = NULL;
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
    
    printf("Incomplete\n");
    return CRB_PARSE_HEADER_INCOMPLETE;
}



