#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "crb_worker.h"
#include "crb_client.h"
#include "crb_reader.h"
#include "crb_list.h"

#define SERVER_PORT 8080

static crb_worker_t *worker;

void 
crb_worker_create()
{
	worker = malloc(sizeof(crb_worker_t));
	
	worker->channels = crb_list_init(16);
	worker->readers = crb_list_init(16);
	worker->senders = crb_list_init(16);
}

crb_worker_t *
crb_worker_get()
{
	return worker;
}

int 
crb_worker_run()
{
	int sock_desc, new_desc;
	int flags;
	int result;
	struct sockaddr_in address;
	
	crb_client_t *client;
	crb_reader_t* reader = crb_reader_init();
	
	// listen address
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVER_PORT);
	address.sin_addr.s_addr = INADDR_ANY;
	
	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	if ( sock_desc == -1 ) {
		perror("socket");
		return -1;
	}
	
	result = bind( sock_desc, (struct sockaddr*)&address, sizeof( struct sockaddr_in ) );
	if ( result == -1 ) {
		perror("bind");
		return -1;
	}
	
	result = listen( sock_desc, 100 );
	if ( result == -1 ) {
		perror("listen");
		return -1;
	}
	
	crb_reader_run(reader);
	
	while (1) {
		new_desc = accept( sock_desc, NULL, NULL);
	
		flags = fcntl (new_desc, F_GETFL, 0);
		if (flags == -1) {
			perror ("fcntl");
			close(new_desc);
			continue;
		}

		flags |= O_NONBLOCK;
		result = fcntl (new_desc, F_SETFL, flags);
		if (result == -1) {
			close(new_desc);
			continue;
		}

		client = crb_client_init();
		client->sock_fd = new_desc;
		crb_reader_add_client(reader, client);
	}
 	
 	crb_reader_drop_all(reader);
 	close(sock_desc);

	return 0;
}

