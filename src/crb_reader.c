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

#include "crb_reader.h"
#include "crb_buffer.h"
#include "crb_task.h"

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
	int n,i,ci, chars_read;
	struct epoll_event *events;
	crb_reader_t *reader;
	crb_client_t *client;
	char *buf[4096];
	buf[4095] = '\0';
	
	/*
	int dnull = open("/dev/null", O_WRONLY);
	
	int pfd[2];
	int pfd2[2];
	struct iovec iov;
	
	pipe(pfd);
	pipe(pfd2);
	*/
	reader = (crb_reader_t *) data;
	events = calloc (10, sizeof (struct epoll_event));
	
	reader->running = 1;
	while(reader->running) {
		n = epoll_wait (reader->epoll_fd, events, 10, 500);
		for (i = 0; i < n; i += 1) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP) || (!(events[i].events & EPOLLIN))) {
				/* Client closed or error occured */
				client = (crb_client_t *) events[i].data.ptr;
				crb_reader_drop_client(reader, client);
				continue;
			} else if (events[i].events & EPOLLIN) {
				client = (crb_client_t *) events[i].data.ptr;
				
				chars_read = read(client->sock_fd, buf, 50);
				if ( chars_read == 0 ) {
					continue;
				}
				
				while ( chars_read >= 0 ) {
					crb_buffer_append_string(client->buffer_in, (const char*)buf, chars_read);
					chars_read = read(client->sock_fd, buf, 50);
				}
				
				/*
				iov.iov_base = client->buffer_in->ptr;
				iov.iov_len = client->buffer_in->used-1;
				printf("vmspliced %li\n", vmsplice(pfd[1], &iov, 1, 0));
				
				for (ci = 0; ci < reader->client_count; ci += 1) {
					if ( reader->clients[ci] && reader->clients[ci]->id != client->id ) {
						printf("total %li\n", client->buffer_in->used-1);
						printf("tee %li\n", tee(pfd[0], pfd2[1], client->buffer_in->used-1, 0));
						printf("splice %li\n", splice(pfd2[0], NULL, reader->clients[ci]->sock_fd, NULL, client->buffer_in->used-1, 0));
					}
				}
				splice(pfd[0], NULL, dnull, NULL, client->buffer_in->used-1, 0);
				*/
				
				if ( strcmp(client->buffer_in->ptr, "exit") == 0 || strcmp(client->buffer_in->ptr, "exit\n") == 0 ) {
					//reader->running = 0;
					printf("STOP commmand recieved\n");
					crb_worker_stop();
				}
				
				// create task
				{
					crb_task_t *task;
					task = crb_task_init();
					task->client = client;
					task->type = CRB_TASK_BROADCAST;
					// TODO: replace this line
					task->data = (void *) crb_worker_register_channel("test");
					task->buffer = crb_buffer_copy(client->buffer_in);
				
					crb_worker_queue_task(task);
				
					crb_buffer_clear(client->buffer_in);
				}
			}
		}
	}
	
	free(events);
	
	printf("reader closed\n");

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
	
 	crb_reader_drop_all(reader);
	reader->running = 0;
}

void 
crb_reader_add_client(crb_reader_t *reader, crb_client_t *client)
{
	struct epoll_event event;
	
	reader->clients[reader->client_count] = client;
	client->id = reader->client_count;
	reader->client_count++;
	
	event.data.ptr = client;
	event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
	
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_ADD, client->sock_fd, &event);
}

void 
crb_reader_drop_client(crb_reader_t *reader, crb_client_t *client)
{
	epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL);
	crb_client_close(client); 
	reader->clients[client->id] = NULL;
}

void 
crb_reader_drop_all(crb_reader_t *reader)
{
	int ci;
	struct epoll_event event;
	
	for (ci = 0; ci < reader->client_count; ci += 1) {
		if ( !reader->clients[ci] ) {
			continue;
		}
		epoll_ctl (reader->epoll_fd, EPOLL_CTL_DEL, reader->clients[ci]->sock_fd, NULL);
		crb_client_close(reader->clients[ci]);
		reader->clients[ci] = NULL;
	}
}
