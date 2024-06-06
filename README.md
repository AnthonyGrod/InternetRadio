# SIK Radio Project

## Overview

This project implements a multicast internet radio transmitter and receiver. The transmitter sends a stream of audio data to multiple receivers over UDP. Receivers discover available transmitters, request missing packets for retransmission, and output the audio data.

## Part A: Transmitter

The transmitter reads an audio stream from standard input, packages it into UDP datagrams, and sends them to a multicast address. It maintains a FIFO queue of the last `FSIZE` bytes read, allowing for retransmission of requested packets.

### Features
- Reads audio data from standard input.
- Sends UDP packets to a specified multicast address and port.
- Maintains a FIFO queue for packet retransmission.
- Listens for control messages on a specified port.

### Usage
```bash
./sikradio-sender -a MCAST_ADDR -n "Radio Name" -P DATA_PORT -C CTRL_PORT -p PSIZE -f FSIZE -R RTIME
```

## Part B: Receiver

The receiver listens for multicast UDP packets, requests retransmissions for missing packets, and outputs the audio data to standard output. It also provides a simple text-based UI over TCP for switching between stations.

### Features
- Discovers active transmitters by sending `LOOKUP` requests.
- Receives and buffers audio packets.
- Requests retransmission of missing packets.
- Provides a text-based UI for station selection.

### Usage
```bash
./sikradio-receiver -d DISCOVER_ADDR -C CTRL_PORT -U UI_PORT -b BSIZE -R RTIME -n "Radio Name"
```

## Protocols

### Audio Packet Structure
```c
struct __attribute__((packed)) audio_packet {
    uint64_t session_id;
    uint64_t first_byte_num;
    char     audio_data[];
};
```

### Building project
In order to build the project, simply run `make` command.