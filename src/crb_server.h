#ifndef __CRB_SERVER_H__
#define __CRB_SERVER_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "crb_list.h"
#include "crb_worker.h"

#define CRB_CONFIG PACKAGE_CONF_FILE

typedef struct crb_server_s crb_server_t;
struct crb_server_s {
	crb_list_t *workers;
	crb_config_t *config;
	unsigned restart:1;
};

crb_server_t *crb_server_init();
void crb_server_start(crb_server_t *server);
void crb_server_restart(crb_server_t *server);
void crb_server_stop(crb_server_t *server);

#endif /* __CRB_SERVER_H__ */
