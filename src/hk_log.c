#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "hk_worker.h"
#include "hk_log.h"

int log_file;

int
hk_log_init()
{
	log_file = open(CRB_LOGFILE, O_WRONLY|O_CREAT|O_APPEND, 0740);
	if ( log_file == -1 ) {
		perror("Cannot open log file "CRB_LOGFILE);
		return -1;
	}
	
	return 0;
}

void
hk_log_info(char *msg)
{
	printf("[INFO]  %s\n", msg);
	dprintf(log_file, "[INFO]  %s\n", msg);
}

void
hk_log_debug(char *msg)
{
	printf("[DEBUG] %s\n", msg);
	dprintf(log_file, "[DEBUG] %s\n", msg);
}

void
hk_log_error(char *msg)
{
	printf("[ERROR] %s\n", msg);
	dprintf(log_file, "[ERROR] %s\n", msg);
}

void
hk_log_mark(char *msg)
{
	char *mark;
	time_t mark_time;
	
	time(&mark_time);
	
	asprintf(&mark, "\n[MARK]  %s[MARK]  %s\n", ctime(&mark_time), msg);
	
	dprintf(log_file, "%s", mark);
	
	free(mark);
}
