#ifndef __CRB_CONFIG_H__
#define __CRB_CONFIG_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hk_list.h"
#include "hk_hash.h"

#define CRB_CONFIG PACKAGE_CONF_FILE

typedef struct hk_config_entry_s hk_config_entry_t;
struct hk_config_entry_s {
	struct in_addr host;
	uint16_t port;
	hk_hash_t *origins;
	uint32_t max_users;
};

typedef struct hk_config_s hk_config_t;
struct hk_config_s {
	hk_list_t *virtuals;
	hk_config_entry_t *defaults;
};


hk_config_entry_t * hk_config_entry_init();

hk_config_t * hk_config_init();
int hk_config_load(hk_config_t *config);

#endif /* __CRB_CONFIG_H__ */
