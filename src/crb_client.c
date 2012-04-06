#include <stdio.h>
#include <stdlib.h>

#include "crb_client.h"

crb_client_t *
crb_client_init()
{
    crb_client_t *client;

    client = malloc(sizeof(crb_client_t));
    if (client == NULL) {
        return NULL;
    }

    return client;
}
