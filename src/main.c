#include <stdio.h>
#include <stdlib.h>

#include "crb_worker.h"

int 
main()
{
	crb_worker_create();
	crb_worker_run();

	return 0;
}
