#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	hk_server_command_e command;
	int init_result;
	
	if ( argc != 2 ) {
		/* Wrong number of arguments. */
		hk_print_help_and_exit();
	}

	init_result = hk_server_init();
	if (init_result != 0 ) {
		fprintf(stderr, "Error initializing server.\nAborting\n");
		return EXIT_FAILURE;
	}
	
	command = hk_parse_command_arg(argv[1]);
	
	if ( command == HK_SERVER_START || command == HK_SERVER_START_DEBUG ) {
		if ( hk_server_is_running() ) {
			fprintf(stderr, "Server is already running.\n");
			return EXIT_FAILURE;
		}
		
		if ( command == HK_SERVER_START_DEBUG ) {
			hk_server_start_single_proc();
		} else {
			printf("Starting server ...\n");
			hk_server_start();
			printf("[OK]\n");
		}
	} else if ( command == HK_SERVER_STOP ) {
		if ( ! hk_server_is_running() ) {
			fprintf(stderr, "Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		hk_server_stop();

		printf("[OK]\n");
	} else if ( command == HK_SERVER_RESTART ) {
		if ( ! hk_server_is_running() ) {
			fprintf(stderr, "Server is not running.\n");
			return EXIT_FAILURE;
		}
		
		printf("Stopping server ...\n");
		hk_server_stop();

		printf("[OK]\nStarting server ...\n");
		hk_server_start();
		printf("[OK]\n");
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
		return HK_SERVER_START;
	} else if ( strcmp(command, "debug") == 0 ) {
		return HK_SERVER_START_DEBUG;
	} else if ( strcmp(command, "stop") == 0 ) {
		return HK_SERVER_STOP;
	} else if ( strcmp(command, "restart") == 0 ) {
		return HK_SERVER_RESTART;
	}
	
	hk_print_help_and_exit();
	
	return HK_SERVER_STOP;
}


