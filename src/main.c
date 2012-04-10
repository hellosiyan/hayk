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

#include "crb_client.h"
#include "crb_reader.h"

#define SERVER_PORT 8080
#define MAX_QUERIES 200
#define BUFF_SIZE 20
#define MAXEVENTS 64


void 
check( int value )
{ 
	if( value < 0 )
	{
		perror("\nError");
  		exit( 1 );
 	}
}

int 
main()
{
	crb_reader_t* reader = crb_reader_init();
	crb_client_t *client;
	/* old code */
	
	int sock_desc, r_bind, r_listen, new_desc;
	int efd;
	int ci;
	struct epoll_event event;
	struct epoll_event *events;
	struct sockaddr_in address;
	pthread_t handler;
	
	crb_reader_run(reader);
	
	//init server structure
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVER_PORT);
	address.sin_addr.s_addr = INADDR_ANY;
	
	printf( "Initializing server....\n" );
	
	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	check( sock_desc );
	
	r_bind = bind( sock_desc, (struct sockaddr*)&address, sizeof( struct sockaddr_in ) );
	check( r_bind );
	
	
	//{
		int flags, s;

		flags = fcntl (sock_desc, F_GETFL, 0);
		if (flags == -1)
		{
			perror ("fcntl");
			return -1;
		}

		flags |= O_NONBLOCK;
		s = fcntl (sock_desc, F_SETFL, flags);
		if (s == -1)
		{
			perror ("fcntl");
			return -1;
		}
	//}
	
	r_listen = listen( sock_desc, MAX_QUERIES );
	check( r_listen );
	
	//{
		efd = epoll_create1 (0);
		if (efd == -1)
		{
			perror ("epoll_create");
			abort ();
		}

		event.data.fd = sock_desc;
		event.events = EPOLLIN | EPOLLET;
		r_listen = epoll_ctl (efd, EPOLL_CTL_ADD, sock_desc, &event);
		if (r_listen == -1)
		{
			perror ("epoll_ctl");
			abort ();
		}

		/* Buffer where events are returned */
		events = calloc (MAXEVENTS, sizeof event);
	//}

	
	while( 1 )
	{
		int n,i;
		
		n = epoll_wait (efd, events, MAXEVENTS, -1);
		
		printf("%i\n", events[i].data.fd);
		for (i = 0; i < n; i += 1)
		{
			
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(!(events[i].events & EPOLLIN)))
			{
				/* An error has occured on this fd, or the socket is not ready for reading (why were we notified then?) */
				printf ("epoll error\n");
				close (events[i].data.fd);
				continue;
			}
		
			if (sock_desc == events[i].data.fd)
			{
				/* We have a notification on the listening socket, which means one or more incoming connections. */
				while (1) {
					struct sockaddr_in client_adress;
					int cleint_addr_len = sizeof(client_adress);
		
					new_desc = accept( sock_desc, (struct sockaddr*)&client_adress, &cleint_addr_len);
				
				
					if (new_desc == -1) {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							/* We have processed all incoming connections. */
							break;
						} else {
							perror ("accept");
							break;
						}
					}
				
					{
						int flags, s;

						flags = fcntl (new_desc, F_GETFL, 0);
						if (flags == -1)
						{
							perror ("fcntl");
							return -1;
						}

						flags |= O_NONBLOCK;
						s = fcntl (new_desc, F_SETFL, flags);
						if (s == -1)
						{
							perror ("fcntl");
							return -1;
						}
					}
		
					printf( "\n** A new query has arrived from: %s **\n\n", inet_ntoa(client_adress.sin_addr));
					client = crb_client_init();
					client->sock_fd = new_desc;
					crb_reader_add_client(reader, client);
				}
			}
		}
		
 	}
 	
 	crb_reader_drop_all(reader);
 	close(sock_desc);

	return 0;
}
