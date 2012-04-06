#ifndef __CRB_CLIENT_H__
#define __CRB_CLIENT_H__ 1

typedef struct crb_client_s crb_client_t;

struct crb_client_s {
	int sock_fd;
};

crb_client_t *crb_client_init();

#endif /* __CRB_CLIENT_H__ */
