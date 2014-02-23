#ifndef __CRB_SERVER_H__
#define __CRB_SERVER_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"

#include "crb_list.h"
#include "crb_worker.h"
#include "crb_log.h"

#define CRB_CONFIG PACKAGE_CONF_FILE
#define CRB_PIDFILE "/tmp/ws-caribou-VERSION.pid"

typedef enum {
     CRB_SERVER_START = 0,
     CRB_SERVER_START_DEBUG,
     CRB_SERVER_STOP,
     CRB_SERVER_RESTART
} crb_server_command_e;

typedef struct crb_server_s crb_server_t;
struct crb_server_s {
	crb_list_t *workers;
	crb_config_t *config;
	unsigned restart:1;
};

crb_server_t *crb_server_init();
void crb_server_start(crb_server_t *server);
void crb_server_start_single_proc(crb_server_t *server);
void crb_server_restart(crb_server_t *server);
void crb_server_stop(crb_server_t *server);

void crb_server_call_restart(pid_t pid);
void crb_server_call_stop(pid_t pid);

pid_t crb_read_pid();
pid_t crb_write_pid();
void crb_clear_pid();

#endif /* __CRB_SERVER_H__ */
