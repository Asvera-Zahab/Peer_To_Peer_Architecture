#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "protocol.h"

#define PORT 5000
#define BUFFER_SIZE 1472

int sockfd;
uint32_t peer_id = 1;
uint32_t sequence_number = 0;

struct sockaddr_in broadcast_addr;

void send_packet(int type, char *message)
{
    unsigned char buffer[BUFFER_SIZE];

    PacketHeader header;

    header.version = VERSION;
    header.type = type;
    header.flags = 0;
    header.sender_id = peer_id;
    header.seq_no = ++sequence_number;
    header.ack_no = 0;

    header.length = strlen(message);
    header.checksum = 0;

    encode_header(&header, buffer);

    memcpy(buffer + HEADER_SIZE, message, header.length);

    int total_size = HEADER_SIZE + header.length;

    header.checksum = calculate_checksum(buffer, total_size);

    encode_header(&header, buffer);

    sendto(sockfd, buffer, total_size, 0,
           (struct sockaddr *)&broadcast_addr,
           sizeof(broadcast_addr));
}

void *receive_messages(void *arg)
{
    unsigned char buffer[BUFFER_SIZE];

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    while(1)
    {
        int bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                            (struct sockaddr *)&sender,
                            &sender_len);

        if(bytes < HEADER_SIZE)
            continue;

        PacketHeader header;

        decode_header(buffer, &header);

        if(header.version != VERSION)
            continue;

        if(header.length != bytes - HEADER_SIZE)
            continue;

        char message[MAX_PAYLOAD + 1];

        memcpy(message, buffer + HEADER_SIZE, header.length);
        message[header.length] = '\0';

        if(header.sender_id == peer_id)
            continue;

        if(header.type == CHAT)
        {
            printf("\nPeer %u: %s\n", header.sender_id, message);
            printf("> ");
            fflush(stdout);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s <peer_id>\n", argv[0]);
        return 1;
    }

    peer_id = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    int reuse = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));

    struct sockaddr_in local_addr;

    memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd, (struct sockaddr *)&local_addr,
            sizeof(local_addr)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    int broadcast_enable = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
               &broadcast_enable, sizeof(broadcast_enable));

    pthread_t receiver;

    pthread_create(&receiver, NULL, receive_messages, NULL);

    printf("Peer %u started!\n", peer_id);
    printf("Enter messages below:\n");

    char message[1024];

    while(1)
    {
        printf("> ");
        fgets(message, sizeof(message), stdin);

        message[strcspn(message, "\n")] = '\0';

        if(strlen(message) > 0)
            send_packet(CHAT, message);
    }

    close(sockfd);

    return 0;
}