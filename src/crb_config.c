#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libconfig.h>

#include "crb_config.h"
#include "crb_list.h"

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

	config_init(&cfg);

	if(! config_read_file(&cfg, CRB_CONFIG)) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		
		config_destroy(&cfg);
		
		return 0;
	}
	
	setting = config_lookup(&cfg, "default");
	if ( !crb_config_parse_group(setting, config->defaults) ) {
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
				return 0;
			}
			
			crb_list_push(config->virtuals, (void *)virtual);
		}
	}
	
	return 1;
}

void
crb_config_free(crb_config_t *config)
{
	// TODO
}

static int
crb_config_parse_group(config_setting_t *group, crb_config_entry_t *config)
{
	const char *str;
	int num, result;
	
	if ( group == NULL || config == NULL ) {
		return 0;
	}
	
	result = config_setting_lookup_string(group, "host", &str);
	if ( result == CONFIG_FALSE ) {
		printf("ERROR: Missing host declaration\n");
		return 0;
	}
	
	if ( str[0] == '*' ) {
		config->host.s_addr = INADDR_ANY;
	} else if ( !inet_aton(str, &(config->host)) ) {
		printf("ERROR: Invalid host\n");
		return 0;
	}
	
	result = config_setting_lookup_int(group, "port", &num);
	if ( result == CONFIG_FALSE ) {
		printf("ERROR: Missing port declaration\n");
		return 0;
	}
	config->port = num;
	
	result = config_setting_lookup_string(group, "origin", &str);
	if ( result == CONFIG_FALSE ) {
		printf("ERROR: Missing origin declaration\n");
		return 0;
	}
	config->origin = str;
	
	result = config_setting_lookup_int(group, "max-users", &num);
	if ( result == CONFIG_FALSE ) {
		printf("ERROR: Missing max-users declaration\n");
		return 0;
	}
	config->max_users = num;
	
	result = config_setting_lookup_bool(group, "log", &num);
	if ( result == CONFIG_FALSE ) {
		printf("ERROR: Missing log declaration\n");
		return 0;
	}
	config->log = num;
	
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
	entry->origin = NULL;
	entry->max_users = 0;
	entry->log = 1;
	
	return entry;
}

void 
crb_config_entry_free (crb_config_entry_t *config)
{
	
}
