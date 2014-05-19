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

#include "hk_server.h"

static hk_server_t *server_inst;

static void hk_server_signals_init();
static void hk_server_sig(int signo);
static void _hk_server_stop(hk_server_t *server, int restart);

typedef struct {
	int signo;
	void (*handler)(int signo);
} hk_signal_t;

static hk_signal_t  signals[] = {
    { SIGINT, hk_server_sig },
    { SIGTERM, hk_server_sig },
    { SIGUSR1, hk_server_sig },
    { SIGUSR2, hk_server_sig },
    { SIGPIPE, SIG_IGN },
    { 0, NULL }
};

hk_server_t *
hk_server_init()
{
	hk_server_t *server;
	
	server = malloc(sizeof(hk_server_t));
	if ( server == NULL ) {
		return NULL;
	}
	server->restart = 0;
	
	server->config = hk_config_init();
	if ( !hk_config_load(server->config) ) {
		hk_log_error("Cannot load config file");
		return NULL;
	}
	
	server->workers = hk_list_init();
	
	hk_log_mark("Hayk "VERSION" - server started");
	printf("[OK]\n");
	
	// create default server
	{
		hk_worker_t *worker;
	
		worker = hk_worker_create(server->config->defaults);
		hk_list_push(server->workers, worker);
	}
	
	// create virtuals
	{
		hk_config_entry_t *virt_config;
		hk_list_item_t *item;
		
		item = server->config->virtuals->first;
	
		while ( item != NULL ) {
			virt_config = (hk_config_entry_t *) item->data;
			hk_worker_t *worker;
	
			worker = hk_worker_create(virt_config);
			hk_list_push(server->workers, worker);
			
			item = item->next;
		}
	}
	
	hk_server_signals_init();
	
	server_inst = server;
	
	return server;
}

void
hk_server_start(hk_server_t *server)
{
	if ( server == NULL ) {
		return;
	}
	
	/* Deamonize */
	daemon(0, 1);
	hk_write_pid();
	
	/* Loop */
	do {
		// start workers
		{
			hk_worker_t *worker;
			hk_list_item_t *item;
	
			item = server->workers->first;
	
			while ( item != NULL ) {
				worker = (hk_worker_t*) item->data;
				worker->pid = hk_worker_fork_and_run(worker);
				item = item->next;
			}
		}
		
		pause();
	} while ( server->restart );
	
	hk_clear_pid();
}


void
hk_server_start_single_proc(hk_server_t *server)
{
	if ( server == NULL ) {
		return;
	}
	
	hk_write_pid();
	
	// start worker
	{
		hk_worker_t *worker;
		hk_list_item_t *item;

		item = server->workers->first;

		worker = (hk_worker_t*) item->data;
		hk_worker_run(worker);
	}
	
	hk_clear_pid();
}

void
hk_server_stop(hk_server_t *server)
{
	_hk_server_stop(server, 0);
}

void
hk_server_restart(hk_server_t *server)
{
	_hk_server_stop(server, 1);
}

static void 
hk_server_signals_init()
{
    hk_signal_t  *sig;
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
hk_server_sig(int signo)
{
	switch(signo) {
		case SIGTERM:
		case SIGINT:
			hk_server_stop(server_inst);
			break;
		case SIGUSR1:
		case SIGUSR2:
			hk_server_restart(server_inst);
			break;
		default:
			break; 
	}
}


static void
_hk_server_stop(hk_server_t *server, int restart)
{
	hk_worker_t *worker;
	hk_list_item_t *item;
	
	item = server->workers->first;
	
	printf("Stopping server %i\n", getpid());
	
	while ( item != NULL ) {
		worker = (hk_worker_t*) item->data;
		hk_worker_stop(worker);
		item = item->next;
	}
	
	printf("Stopped server %i\n", getpid());
	
	server_inst->restart = restart;
}

void 
hk_server_call_restart(pid_t pid)
{
	kill(pid, SIGUSR1);
}

void 
hk_server_call_stop(pid_t pid)
{
	kill(pid, SIGINT);
	while( hk_read_pid() > 0 ) {
		sleep(1);
	}
}

pid_t
hk_read_pid()
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
hk_write_pid()
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
hk_clear_pid()
{
	int pid_file;
	pid_file = open(CRB_PIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0640);
	if ( pid_file < 0 ) {
		return;
	}
	
	close(pid_file);
}
