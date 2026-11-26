
#include "membership.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

static Peer peers[MAX_PEERS];
static int peer_count = 0;

static int sockfd;
static uint32_t my_id;
static char my_name[MAX_NAME_LENGTH + 1];
static int my_port;

static pthread_mutex_t peer_lock = PTHREAD_MUTEX_INITIALIZER;

static void add_peer(uint32_t id, const char *name,
                     struct sockaddr_in address) {

    if (id == my_id || id == 0)
        return;

    pthread_mutex_lock(&peer_lock);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].id == id) {
            peers[i].last_seen = time(NULL);
            peers[i].active = 1;
            peers[i].address = address;
            pthread_mutex_unlock(&peer_lock);
            return;
        }
    }

    if (peer_count < MAX_PEERS) {
        peers[peer_count].id = id;
        strncpy(peers[peer_count].name, name, MAX_NAME_LENGTH);
        peers[peer_count].name[MAX_NAME_LENGTH] = '\0';
        peers[peer_count].address = address;
        peers[peer_count].last_seen = time(NULL);
        peers[peer_count].active = 1;

        printf("\n[JOINED] %s\n", name);
        peer_count++;
    }

    pthread_mutex_unlock(&peer_lock);
}

static void send_announcement(struct sockaddr_in *destination) {
    char payload[128];

    snprintf(payload, sizeof(payload), "%u,%s,127.0.0.1,%d",
             my_id, my_name, my_port);

    send_packet(sockfd, destination, PEER_ANNOUNCE, my_id, payload);
}

static void send_peer_list(struct sockaddr_in *destination) {
    char payload[MAX_PAYLOAD];
    char ip[INET_ADDRSTRLEN];

    payload[0] = '\0';

    // Include this peer in the list.
    snprintf(payload, sizeof(payload), "%u,%s,127.0.0.1,%d",
             my_id, my_name, my_port);

    pthread_mutex_lock(&peer_lock);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            inet_ntop(AF_INET, &peers[i].address.sin_addr,
                      ip, sizeof(ip));

            char entry[128];

            snprintf(entry, sizeof(entry), ";%u,%s,%s,%d",
                     peers[i].id,
                     peers[i].name,
                     ip,
                     ntohs(peers[i].address.sin_port));

            strncat(payload, entry,
                    sizeof(payload) - strlen(payload) - 1);
        }
    }

    pthread_mutex_unlock(&peer_lock);

    send_packet(sockfd, destination, PEER_LIST, my_id, payload);
}

void membership_init(int socket_fd, uint32_t id,
                     const char *name, int port) {
    sockfd = socket_fd;
    my_id = id;
    my_port = port;

    strncpy(my_name, name, MAX_NAME_LENGTH);
    my_name[MAX_NAME_LENGTH] = '\0';
}

void membership_handle_packet(unsigned char *packet, int length,
                              struct sockaddr_in *source) {

    PacketHeader header;

    if (!validate_packet(packet, length, &header))
        return;

    char payload[MAX_PAYLOAD + 1];

    memcpy(payload, packet + HEADER_SIZE, header.length);
    payload[header.length] = '\0';

    if (header.sender_id == my_id)
        return;

    if (header.type == JOIN) {

        add_peer(header.sender_id, payload, *source);

        // Send the joining peer the existing membership list.
        send_peer_list(source);

        // Tell existing peers about the newcomer.
        pthread_mutex_lock(&peer_lock);

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].active &&
                peers[i].id != header.sender_id) {
                send_announcement(&peers[i].address);
            }
        }

        pthread_mutex_unlock(&peer_lock);
    }

    else if (header.type == PEER_LIST) {

        char *saveptr;
        char *entry = strtok_r(payload, ";", &saveptr);

        while (entry != NULL) {
            unsigned int id;
            char name[32], ip[INET_ADDRSTRLEN];
            int port;

            if (sscanf(entry, "%u,%31[^,],%15[^,],%d",
                       &id, name, ip, &port) == 4) {

                struct sockaddr_in address = {0};

                address.sin_family = AF_INET;
                address.sin_port = htons(port);
                inet_pton(AF_INET, ip, &address.sin_addr);

                add_peer(id, name, address);
            }

            entry = strtok_r(NULL, ";", &saveptr);
        }

        // Introduce ourselves to the peers we just discovered.
        pthread_mutex_lock(&peer_lock);

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].active)
                send_announcement(&peers[i].address);
        }

        pthread_mutex_unlock(&peer_lock);
    }

    else if (header.type == PEER_ANNOUNCE) {

        unsigned int id;
        char name[32], ip[INET_ADDRSTRLEN];
        int port;

        if (sscanf(payload, "%u,%31[^,],%15[^,],%d",
                   &id, name, ip, &port) == 4) {

            struct sockaddr_in address = {0};

            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            inet_pton(AF_INET, ip, &address.sin_addr);

            add_peer(id, name, address);
        }
    }

    else if (header.type == HEARTBEAT) {

        pthread_mutex_lock(&peer_lock);

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].id == header.sender_id) {
                peers[i].last_seen = time(NULL);
                break;
            }
        }

        pthread_mutex_unlock(&peer_lock);
    }

    else if (header.type == LEAVE) {

        pthread_mutex_lock(&peer_lock);

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].id == header.sender_id) {
                peers[i].active = 0;
                printf("\n[LEFT] %s disconnected\n",
                       peers[i].name);
                break;
            }
        }

        pthread_mutex_unlock(&peer_lock);
    }

    else if (header.type == CHAT) {

        pthread_mutex_lock(&peer_lock);

        const char *sender_name = "Unknown";

        for (int i = 0; i < peer_count; i++) {
            if (peers[i].id == header.sender_id) {
                sender_name = peers[i].name;
                peers[i].last_seen = time(NULL);
                break;
            }
        }

        printf("\n%s: %s\n", sender_name, payload);
        pthread_mutex_unlock(&peer_lock);
    }
}

void membership_send_heartbeat(void) {
    pthread_mutex_lock(&peer_lock);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            send_packet(sockfd, &peers[i].address,
                        HEARTBEAT, my_id, "");
        }
    }

    pthread_mutex_unlock(&peer_lock);
}

void membership_check_timeouts(void) {
    pthread_mutex_lock(&peer_lock);

    time_t now = time(NULL);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active &&
            now - peers[i].last_seen > 6) {

            peers[i].active = 0;

            printf("\n[OFFLINE] %s disconnected\n",
                   peers[i].name);
        }
    }

    pthread_mutex_unlock(&peer_lock);
}

void membership_leave(void) {
    pthread_mutex_lock(&peer_lock);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            send_packet(sockfd, &peers[i].address,
                        LEAVE, my_id, "");
        }
    }

    pthread_mutex_unlock(&peer_lock);
}

void membership_show(void) {
    pthread_mutex_lock(&peer_lock);

    printf("\n------ ACTIVE PEERS ------\n");

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            char ip[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &peers[i].address.sin_addr,
                      ip, sizeof(ip));

            printf("%s | ID: %u | %s:%d\n",
                   peers[i].name,
                   peers[i].id,
                   ip,
                   ntohs(peers[i].address.sin_port));
        }
    }

    printf("--------------------------\n");

    pthread_mutex_unlock(&peer_lock);
}

void membership_chat(const char *message) {
    pthread_mutex_lock(&peer_lock);

    for (int i = 0; i < peer_count; i++) {
        if (peers[i].active) {
            send_packet(sockfd, &peers[i].address,
                        CHAT, my_id, message);
        }
    }

    pthread_mutex_unlock(&peer_lock);
}
