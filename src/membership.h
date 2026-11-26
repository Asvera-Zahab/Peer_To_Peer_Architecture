
#ifndef MEMBERSHIP_H
#define MEMBERSHIP_H

#include "protocol.h"

#include <time.h>
#include <pthread.h>
#include <netinet/in.h>

#define MAX_PEERS 32
#define MAX_NAME_LENGTH 31

typedef struct {
    uint32_t id;
    char name[MAX_NAME_LENGTH + 1];
    struct sockaddr_in address;
    time_t last_seen;
    int active;
} Peer;

void membership_init(int socket_fd, uint32_t id,
                    const char *name, int port);

void membership_handle_packet(unsigned char *packet, int length,
                              struct sockaddr_in *source);

void membership_send_heartbeat(void);
void membership_check_timeouts(void);
void membership_leave(void);
void membership_show(void);
void membership_chat(const char *message);

#endif
