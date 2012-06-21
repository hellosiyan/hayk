#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>
#include <openssl/sha.h>

#include "crb_sender.h"
#include "crb_task.h"
#include "crb_channel.h"
#include "crb_crypt.h"
#include "crb_ws.h"

static void crb_sender_task_broadcast(crb_task_t *task);
static void crb_sender_task_handshake(crb_task_t *task);

crb_sender_t *
crb_sender_init()
{
    crb_sender_t *sender;

    sender = malloc(sizeof(crb_sender_t));
    if (sender == NULL) {
        return NULL;
    }
    
    sender->running = 0;
    
    sender->tasks = crb_task_queue_init();
    if ( sender->tasks == NULL ) {
		free(sender);
		return NULL;
    }
    
    sender->mu_tasks = malloc(sizeof(pthread_mutex_t));
    if ( sender->mu_tasks == NULL ) {
		free(sender->tasks);
    	free(sender);
		return NULL;
    }
    pthread_mutex_init(sender->mu_tasks, NULL);
    
    sem_init(&sender->sem_tasks, 0, 0);
    
    return sender;
}

void 
crb_sender_free(crb_sender_t *sender)
{
	crb_sender_stop(sender);
	
	pthread_mutex_lock(sender->mu_tasks);
	crb_task_queue_free(sender->tasks);
	pthread_mutex_unlock(sender->mu_tasks);
	
	pthread_mutex_destroy(sender->mu_tasks);
	free(sender->mu_tasks);
	sem_destroy(&sender->sem_tasks);
	
	free(sender);
}

void 
crb_sender_add_task(crb_sender_t *sender, crb_task_t *task)
{
	crb_task_queue_push(sender->tasks, task);
}


static void* 
crb_sender_loop(void *data)
{
	crb_sender_t *sender;
	crb_task_t *task;
	int result;
	
	sender = (crb_sender_t *) data;
	
	sender->running = 1;
	while (sender->running) {
		result = sem_wait(&sender->sem_tasks);
		
		pthread_mutex_lock(sender->mu_tasks);
		task = crb_task_queue_pop(sender->tasks);
		pthread_mutex_unlock(sender->mu_tasks);
		
		if ( task ) {
			switch(task->type) {
				case CRB_TASK_SHUTDOWN:
					// breaks out of the loop
					sender->running = 0;
					crb_task_free(task);
					break;
				case CRB_TASK_BROADCAST:
					crb_sender_task_broadcast(task);
					break;
				case CRB_TASK_HANDSHAKE:
					crb_sender_task_handshake(task);
					break;
				default: 
					crb_task_free(task);
					break;
			}
		}
	}

	return 0;
}

void
crb_sender_run(crb_sender_t *sender)
{
	if ( sender->running ) {
		return;
	}
		
	pthread_create( &(sender->thread_id), NULL, crb_sender_loop, (void*) sender );
	
}

void
crb_sender_stop(crb_sender_t *sender)
{
	crb_task_t *task;
	
	if ( !sender->running ) {
		return;
	}
	
	task = crb_task_init();
	task->type = CRB_TASK_SHUTDOWN;
	
	pthread_mutex_lock(sender->mu_tasks);
	crb_sender_add_task(sender, task);
	pthread_mutex_unlock(sender->mu_tasks);
	sem_post(&sender->sem_tasks);
	
	pthread_join(sender->thread_id, NULL);
	
	sender->running = 0;
}

static void
crb_sender_task_broadcast(crb_task_t *task) {
	crb_channel_t *channel = task->data;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(channel->clients);
	crb_client_t *client;
	crb_ws_frame_t *frame = task->data2;
	ssize_t data_offset = 0;
	ssize_t data_size = frame->data_length;
	int bytes_written;
	
	uint8_t *header;
	int header_length;
	
	header = crb_ws_frame_head_from_data(frame->data, data_size, &header_length, 0);
	
	while ( (client = crb_hash_cursor_next(cursor)) != NULL ) {
		data_offset = 0;
		
		if ( client->state == CRB_STATE_OPEN && client->sock_fd != task->client->sock_fd ) {
			bytes_written = write(client->sock_fd, (char*)header, header_length);
			
			bytes_written = write(client->sock_fd, frame->data, data_size);
			
			while (bytes_written > 0 && bytes_written < data_size) {
				data_offset += bytes_written;
				data_size -= bytes_written;
				
				do {
					errno == 0;
					bytes_written = write(client->sock_fd, frame->data + data_offset, data_size);
				} while (bytes_written == -1 && errno == 11);
			}
		}
	}
	
	free(header);
	crb_hash_cursor_free(cursor);
	free(frame->data);
	crb_ws_frame_free(frame);
	crb_task_free(task);
}

static void 
crb_sender_task_handshake(crb_task_t *task)
{
	crb_request_t *request;
	crb_client_t *client;
	int headers_length = 0;
	char *headers;
	
	request = crb_request_init();
	client = task->client;
	
	/* Construct request */
	crb_request_add_header(request, "Upgrade", -1, "websocket", -1);
	crb_request_add_header(request, "Connection", -1, "Upgrade", -1);
	
	/* Calculate Sec-WebSocket-Accept */
	{
		crb_header_t *key_header;
		char ws_sha1[SHA_DIGEST_LENGTH];
		char *ws_base64;
		char *ws_string = NULL;
		int ws_string_length = 0,
			ws_base64_length = 0;
		SHA_CTX sha1;
		
		// Compute SHA1
		key_header = crb_request_get_header(task->client->request, CRB_WS_KEY, -1);
		ws_string_length = asprintf(&ws_string, "%s%s", key_header->value, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		
		SHA1_Init(&sha1);
		SHA1_Update(&sha1, ws_string, ws_string_length);
		SHA1_Final(ws_sha1, &sha1);
		
		free(ws_string);
		
		// Compute Base64
		ws_base64_length = (((SHA_DIGEST_LENGTH + 2) / 3) * 4);
		ws_base64 = malloc(ws_base64_length+1);
		ws_base64_length = crb_encode_base64(ws_base64, ws_sha1, SHA_DIGEST_LENGTH);
		
		crb_request_add_header(request, "Sec-WebSocket-Accept", -1, ws_base64, ws_base64_length);
		free(ws_base64);
	}
	
	/* Send request */
	
	write(client->sock_fd, "HTTP/1.1 101 Switching Protocols\r\n", 34);
	
	headers = crb_request_get_headers_string(request, &headers_length);
	write(client->sock_fd, headers, headers_length);
	
	free(headers);
	crb_request_unref(request);
	
	crb_task_free(task);
}
