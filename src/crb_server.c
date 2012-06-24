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
#include <fcntl.h>
#include <errno.h>

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
	
	if ( crb_log_init() == -1 ) {
		crb_log_error("Cannot open log file");
		return NULL;
	}
	
	server->config = crb_config_init();
	if ( !crb_config_load(server->config) ) {
		crb_log_error("Cannot load config file");
		return NULL;
	}
	
	server->workers = crb_list_init();
	
	crb_log_mark("Caribou "VERSION" - server started");
	printf("[OK]\n");
	
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
			crb_worker_t *worker;
	
			worker = crb_worker_create(virt_config);
	
			crb_list_push(server->workers, worker);
			
			item = item->next;
		}
	}
	
	crb_server_signals_init();
	
	server_inst = server;
	
	return server;
}

void
crb_server_start(crb_server_t *server)
{
	if ( server == NULL ) {
		return;
	}
	
	/* Deamonize */
	// daemon(0, 1);
	crb_write_pid();
	
	/* Loop */
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
	
	crb_clear_pid();
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
    crb_signal_t  *sig;
    struct sigaction sa;

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
	
	printf("Stopping server %i\n", getpid());
	
	while ( item != NULL ) {
		worker = (crb_worker_t*) item->data;
		crb_worker_stop(worker);
		item = item->next;
	}
	
	printf("Stopped server %i\n", getpid());
	
	server_inst->restart = restart;
}

void 
crb_server_call_restart(pid_t pid)
{
	kill(pid, SIGUSR1);
}

void 
crb_server_call_stop(pid_t pid)
{
	kill(pid, SIGINT);
	while( crb_read_pid() > 0 ) {
		sleep(1);
	}
}

pid_t
crb_read_pid()
{
	int pid_file;
	ssize_t bytes_read;
	pid_t pid;
	
	pid_file = open(CRB_PIDFILE,O_RDONLY|O_CREAT,0640);
	
	if ( pid_file < 0 ) {
		printf("Unable to open pid file \"%s\"\n", CRB_PIDFILE );
		exit(EXIT_FAILURE);
	}
	
	bytes_read = read(pid_file, &pid, sizeof(pid_t));
	close(pid_file);
	
	if ( bytes_read < sizeof(pid_t)) {
		return 0;
	}
	
	if ( kill(pid, 0) == -1 && errno == ESRCH ) {
		return 0;
	}
	
	return pid;
}

pid_t
crb_write_pid()
{
	int pid_file;
	ssize_t bytes_written;
	pid_t pid = getpid();
	
	pid_file = open(CRB_PIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0640);
	
	if ( pid_file < 0 ) {
		printf("Unable to open pid file \"%s\"\n", CRB_PIDFILE );
		exit(EXIT_FAILURE);
	}
	
	bytes_written = write(pid_file, &pid, sizeof(pid_t));
	close(pid_file);
	
	if ( bytes_written < sizeof(pid_t)) {
		return 0;
	}
	
	return pid;
}

void
crb_clear_pid()
{
	int pid_file;
	
	pid_file = open(CRB_PIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0640);
	if ( pid_file < 0 ) {
		return;
	}
	
	close(pid_file);
}
