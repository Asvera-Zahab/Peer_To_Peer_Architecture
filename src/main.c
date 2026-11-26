
#include "protocol.h"
#include "membership.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

volatile sig_atomic_t running = 1;

int sockfd;
uint32_t my_id;

void handle_signal(int signal_number) {
    (void)signal_number;
    running = 0;
}

void *receiver_thread(void *arg) {
    unsigned char buffer[MAX_PACKET_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_length;

    while (running) {
        sender_length = sizeof(sender);

        int length = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                              (struct sockaddr *)&sender,
                              &sender_length);

        if (length > 0) {
            membership_handle_packet(buffer, length, &sender);
        }
    }

    return NULL;
}

void *heartbeat_thread(void *arg) {
    while (running) {
        sleep(2);

        if (running) {
            membership_send_heartbeat();
            membership_check_timeouts();
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc != 3 && argc != 5) {
        printf("Usage: ./peer <name> <port> [bootstrap_ip bootstrap_port]\n");
        return 1;
    }

    char *name = argv[1];
    int port = atoi(argv[2]);

    if (port < 1 || port > 65535 || strlen(name) > MAX_NAME_LENGTH) {
        printf("Invalid name or port.\n");
        return 1;
    }

    my_id = ((uint32_t)time(NULL) ^ (uint32_t)getpid());
    if (my_id == 0)
        my_id = 1;

    signal(SIGINT, handle_signal);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct timeval timeout = {1, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               &timeout, sizeof(timeout));

    struct sockaddr_in local_address = {0};

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = INADDR_ANY;
    local_address.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&local_address,
             sizeof(local_address)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    membership_init(sockfd, my_id, name, port);

    printf("\nPeer started successfully!\n");
    printf("Name: %s\n", name);
    printf("ID: %u\n", my_id);
    printf("Port: %d\n", port);

    pthread_t receiver, heartbeat;

    pthread_create(&receiver, NULL, receiver_thread, NULL);
    pthread_create(&heartbeat, NULL, heartbeat_thread, NULL);

    if (argc == 5) {
        struct sockaddr_in bootstrap = {0};

        bootstrap.sin_family = AF_INET;
        bootstrap.sin_port = htons(atoi(argv[4]));

        if (inet_pton(AF_INET, argv[3],
                      &bootstrap.sin_addr) != 1) {
            printf("Invalid bootstrap IP address.\n");
            running = 0;
        } else {
            send_packet(sockfd, &bootstrap, JOIN, my_id, name);
        }
    }

    printf("\nCommands:\n");
    printf("/peers - Show active peers\n");
    printf("/quit  - Leave the group\n");
    printf("Type a message to chat.\n\n");

    char input[MAX_PAYLOAD + 1];

    while (running) {
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "/peers") == 0) {
            membership_show();
        }

        else if (strcmp(input, "/quit") == 0) {
            membership_leave();
            break;
        }

        else if (strlen(input) > 0) {
            membership_chat(input);
        }
    }

    running = 0;

    close(sockfd);

    pthread_join(receiver, NULL);
    pthread_join(heartbeat, NULL);

    printf("\nPeer shutting down.\n");

    return 0;
}
