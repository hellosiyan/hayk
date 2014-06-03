#ifndef __HK_SERVER_H__
#define __HK_SERVER_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"

#include "lib/hayk.h"

#define HK_CONFIG PACKAGE_CONF_FILE
#define HK_PIDFILE "/tmp/ws-hayk-VERSION.pid"
#define HK_LOGFILE PACKAGE_DATA_DIR "/hayk.log"

typedef enum {
     HK_SERVER_START = 0,
     HK_SERVER_START_DEBUG,
     HK_SERVER_STOP,
     HK_SERVER_RESTART
} hk_server_command_e;

int hk_server_init();

void hk_server_start();
void hk_server_start_single_proc();
void hk_server_stop();

int hk_server_is_running();

#endif /* __HK_SERVER_H__ */
