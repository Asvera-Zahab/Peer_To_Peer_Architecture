#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define VERSION 1
#define MAX_PACKET_SIZE 1472
#define HEADER_SIZE 20
#define MAX_PAYLOAD (MAX_PACKET_SIZE - HEADER_SIZE)

#define JOIN 1
#define PEER_LIST 2
#define LEAVE 3
#define HEARTBEAT 4
#define CHAT 5
#define ACK 6
#define FILE_LIST 7
#define CHUNK_MAP 8
#define CHUNK_ANNOUNCE 9
#define CHUNK_REQUEST 10
#define CHUNK_DATA 11

typedef struct
{
    uint8_t version;
    uint8_t type;
    uint16_t flags;

    uint32_t sender_id;
    uint32_t seq_no;
    uint32_t ack_no;

    uint16_t length;
    uint16_t checksum;

} PacketHeader;

void encode_header(PacketHeader *header, unsigned char *buffer);

void decode_header(unsigned char *buffer, PacketHeader *header);

uint16_t calculate_checksum(unsigned char *data, int length);

#endif