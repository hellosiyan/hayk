#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "crb_server.h"
#include "crb_list.h"
#include "crb_worker.h"

static crb_server_t *server_inst;

static void crb_server_signals_init();
static void crb_server_sig(int signo);
static void _crb_server_stop(crb_server_t *server, int restart);

typedef struct {
	int signo;
	void (*handler)(int signo);
} crb_signal_t;

static crb_signal_t  signals[] = {
    { SIGINT, crb_server_sig },
    { SIGTERM, crb_server_sig },
    { SIGUSR1, crb_server_sig },
    { SIGUSR2, crb_server_sig },
    { SIGPIPE, SIG_IGN },
    { 0, NULL }
};

crb_server_t *
crb_server_init()
{
	crb_server_t *server;
	
	server = malloc(sizeof(crb_server_t));
	if ( server == NULL ) {
		return NULL;
	}
	
	server->restart = 0;
	
	server->config = crb_config_init();
	if ( !crb_config_load(server->config) ) {
		printf("Aborting\n");
		return 0;
	}
	
	server->workers = crb_list_init();
	
	server_inst = server;
	
	printf("Started server %i\n", getpid());
	
	// create default server
	{
		crb_worker_t *worker;
	
		worker = crb_worker_create(server->config->defaults);
	
		crb_list_push(server->workers, worker);
	}
	
	// create virtuals
	{
		crb_config_entry_t *virt_config;
		crb_list_item_t *item;
		
		item = server->config->virtuals->first;
	
		while ( item != NULL ) {
			virt_config = (crb_config_entry_t *) item->data;
			printf("Create Virtual\n");
			crb_worker_t *worker;
	
			worker = crb_worker_create(virt_config);
	
			crb_list_push(server->workers, worker);
			
			item = item->next;
		}
	}
	
	crb_server_signals_init();
	
	return server;
}

void
crb_server_start(crb_server_t *server)
{
	do {
		// start workers
		{
			crb_worker_t *worker;
			crb_list_item_t *item;
	
			item = server->workers->first;
	
			while ( item != NULL ) {
				worker = (crb_worker_t*) item->data;
				worker->pid = crb_worker_run(worker);
				item = item->next;
			}
		}
		
		pause();
	} while ( server->restart );
}

void
crb_server_stop(crb_server_t *server)
{
	_crb_server_stop(server, 0);
}

void
crb_server_restart(crb_server_t *server)
{
	_crb_server_stop(server, 1);
}

static void 
crb_server_signals_init()
{
    crb_signal_t      *sig;
    struct sigaction   sa;

    for (sig = signals; sig->signo != 0; sig++) {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            break;
        }
    }
}

static void
crb_server_sig(int signo)
{
	switch(signo) {
		case SIGTERM:
		case SIGINT:
			crb_server_stop(server_inst);
			break;
		case SIGUSR1:
		case SIGUSR2:
			crb_server_restart(server_inst);
			break;
		default:
			break; 
	}
}


static void
_crb_server_stop(crb_server_t *server, int restart)
{
	crb_worker_t *worker;
	crb_list_item_t *item;
	
	item = server->workers->first;
	
	while ( item != NULL ) {
		worker = (crb_worker_t*) item->data;
		crb_worker_stop(worker);
		item = item->next;
	}
	
	server_inst->restart = restart;
}
