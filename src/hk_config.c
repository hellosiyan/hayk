#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#include "hk_config.h"
#include "hk_list.h"
#include "hk_hash.h"

static int hk_config_parse_group(config_setting_t *group, hk_config_entry_t *config);

hk_config_t * 
hk_config_init()
{
	hk_config_t * conf;
	
	conf = malloc(sizeof(hk_config_t));
	if ( conf == NULL ) {
		return NULL;
	}
	
	conf->defaults = hk_config_entry_init();
	conf->virtuals = hk_list_init();
	
	return conf;
}

int
hk_config_load(hk_config_t *config)
{
	config_t cfg;
	config_setting_t *setting;
	config_setting_t *virtuals;
	
	if ( config == NULL ) {
		return 0;
	}

	config_init(&cfg);

	if(! config_read_file(&cfg, CRB_CONFIG)) {
		hk_log_error(config_error_text(&cfg));
		
		config_destroy(&cfg);
		
		return 0;
	}
	
	setting = config_lookup(&cfg, "default");
	if ( !hk_config_parse_group(setting, config->defaults) ) {
		hk_log_error("Default configuration missing");
		return 0;
	}
	
	virtuals = config_lookup(&cfg, "virtual");
	if ( virtuals != NULL ) {
		hk_config_entry_t *virtual;
		int count = config_setting_length(virtuals);
		int i;
		
		for (i = 0; i < count; ++i) {
			virtual = hk_config_entry_init();
			setting = config_setting_get_elem(virtuals, i);
			
			if ( !hk_config_parse_group(setting, virtual) ) {
				hk_log_error("Incomplete virtual configuration");
				return 0;
			}
			
			hk_list_push(config->virtuals, (void *)virtual);
		}
	}
	
  	config_destroy(&cfg);
	
	return 1;
}

static int
hk_config_parse_group(config_setting_t *group, hk_config_entry_t *config)
{
	const char *str;
	config_setting_t *origins;
	int num, result, i, length;
	
	if ( group == NULL || config == NULL ) {
		return 0;
	}
	
	result = config_setting_lookup_string(group, "host", &str);
	if ( result == CONFIG_FALSE ) {
		hk_log_error("Missing host declaration");
		return 0;
	}
	
	if ( str[0] == '*' ) {
		config->host.s_addr = INADDR_ANY;
	} else if ( !inet_aton(str, &(config->host)) ) {
		hk_log_error("Invalid host");
		return 0;
	}
	
	result = config_setting_lookup_int(group, "port", &num);
	if ( result == CONFIG_FALSE ) {
		hk_log_error("Missing port declaration");
		return 0;
	}
	config->port = num;
	
	origins = config_setting_get_member(group, "origin");
	config->origins = NULL;
	if ( origins != CONFIG_FALSE ) {
		length = config_setting_length(origins);
		if ( length > 0 ) {
			config->origins = hk_hash_init(5);
			for (i = 0; i < length; i += 1) {
				str = config_setting_get_string_elem(origins, i);
				if ( str != NULL ) {
					hk_hash_insert(config->origins, (void*)str, (void*)str, strlen(str));
				}
			}
		}
	}
	
	result = config_setting_lookup_int(group, "max-users", &num);
	if ( result == CONFIG_FALSE ) {
		hk_log_error("Missing max-users declaration");
		return 0;
	}
	config->max_users = num;
	
	return 1;
}

hk_config_entry_t *
hk_config_entry_init()
{
	hk_config_entry_t * entry;
	
	entry = malloc(sizeof(hk_config_entry_t));
	if ( entry == NULL ) {
		return NULL;
	}
	
	entry->host.s_addr = INADDR_ANY;
	entry->port = 0;
	entry->origins = NULL;
	entry->max_users = 0;
	
	return entry;
}

