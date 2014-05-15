#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "lib/hayk.h"
#include "hk_server.h"

static void hk_print_help_and_exit();
static hk_server_command_e hk_parse_command_arg(char *command);

int 
main(int argc, char **argv)
{
	hk_server_t *server;
	hk_server_command_e command;
	pid_t server_pid;
	
	if ( argc != 2 ) {
		/* Wrong number of arguments. */
		hk_print_help_and_exit();
	}
	
	command = hk_parse_command_arg(argv[1]);
	server_pid = hk_read_pid();
	
	if ( command == CRB_SERVER_START || command == CRB_SERVER_START_DEBUG ) {
		if ( server_pid > 0 ) {
			printf("Server is already running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Starting server ...\n");
		server = hk_server_init();
		if (server == NULL ) {
			fprintf(stderr, "Error initializing server.\nAborting\n");
			return EXIT_FAILURE;
		}
		
		if ( command == CRB_SERVER_START ) {
			hk_server_start(server);
		} else {
			hk_server_start_single_proc(server);
		}
	} else if ( command == CRB_SERVER_STOP ) {
		if ( server_pid == 0 ) {
			printf("Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		hk_server_call_stop(server_pid);
		printf("[OK]\n");
	} else if ( command == CRB_SERVER_RESTART ) {
		if ( server_pid == 0 ) {
			printf("Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		hk_server_call_stop(server_pid);
		printf("[OK]\n");
		
		printf("Starting server ...\n");
		server = hk_server_init();
		if (server == NULL ) {
			fprintf(stderr, "Error initializing server.\nAborting\n");
			return EXIT_FAILURE;
		}
		
		hk_server_start(server);
	}
	
	return EXIT_SUCCESS;
}

static void
hk_print_help_and_exit()
{
	printf("Hayk - WebSocket Server\nUsage:\n  hayk {start|stop|restart}\n");
	exit(EXIT_SUCCESS);
}

static hk_server_command_e 
hk_parse_command_arg(char *command)
{
	if ( command == NULL ) {
		hk_print_help_and_exit();
	}
	
	if ( strcmp(command, "start") == 0 ) {
		return CRB_SERVER_START;
	} else if ( strcmp(command, "debug") == 0 ) {
		return CRB_SERVER_START_DEBUG;
	} else if ( strcmp(command, "stop") == 0 ) {
		return CRB_SERVER_STOP;
	} else if ( strcmp(command, "restart") == 0 ) {
		return CRB_SERVER_RESTART;
	}
	
	hk_print_help_and_exit();
}


