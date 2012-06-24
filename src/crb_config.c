#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#include "crb_config.h"
#include "crb_list.h"
#include "crb_hash.h"

static int crb_config_parse_group(config_setting_t *group, crb_config_entry_t *config);

crb_config_t * 
crb_config_init()
{
	crb_config_t * conf;
	
	conf = malloc(sizeof(crb_config_t));
	if ( conf == NULL ) {
		return NULL;
	}
	
	conf->defaults = crb_config_entry_init();
	conf->virtuals = crb_list_init();
	
	return conf;
}

int
crb_config_load(crb_config_t *config)
{
	config_t cfg;
	config_setting_t *setting;
	config_setting_t *virtuals;
	
	if ( config == NULL ) {
		return 0;
	}

	config_init(&cfg);

	if(! config_read_file(&cfg, CRB_CONFIG)) {
		crb_log_error(config_error_text(&cfg));
		
		config_destroy(&cfg);
		
		return 0;
	}
	
	setting = config_lookup(&cfg, "default");
	if ( !crb_config_parse_group(setting, config->defaults) ) {
		crb_log_error("Default configuration missing");
		return 0;
	}
	
	virtuals = config_lookup(&cfg, "virtual");
	if ( virtuals != NULL ) {
		crb_config_entry_t *virtual;
		int count = config_setting_length(virtuals);
		int i;
		
		for (i = 0; i < count; ++i) {
			virtual = crb_config_entry_init();
			setting = config_setting_get_elem(virtuals, i);
			
			if ( !crb_config_parse_group(setting, virtual) ) {
				crb_log_error("Incomplete virtual configuration");
				return 0;
			}
			
			crb_list_push(config->virtuals, (void *)virtual);
		}
	}
	
  	config_destroy(&cfg);
	
	return 1;
}

static int
crb_config_parse_group(config_setting_t *group, crb_config_entry_t *config)
{
	const char *str;
	config_setting_t *origins;
	int num, result, i, length;
	
	if ( group == NULL || config == NULL ) {
		return 0;
	}
	
	result = config_setting_lookup_string(group, "host", &str);
	if ( result == CONFIG_FALSE ) {
		crb_log_error("Missing host declaration");
		return 0;
	}
	
	if ( str[0] == '*' ) {
		config->host.s_addr = INADDR_ANY;
	} else if ( !inet_aton(str, &(config->host)) ) {
		crb_log_error("Invalid host");
		return 0;
	}
	
	result = config_setting_lookup_int(group, "port", &num);
	if ( result == CONFIG_FALSE ) {
		crb_log_error("Missing port declaration");
		return 0;
	}
	config->port = num;
	
	origins = config_setting_get_member(group, "origin");
	config->origins = NULL;
	if ( origins != CONFIG_FALSE ) {
		length = config_setting_length(origins);
		if ( length > 0 ) {
			config->origins = crb_hash_init(5);
			for (i = 0; i < length; i += 1) {
				str = config_setting_get_string_elem(origins, i);
				if ( str != NULL ) {
					crb_hash_insert(config->origins, (void*)str, (void*)str, strlen(str));
				}
			}
		}
	}
	
	result = config_setting_lookup_int(group, "max-users", &num);
	if ( result == CONFIG_FALSE ) {
		crb_log_error("Missing max-users declaration");
		return 0;
	}
	config->max_users = num;
	
	return 1;
}

crb_config_entry_t *
crb_config_entry_init()
{
	crb_config_entry_t * entry;
	
	entry = malloc(sizeof(crb_config_entry_t));
	if ( entry == NULL ) {
		return NULL;
	}
	
	entry->host.s_addr = INADDR_ANY;
	entry->port = 0;
	entry->origins = NULL;
	entry->max_users = 0;
	
	return entry;
}

