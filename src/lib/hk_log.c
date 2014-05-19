#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "hk_log.h"

int log_file = 0;
char *log_file_path = NULL;
hk_log_level_e log_level = HK_LOG_MARK | HK_LOG_INFO | HK_LOG_DEBUG | HK_LOG_ERROR;
hk_log_destination_e log_destination = HK_LOG_STDERR;

int
hk_log_init(hk_log_level_e level, hk_log_destination_e destination, const char *file_path)
{
	if ( log_file ) {
		close(log_file);
		log_file = 0;
	}

	if ( log_file_path != NULL ) {
		free(log_file_path);
		log_file_path = NULL;
	}

	log_level = level;
	log_destination = destination;

	if ( file_path != NULL ) {
		log_file_path = strdup(file_path);
	}

	if ( log_destination & HK_LOG_FILE ) {
		log_file = open(log_file_path, O_WRONLY|O_CREAT|O_APPEND, 0740);

		if ( log_file == -1 ) {
			perror("Cannot open log file");
			return -1;
		}
	}
	
	return 0;
}

void
hk_log_info(char *msg)
{
	if ( log_level & HK_LOG_INFO == 0) {
		return;
	}

	if ( log_destination & HK_LOG_STDERR ) {
		printf("[INFO]  %s\n", msg);
	}
	

	if ( log_destination & HK_LOG_FILE ) {
		dprintf(log_file, "[INFO]  %s\n", msg);
	}
}

void
hk_log_debug(char *msg)
{
	if ( log_level & HK_LOG_DEBUG == 0) {
		return;
	}

	if ( log_destination & HK_LOG_STDERR ) {
		printf("[DEBUG] %s\n", msg);
	}
	

	if ( log_destination & HK_LOG_FILE ) {
		dprintf(log_file, "[DEBUG] %s\n", msg);
	}
}

void
hk_log_error(char *msg)
{
	if ( log_level & HK_LOG_DEBUG == 0) {
		return;
	}

	if ( log_destination & HK_LOG_STDERR ) {
		printf("[ERROR] %s\n", msg);
	}
	

	if ( log_destination & HK_LOG_FILE ) {
		dprintf(log_file, "[ERROR] %s\n", msg);
	}
}

void
hk_log_mark(char *msg)
{
	char *mark;
	time_t mark_time;

	// marks should appear only in log files
	if ( log_level & HK_LOG_MARK == 0 || log_destination & HK_LOG_FILE == 0) {
		return;
	}
	
	time(&mark_time);
	asprintf(&mark, "\n[MARK]  %s[MARK]  %s\n", ctime(&mark_time), msg);
	
	dprintf(log_file, mark);
	
	free(mark);
}

