#include <stdio.h>
#include <fcntl.h>

#include "crb_worker.h"
#include "crb_log.h"

int log_file;

int
crb_log_init()
{
	log_file = open(CRB_LOGFILE, O_WRONLY|O_CREAT, 0740);
	if ( log_file == -1 ) {
		perror("Cannot open log file "CRB_LOGFILE);
		return -1;
	}
	
	return 0;
}

void
crb_log_info(char *msg)
{
	dprintf(log_file, "[INFO]  %s\n", msg);
}

void
crb_log_debug(char *msg)
{
	dprintf(log_file, "[DEBUG] %s\n", msg);
}

void
crb_log_error(char *msg)
{
	dprintf(log_file, "[ERROR] %s\n", msg);
}


