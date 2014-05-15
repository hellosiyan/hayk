#ifndef __CRB_LOG_H__
#define __CRB_LOG_H__ 1

#include <stdio.h>
#include <time.h>

#include "hk_worker.h"

#define CRB_LOGFILE PACKAGE_DATA_DIR "/hayk.log"

int hk_log_init();
void hk_log_info(char *msg);
void hk_log_debug(char *msg);
void hk_log_error(char *msg);
void hk_log_mark(char *msg);

#endif /* __CRB_LOG_H__ */
