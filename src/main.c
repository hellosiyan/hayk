#include <stdio.h>
#include <stdlib.h>

 #include <libconfig.h>

#include "crb_worker.h"
#include "crb_config.h"

int 
main()
{
	crb_config_t *conf;
	
	conf = crb_config_init();
	if ( !crb_config_load(conf) ) {
		printf("Aborting\n");
		return 0;
	}
	
	crb_worker_create(conf->defaults);
	crb_worker_run();

	return 0;
}
