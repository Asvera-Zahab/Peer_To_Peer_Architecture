# Protocol Specification (Version 1)

Every packet = 20 byte header + payload. All numbers are big-endian (network byte order).

Max packet size is 1472 bytes (header + payload). The Ethernet limit is 1500, and the IP (20) and UDP (8) headers use 28 of that.

## (1) Header (20 bytes)

- Version (1 byte): always 1
- Type (1 byte): message type
- Flags (2 bytes): bit 0 = ACK required, bit 1 = retransmission, bit 2 = reply
- Sender ID (4 bytes): ID of the sending peer
- Seq No. (4 bytes): packet number, for ordering and duplicate detection
- Ack No. (4 bytes): seq number being acknowledged
- Length (2 bytes): payload size
- Checksum (2 bytes): CRC32 of header + payload, lowest 16 bits

Packets with a wrong checksum or version are dropped.

## (2) Message Types

- 1 JOIN: listen_port, name
- 2 PEER_LIST: list of peers (id, ip, port)
- 3 LEAVE: empty
- 4 HEARTBEAT: empty
- 5 CHAT: text (ACK required)
- 6 ACK: empty (Ack No. is in the header)
- 7 FILE_LIST: empty request, or reply with files (file_id, size, num_chunks, sha256, name)
- 8 CHUNK_MAP: file_id ("which chunks do you hold?")
- 9 CHUNK_ANNOUNCE: file_id, chunk ranges (start, end)
- 10 CHUNK_REQUEST: file_id, chunk_index (ACK required)
- 11 CHUNK_DATA: file_id, chunk_index, chunk_sha256, data (ACK required)

## (3) Rules

- Chat order (per sender): lower seq than expected is a duplicate (ignore, still ACK), equal is delivered, higher is buffered until the gap is filled.
- Reliability: if no ACK in 0.5 s, resend (max 5 tries), then treat the peer as unreachable.

## (4) Main Flows

- Join: JOIN -> PEER_LIST -> FILE_LIST -> CHUNK_MAP -> CHUNK_ANNOUNCE -> CHUNK_REQUEST / CHUNK_DATA
- Chat: CHAT -> ACK
- Download: CHUNK_REQUEST -> CHUNK_DATA -> ACK, from several peers in parallel. If a peer times out, ask another holder.
- Heartbeat: every 2 s. Silent for 6 s means dead.
- Leave: LEAVE sent to all peers.

## (5) Defaults

- Chunk size 1024 bytes
- Window size 8
- Each chunk stored on at least 2 peers
