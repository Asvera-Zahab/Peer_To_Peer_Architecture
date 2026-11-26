# Peer-to-Peer Group Communication over UDP: Basic Architecture

## (1) Project Overview

We are building a serverless, multi-party group communication system (chat + file sharing) on top of UDP with no central server. Every peer is both a client and a server. Since UDP does not guarantee delivery, ordering or duplicate protection, we have to build our own reliability layer at the application level.

Why P2P? A central server is a single point of failure and a bottleneck. In P2P, if one peer dies the rest keep working.

## (2) Core Requirements (from the assignment)

1. Peer Discovery & Membership: Peers find each other without a server and keep a list of active peers.
2. Group Chat: Messages reach every peer, in order, with no duplicates, even if packets are lost.
3. Reliable Data Transfer over UDP: ACKs, retransmission, sequence numbers.
4. Distributed File Storage: At least 2 shared files (>= 2MB), split into chunks, stored across different peers.
5. File Availability at Joining: A new peer learns which files/chunks exist and who holds them, then takes over a share.
6. Multi-Source Download: Download chunks from at least 2 peers in parallel, verify integrity.
7. Fault Tolerance: If a peer crashes mid-download, switch to another replica and finish.

## (3) High-Level Architecture

Every peer runs the same program with these layers:

- User Interface: CLI where you type commands (chat, list, download).
- Application Modules:
  - Chat Module: message ordering and duplicate removal.
  - File Module: splits files into chunks and manages downloads.
  - Membership Module: finds peers and tracks who is online (heartbeat).
- Reliability Layer (Mini-TCP): sequence numbers, ACKs, timeouts and retransmissions on top of UDP.
- Protocol Layer: encodes/decodes our custom packet header and body.
- UDP Socket: sends and receives raw packets.

## (4) Approach for Each Feature

### 4.1 Peer Discovery and Membership
- A new peer sends JOIN using LAN broadcast/multicast (or to a known bootstrap peer).
- Existing peers reply with their peer list.
- Every peer sends a HEARTBEAT every few seconds; a peer silent for about 3 beats is marked dead.
- A peer leaving politely sends LEAVE.

### 4.2 Group Chat (ordered, no duplicates)
- Each message carries sender_id and sequence_number.
- Each peer tracks the next expected sequence number per sender:
  - Lower: duplicate, ignore it (but still ACK).
  - Equal: deliver, then increment.
  - Higher: out of order, buffer it until the missing ones arrive.
- If the sender gets no ACK before timeout, it retransmits.

### 4.3 Reliable Data Transfer over UDP
- Sequence numbers for ordering and duplicate detection.
- ACKs to confirm receipt.
- Timeout + retransmission for lost packets.
- Sliding window so we don't send one packet at a time.

### 4.4 Distributed File Storage
- At least 2 shared files, each >= 2MB, split into fixed-size chunks (e.g. 16 KB).
- Each chunk has an index and a SHA-256 hash for integrity checking.
- Chunks are spread across peers, so no single peer holds a full file.
- Each chunk is stored on at least 2 peers (replication).
- Every peer keeps a chunk map: which peers have which chunk of which file.

### 4.5 File Availability at Joining
- A new peer asks for the file list and chunk map.
- It takes over some chunks (to balance load) by downloading them from existing holders.
- It then announces: "I now hold these chunks".

### 4.6 Multi-Source Download
- The downloader requests different chunks from different peers in parallel (threads).
- Each chunk's hash is checked on arrival; at the end the full file hash is checked.

### 4.7 Fault Tolerance
- If a peer stops responding during a download, switch to another peer that has the same chunk.
- Only missing chunks are re-requested.

## (5) Protocol Design (Draft)

The final version will be in docs/PROTOCOL.md.

Packet header (fixed size, big-endian):
- Version (1 byte)
- Message Type (1 byte)
- Flags (1 byte)
- Sender ID (4 bytes)
- Sequence Number (4 bytes)
- Payload Length (2 bytes)
- Checksum (4 bytes)

Message types: JOIN, PEER_LIST, LEAVE, HEARTBEAT, CHAT, ACK, FILE_LIST, CHUNK_MAP, CHUNK_ANNOUNCE, CHUNK_REQUEST, CHUNK_DATA

Example flows:
- Join: JOIN -> PEER_LIST -> FILE_LIST / CHUNK_MAP -> CHUNK_REQUEST / CHUNK_DATA -> CHUNK_ANNOUNCE
- Chat: CHAT (seq = n) -> ACK (n), retransmit if no ACK
- Download: CHUNK_REQUEST -> CHUNK_DATA -> ACK, repeated for all chunks

## (6) Data Each Peer Keeps

- peers: peer_id -> (ip, port, last_heartbeat)
- next_expected_seq: sender_id -> next sequence number
- out_of_order_buffer: messages that arrived early
- pending_acks: sent packets waiting for an ACK
- chunk_map: file_id -> chunk_index -> peers that have it
- local_chunks: chunks stored on this peer's disk

## (7) Threads

- Receiver, Heartbeat, Retransmit timer, Download workers, UI
- Shared data (peer list, chunk map) is protected with locks.

## (8) Tech and Folder Structure

Language: Python (socket, threading, struct, hashlib).

```
Peer_To_Peer_Architecture/
|-- README.md
|-- ARCHITECTURE.md
|-- docs/PROTOCOL.md
|-- src/
|   |-- main.py
|   |-- protocol.py
|   |-- reliability.py
|   |-- membership.py
|   |-- chat.py
|   +-- files.py
|-- shared_files/
|-- tests/
+-- results/
```

## (09) Testing Plan

- Run with at least 4 peers.
- Simulate packet loss, delay and duplicates.
- Kill a peer during a download.
- Measure transfer time vs chunk size and effect of packet loss. Plot graphs.
