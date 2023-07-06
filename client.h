#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"

#define MAX_INBOX 300
#define MAX_TOPICS 300
#define MAX_CONNECTIONS	32

/* Structure for topic */
struct topic {
    char name[MAX_TOPIC];
    int sf;
};

/* Structure for client */
struct client {
    uint32_t flag_active;
    char id[MAX_ID];
    uint32_t fd;
    uint32_t topics_size;
    struct topic topics[MAX_TOPICS];
    uint32_t inbox_size;
    struct tcp_packet inbox[MAX_INBOX];
};

struct client clients[MAX_CONNECTIONS];
uint32_t clients_size = 0;

#endif /* _CLIENT_H_ */