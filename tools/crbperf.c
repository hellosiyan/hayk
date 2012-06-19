#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define BUFF_SIZE 2
#define REP_COUNT 10

void check( int value )
{
	if( value < 0 ) 
	{
		perror("\n Error: ");
   		exit( 1 );
 	}
}

void send_query( int sock_desc, char query[], int size, int rep )
{
	int i;
	
	for (i = 0; i < rep; i += 1) {
		printf("write %i\n", write( sock_desc, query, size ));
	}
}

int main()
{
	int sock_desc, r_bind, r_connect;
	struct sockaddr_in server_address, client_address;
	char *query;
	
	//init server structure
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	
	// Server IP addres - only an example
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
 	
	//init client structure
	client_address.sin_family = AF_INET;
	client_address.sin_port = 0;
	client_address.sin_addr.s_addr = INADDR_ANY;

	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	
	check( sock_desc );
	
	r_bind = bind( sock_desc, (struct sockaddr*)&client_address, sizeof( struct sockaddr ) );
	check( r_bind );
	
	r_connect = connect( sock_desc, (struct sockaddr*)&server_address, sizeof( struct sockaddr ) );
	check( r_connect );
	
	/* simple client header */
	query = "GET /chat HTTP/1.1\nHost: server.example.com\nUpgrade: websocket\nConnection: Upgrade\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\nOrigin: http://example.com\nSec-WebSocket-Protocol: chat, superchat\nSec-WebSocket-Version: 13\n\n\0";

	send_query( sock_desc, query, strlen(query)+1, 1);
	
	close(r_connect);
	sleep(1);

	return 0;
}
