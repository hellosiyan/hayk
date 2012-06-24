#ifndef __CRB_LOG_H__
#define __CRB_LOG_H__ 1

#include <stdio.h>
#include <time.h>

#include "crb_worker.h"

// #define CRB_LOGFILE PACKAGE_DATA_DIR "/caribou.log"
#define CRB_LOGFILE "/home/siyan/caribou.log"

int crb_log_init();
void crb_log_info(char *msg);
void crb_log_debug(char *msg);
void crb_log_error(char *msg);
void crb_log_mark(char *msg);

#endif /* __CRB_LOG_H__ */
