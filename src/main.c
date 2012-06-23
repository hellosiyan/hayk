#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

 #include <libconfig.h>

#include "crb_worker.h"
#include "crb_config.h"
#include "crb_server.h"

int 
main()
{
	crb_server_t *server;
	
	server = crb_server_init();
	crb_server_start(server);
	
	/*
	pid_t pid;
	crb_config_t *conf;
	
	
	conf = crb_config_init();
	if ( !crb_config_load(conf) ) {
		printf("Aborting\n");
		return 0;
	}
	
	crb_worker_create(conf->defaults);
	crb_worker_run();
	*/
	return 0;
}
