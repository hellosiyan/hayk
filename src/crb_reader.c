#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <string.h>

#include "crb_reader.h"
#include "crb_buffer.h"


crb_reader_t *
crb_reader_init()
{
    crb_reader_t *reader;

    reader = malloc(sizeof(crb_reader_t));
    if (reader == NULL) {
        return NULL;
    }
    
    reader->running = 0;
    reader->client_count = 0;
    
    reader->epoll_fd = epoll_create1 (0);
	if (reader->epoll_fd == -1) {
		free(reader);
		return NULL;
	}

    return reader;
}

static void* 
crb_reader_loop(void *data)
{
	int n,i, chars_read;
	struct epoll_event *events;
	crb_reader_t *reader;
	crb_client_t *client;
	char *buf[80];
	char *buf2[80];
	buf[79] = '\0';
	
	reader = (crb_reader_t *) data;
	events = calloc (10, sizeof (struct epoll_event));
	
	while(1) {
		n = epoll_wait (reader->epoll_fd, events, 10, -1);
		for (i = 0; i < n; i += 1) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
				/* Client closed or error occured */
				client = (crb_client_t *) events[i].data.ptr;
				crb_reader_drop_client(reader, client);
				continue;
			} else if (events[i].events & EPOLLIN) {
				client = (crb_client_t *) events[i].data.ptr;
				
				chars_read = read(client->sock_fd, buf, 5);
				while ( chars_read >= 0 ) {
					crb_buffer_append_string(client->buffer_in, buf, chars_read);
					chars_read = read(client->sock_fd, buf, 5);
				}
				
				write(STDIN_FILENO, client->buffer_in->ptr, strlen(client->buffer_in->ptr));
			}
		}
	}

	return 0;
}

void
crb_reader_run(crb_reader_t *reader)
{
	pthread_t handler;
	if ( reader->running ) {
		return;
	}
		
	pthread_create( &handler, NULL, crb_reader_loop, (void*) reader );
	
}

void 
crb_reader_add_client(crb_reader_t *reader, crb_client_t *client)
{
	struct epoll_event event;
	
	reader->clients[reader->client_count] = client;
	reader->client_count++;
	
	event.data.ptr = client;
	event.events = EPOLLIN;
	
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_ADD, client->sock_fd, &event);
}

void 
crb_reader_drop_client(crb_reader_t *reader, crb_client_t *client)
{
	struct epoll_event event;
	
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL);
	close(client->sock_fd);
}
