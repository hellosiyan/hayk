#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>
#include <openssl/sha.h>
#include <signal.h>
#include <string.h>

#include "hk_crypt.h"
#include "hk_sender.h"
#include "hk_task.h"
#include "hk_channel.h"
#include "hk_ws.h"

static void hk_sender_task_broadcast(hk_task_t *task);
static void hk_sender_task_pong(hk_task_t *task);
static void hk_sender_task_handshake(hk_task_t *task);

hk_sender_t *
hk_sender_init()
{
    hk_sender_t *sender;

    sender = malloc(sizeof(hk_sender_t));
    if (sender == NULL) {
        return NULL;
    }
    
    sender->running = 0;
    
    sender->tasks = hk_task_queue_init();
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
hk_sender_free(hk_sender_t *sender)
{
	hk_sender_stop(sender);
	
	pthread_mutex_lock(sender->mu_tasks);
	hk_task_queue_free(sender->tasks);
	pthread_mutex_unlock(sender->mu_tasks);
	
	pthread_mutex_destroy(sender->mu_tasks);
	free(sender->mu_tasks);
	sem_destroy(&sender->sem_tasks);
	
	free(sender);
}

void 
hk_sender_add_task(hk_sender_t *sender, hk_task_t *task)
{
	hk_task_queue_push(sender->tasks, task);
}


static void* 
hk_sender_loop(void *data)
{
	hk_sender_t *sender;
	hk_task_t *task;
	int result;
	
	sender = (hk_sender_t *) data;
	
	sender->running = 1;
	while (sender->running) {
		result = sem_wait(&sender->sem_tasks);
		
		pthread_mutex_lock(sender->mu_tasks);
		task = hk_task_queue_pop(sender->tasks);
		pthread_mutex_unlock(sender->mu_tasks);
		
		if ( task ) {
			switch(task->type) {
				case HK_TASK_SHUTDOWN:
					// break out of the loop
					sender->running = 0;
					hk_task_free(task);
					break;
				case HK_TASK_BROADCAST:
					hk_sender_task_broadcast(task);
					break;
				case HK_TASK_PONG:
					hk_sender_task_pong(task);
					break;
				case HK_TASK_HANDSHAKE:
					hk_sender_task_handshake(task);
					break;
				default: 
					hk_log_error("Invalid task");
					hk_task_free(task);
					break;
			}
		}
	}

	return 0;
}

void
hk_sender_run(hk_sender_t *sender)
{
	if ( sender->running ) {
		return;
	}
		
	pthread_create( &(sender->thread_id), NULL, hk_sender_loop, (void*) sender );
	
}

void
hk_sender_stop(hk_sender_t *sender)
{
	hk_task_t *task;
	
	if ( !sender->running ) {
		return;
	}
	
	task = hk_task_init();
	task->type = HK_TASK_SHUTDOWN;
	
	pthread_mutex_lock(sender->mu_tasks);
	hk_sender_add_task(sender, task);
	pthread_mutex_unlock(sender->mu_tasks);
	sem_post(&sender->sem_tasks);
	
	pthread_join(sender->thread_id, NULL);
	
	sender->running = 0;
}

static void
hk_sender_task_broadcast(hk_task_t *task) {
	hk_channel_t *channel;
	hk_hash_cursor_t *cursor;
	hk_client_t *client;
	hk_ws_frame_t *frame;
	
	size_t data_offset, data_size;
	int bytes_written;
	
	uint8_t *header;
	int header_length;
	
	frame = task->data2;
	channel = task->data;
	
	data_offset = 0;
	data_size = frame->data_length;
	
	header = hk_ws_generate_frame_head(data_size, &header_length, 0, frame->opcode);
	if ( header == NULL ) {
		hk_log_error("Cannot create frame head");
		
		hk_ws_frame_free_with_data(frame);
		hk_task_free(task);
		return;
	}
	
	cursor = hk_hash_cursor_init(channel->clients);
	while ( (client = hk_hash_cursor_next(cursor)) != NULL ) {
		data_offset = 0;
		data_size = frame->data_length;
		
		if ( client->state == HK_STATE_OPEN /*&& client->sock_fd != task->client->sock_fd*/ ) {
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
	hk_hash_cursor_free(cursor);
	hk_ws_frame_free_with_data(frame);
	hk_task_free(task);
}

static void
hk_sender_task_pong(hk_task_t *task) {
	hk_client_t *client;
	hk_ws_frame_t *frame;
	
	size_t data_offset, data_size;
	int bytes_written;
	
	uint8_t *header;
	int header_length;
	
	frame = task->data2;
	client = task->client;
	
	data_offset = 0;
	data_size = frame->data_length;

	if ( client->state != HK_STATE_OPEN ) {
		hk_ws_frame_free_with_data(frame);
		hk_task_free(task);
		return;
	}
	
	header = hk_ws_generate_frame_head(data_size, &header_length, 0, HK_WS_PONG_FRAME);
	if ( header == NULL ) {
		hk_log_error("Cannot create frame head");
		
		hk_ws_frame_free_with_data(frame);
		hk_task_free(task);
		return;
	}
	
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
	
	free(header);
	hk_ws_frame_free_with_data(frame);
	hk_task_free(task);
}

static void 
hk_sender_task_handshake(hk_task_t *task)
{
	hk_http_request_t *request;
	hk_client_t *client;
	int headers_length = 0;
	char *headers;
	
	request = hk_request_init();
	if ( request == NULL ) {
		hk_log_error("Cannot create new request");
		return;
	}
	
	hk_request_ref(request);
	client = task->client;
	
	/* Construct request */
	hk_request_add_header(request, "Upgrade", -1, "websocket", -1);
	hk_request_add_header(request, "Connection", -1, "Upgrade", -1);
	
	/* Calculate Sec-WebSocket-Accept */
	{
		hk_http_header_t *key_header;
		char ws_sha1[SHA_DIGEST_LENGTH];
		char *ws_base64;
		int ws_base64_length = 0;
		SHA_CTX sha1;
		
		// Compute SHA1
		key_header = hk_request_get_header(task->client->request, HK_WS_KEY, -1);
		
		SHA1_Init(&sha1);
		SHA1_Update(&sha1, key_header->value, strlen(key_header->value));
		SHA1_Update(&sha1, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
		SHA1_Final(ws_sha1, &sha1);
		
		// Compute Base64
		ws_base64_length = (((SHA_DIGEST_LENGTH + 2) / 3) * 4);
		ws_base64 = malloc(ws_base64_length+1);
		if ( ws_base64 == NULL ) {
			hk_request_unref(request);
			hk_task_free(task);
			return;
		}
		
		ws_base64_length = hk_encode_base64(ws_base64, ws_sha1, SHA_DIGEST_LENGTH);
		
		hk_request_add_header(request, "Sec-WebSocket-Accept", -1, ws_base64, ws_base64_length);
		free(ws_base64);
	}
	
	/* Send request */
	
	write(client->sock_fd, "HTTP/1.1 101 Switching Protocols\r\n", 34);
	
	headers = hk_request_get_headers_string(request, &headers_length);
	write(client->sock_fd, headers, headers_length);
	
	free(headers);
	hk_request_unref(request);
	
	hk_task_free(task);
}
