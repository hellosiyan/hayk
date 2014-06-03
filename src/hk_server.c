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

typedef struct hk_server_s hk_server_t;
struct hk_server_s {
	hk_list_t *workers;
	hk_config_t *config;
	unsigned restart:1;
};

static hk_server_t *server_inst;

static hk_server_t *hk_server();
static pid_t hk_read_pid();
static pid_t hk_write_pid();
static void hk_clear_pid();

static void hk_server_stop_real();
static void hk_server_restart_real();

static void hk_server_signals_init();
static void hk_server_sig(int signo);
static void hk_server_stop_workers();
static hk_worker_t *hk_server_create_worker(hk_server_t *server, hk_config_entry_t *config);

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

static hk_server_t *
hk_server()
{
	return server_inst;
}

int
hk_server_init()
{
	hk_server_t *server = hk_server();
	
	if ( server != NULL ) {
		hk_log_error("Server already initialized");
		return -1;
	}
	
	server = malloc(sizeof(hk_server_t));
	if ( server == NULL ) {
		return -1;
	}

	server->restart = 0;

	if ( hk_log_init(HK_LOG_ALL, HK_LOG_STDERR | HK_LOG_FILE, HK_LOGFILE) == -1 ) {
		fprintf(stderr, "Cannot open log file "HK_LOGFILE);
		return EXIT_FAILURE;
	}
	
	server->config = hk_config_init();
	if ( !hk_config_load(server->config) ) {
		hk_log_error("Cannot load config file");
		return -1;
	}
	
	server->workers = hk_list_init();
	
	hk_server_signals_init();
	
	server_inst = server;
	
	return 0;
}

void
hk_server_start()
{
	pid_t pid;
	hk_server_t *server = hk_server();

	if ( server == NULL ) {
		return;
	}
	
	/* Deamonize */
	pid = fork();
	if ( pid != 0 ) {
		return;
	}
	daemon(0, 0);
	hk_write_pid();

	hk_log_mark("Hayk "VERSION" - server started");

	/* Init workers */
	hk_server_create_worker(server, server->config->defaults);

	{
		hk_list_item_t *item;
		
		item = server->config->virtuals->first;
	
		while ( item != NULL ) {
			hk_server_create_worker(server, (hk_config_entry_t *) item->data);
			item = item->next;
		}
	}
	
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
hk_server_start_single_proc()
{
	hk_server_t *server = hk_server();
	hk_worker_t *worker;

	if ( server == NULL ) {
		return;
	}
	
	hk_write_pid();

	worker = hk_server_create_worker(server, server->config->defaults);
	hk_worker_run(worker);
	
	hk_clear_pid();
}

void 
hk_server_stop()
{
	pid_t pid;

	pid = hk_read_pid();

	if ( pid == 0 ) {
		return;
	}

	kill(pid, SIGINT);
	while( hk_read_pid() > 0 ) {
		sleep(1);
	}
}

int 
hk_server_is_running()
{
	pid_t server_pid;

	server_pid = hk_read_pid();

	return server_pid > 0;
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
			hk_server_stop_real();
			break;
		case SIGUSR1:
		case SIGUSR2:
			hk_server_restart_real();
			break;
		default:
			break; 
	}
}

static void
hk_server_stop_real()
{
	hk_server_stop_workers();

	server_inst->restart = 0;
}

static void
hk_server_restart_real()
{
	hk_server_stop_workers();

	server_inst->restart = 1;
}

static void
hk_server_stop_workers()
{
	hk_worker_t *worker;
	hk_list_item_t *item;
	hk_server_t *server;

	server = hk_server();
	item = server->workers->first;
	
	while ( item != NULL ) {
		worker = (hk_worker_t*) item->data;
		hk_worker_stop(worker);
		item = item->next;
	}
}

pid_t
hk_read_pid()
{
	int pid_file;
	ssize_t bytes_read;
	pid_t pid;
	
	pid_file = open(HK_PIDFILE,O_RDONLY|O_CREAT,0640);
	
	if ( pid_file < 0 ) {
		printf("Unable to open pid file \"%s\"\n", HK_PIDFILE );
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
	
	pid_file = open(HK_PIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0640);
	
	if ( pid_file < 0 ) {
		printf("Unable to open pid file \"%s\"\n", HK_PIDFILE );
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
	pid_file = open(HK_PIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0640);
	if ( pid_file < 0 ) {
		return;
	}
	
	close(pid_file);
}

static hk_worker_t * 
hk_server_create_worker(hk_server_t *server, hk_config_entry_t *config)
{
	hk_worker_t *worker;

	worker = hk_worker_create(config);
	hk_list_push(server->workers, worker);

	return worker;
}