#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"

void encode_header(PacketHeader *h, unsigned char *buffer)
{
    buffer[0] = h->version;
    buffer[1] = h->type;

    uint16_t flags = htons(h->flags);
    memcpy(buffer + 2, &flags, 2);

    uint32_t sender = htonl(h->sender_id);
    memcpy(buffer + 4, &sender, 4);

    uint32_t seq = htonl(h->seq_no);
    memcpy(buffer + 8, &seq, 4);

    uint32_t ack = htonl(h->ack_no);
    memcpy(buffer + 12, &ack, 4);

    uint16_t length = htons(h->length);
    memcpy(buffer + 16, &length, 2);

    uint16_t checksum = htons(h->checksum);
    memcpy(buffer + 18, &checksum, 2);
}

void decode_header(unsigned char *buffer, PacketHeader *h)
{
    h->version = buffer[0];
    h->type = buffer[1];

    uint16_t temp16;
    uint32_t temp32;

    memcpy(&temp16, buffer + 2, 2);
    h->flags = ntohs(temp16);

    memcpy(&temp32, buffer + 4, 4);
    h->sender_id = ntohl(temp32);

    memcpy(&temp32, buffer + 8, 4);
    h->seq_no = ntohl(temp32);

    memcpy(&temp32, buffer + 12, 4);
    h->ack_no = ntohl(temp32);

    memcpy(&temp16, buffer + 16, 2);
    h->length = ntohs(temp16);

    memcpy(&temp16, buffer + 18, 2);
    h->checksum = ntohs(temp16);
}

uint16_t calculate_checksum(unsigned char *data, int length)
{
    uint32_t sum = 0;

    for(int i = 0; i < length; i++)
        sum += data[i];

    return (uint16_t)(sum & 0xFFFF);
}

int send_packet(int sockfd, struct sockaddr_in *destination,
                uint8_t type, uint32_t sender_id,
                const char *payload) {

    unsigned char packet[MAX_PACKET_SIZE];
    int payload_length = payload ? strlen(payload) : 0;

    if (payload_length > MAX_PAYLOAD)
        return -1;

    PacketHeader header = {
        .version = VERSION,
        .type = type,
        .flags = 0,
        .sender_id = sender_id,
        .seq_no = 0,
        .ack_no = 0,
        .length = payload_length,
        .checksum = 0
    };

    encode_header(&header, packet);

    if (payload_length > 0)
        memcpy(packet + HEADER_SIZE, payload, payload_length);

    header.checksum = calculate_checksum(packet,
                                         HEADER_SIZE + payload_length);

    encode_header(&header, packet);

    return sendto(sockfd, packet, HEADER_SIZE + payload_length, 0,
                  (struct sockaddr *)destination,
                  sizeof(*destination));
}

int validate_packet(unsigned char *packet, int packet_length,
                    PacketHeader *header) {

    if (packet_length < HEADER_SIZE)
        return 0;

    decode_header(packet, header);

    if (header->version != VERSION)
        return 0;

    if (header->length != packet_length - HEADER_SIZE)
        return 0;

    uint16_t received_checksum = header->checksum;

    packet[18] = 0;
    packet[19] = 0;

    uint16_t calculated = calculate_checksum(packet, packet_length);

    if (received_checksum != calculated)
        return 0;

    return 1;
}