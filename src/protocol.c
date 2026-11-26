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