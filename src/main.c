#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "crb_worker.h"
#include "crb_server.h"

static void crb_print_help_and_exit();
static crb_server_command_e crb_parse_command_arg(char *command);

int 
main(int argc, char **argv)
{
	crb_server_t *server;
	crb_server_command_e command;
	pid_t server_pid;
	
	if ( argc != 2 ) {
		/* Wrong number of arguments. */
		crb_print_help_and_exit();
	}
	
	command = crb_parse_command_arg(argv[1]);
	server_pid = crb_read_pid();
	
	if ( command == CRB_SERVER_START ) {
		if ( server_pid > 0 ) {
			printf("Server is already running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Starting server ...\n");
		server = crb_server_init();
		if (server == NULL ) {
			fprintf(stderr, "Error initializing server.\nAborting\n");
			return EXIT_FAILURE;
		}
		
		crb_server_start(server);
	} else if ( command == CRB_SERVER_STOP ) {
		if ( server_pid == 0 ) {
			printf("Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		crb_server_call_stop(server_pid);
		printf("[OK]\n");
	} else if ( command == CRB_SERVER_RESTART ) {
		if ( server_pid == 0 ) {
			printf("Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		crb_server_call_stop(server_pid);
		printf("[OK]\n");
		
		printf("Starting server ...\n");
		server = crb_server_init();
		if (server == NULL ) {
			fprintf(stderr, "Error initializing server.\nAborting\n");
			return EXIT_FAILURE;
		}
		
		crb_server_start(server);
	}
	
	return EXIT_SUCCESS;
}

static void
crb_print_help_and_exit()
{
	printf("Caribou - WebSocket Server\nUsage:\n  caribou {start|stop|restart}\n");
	exit(EXIT_SUCCESS);
}

static crb_server_command_e 
crb_parse_command_arg(char *command)
{
	if ( command == NULL ) {
		crb_print_help_and_exit();
	}
	
	if ( strcmp(command, "start") == 0 ) {
		return CRB_SERVER_START;
	} else if ( strcmp(command, "stop") == 0 ) {
		return CRB_SERVER_STOP;
	} else if ( strcmp(command, "restart") == 0 ) {
		return CRB_SERVER_RESTART;
	}
	
	crb_print_help_and_exit();
}


