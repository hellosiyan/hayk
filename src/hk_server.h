#ifndef __CRB_SERVER_H__
#define __CRB_SERVER_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"

#include "lib/hayk.h"

#define CRB_CONFIG PACKAGE_CONF_FILE
#define CRB_PIDFILE "/tmp/ws-hayk-VERSION.pid"

typedef enum {
     CRB_SERVER_START = 0,
     CRB_SERVER_START_DEBUG,
     CRB_SERVER_STOP,
     CRB_SERVER_RESTART
} hk_server_command_e;

typedef struct hk_server_s hk_server_t;
struct hk_server_s {
	hk_list_t *workers;
	hk_config_t *config;
	unsigned restart:1;
};

hk_server_t *hk_server_init();
void hk_server_start(hk_server_t *server);
void hk_server_start_single_proc(hk_server_t *server);
void hk_server_restart(hk_server_t *server);
void hk_server_stop(hk_server_t *server);

void hk_server_call_restart(pid_t pid);
void hk_server_call_stop(pid_t pid);

pid_t hk_read_pid();
pid_t hk_write_pid();
void hk_clear_pid();

#endif /* __CRB_SERVER_H__ */
