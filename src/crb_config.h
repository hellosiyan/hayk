#ifndef __CRB_CONFIG_H__
#define __CRB_CONFIG_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "crb_list.h"

#define CRB_CONFIG PACKAGE_CONF_FILE

typedef struct crb_config_entry_s crb_config_entry_t;
struct crb_config_entry_s {
	struct in_addr host;
	uint16_t port;
	const char *origin;
	uint32_t max_users;
	unsigned log:1;
};

typedef struct crb_config_s crb_config_t;
struct crb_config_s {
	crb_list_t *virtuals;
	crb_config_entry_t *defaults;
};


crb_config_entry_t * crb_config_entry_init();
void crb_config_item_free(crb_config_entry_t *config);

crb_config_t * crb_config_init();
int crb_config_load(crb_config_t *config);
void crb_config_free(crb_config_t *config);

#endif /* __CRB_CONFIG_H__ */
