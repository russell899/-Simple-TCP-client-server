# TCP Echo Client-Server (C, Linux)

A simple TCP client-server pair written in C for Linux.

- **Server** listens on TCP port **8080** and supports **multiple concurrent clients** using **pthread** (one thread per connection).
- **Client** connects to a given host/port (`-h`, `-p`), sends a message every **1 second**, and prints the echoed reply.

## Project Structure

- `server.c` — Multi-client TCP echo server (pthread-based).
- `client.c` — TCP client with command-line options, periodic sending, and echo receiving.
- `Makefile` — Build rules for server/client.
- `echo_8080_lo.pacp` — Wireshark capture on loopback (evidence of TCP handshake/data/teardown).
- `Report` — Screenshots of running results + Wireshark capture views.

## Features

### Server (`server.c`)
- Uses `getaddrinfo(NULL, "8080", ...)` with `AI_PASSIVE` to bind locally.  
- Listens with `BACKLOG = 10` pending connections.
- Accepts clients in a loop; each accepted `client_fd` is handled by a new pthread:
  - The thread is detached (`pthread_detach(pthread_self())`) so it cleans up automatically.
  - Echo loop:
    - `recv()` into a buffer
    - `send()` back **all bytes** using a loop (because `send()` may not send everything at once)
    - Closes when client disconnects (`recv() == 0`)

### Client (`client.c`)
- Command line options:
  - `-h <server_ip/host>`
  - `-p <port>`
- Resolves address via `getaddrinfo(host, port, ...)`, then tries to `connect()` to the first working result.
- In a loop (every 1 second):
  - Sends `count=<n>\n`
  - Receives echoed data and prints both sent/received messages

Run
1) Start the server

Open Terminal A:

./server


You should see:

server: waiting for connections...

server: got connection from <client_ip>

2) Start one client

Open Terminal B:

./client -h 127.0.0.1 -p 8080


Example output (will vary):

client: attempting to connect to ...

client: connecting to ...

client: sent: count=1

client: recv: count=1

...

3) Multiple clients

Open more terminals and run the same client command multiple times.
The server will handle them concurrently (one thread per client connection).

Verification with Wireshark

Capture on loopback and filter by port:

Display filter: tcp.port == 8080

You should observe:

TCP 3-way handshake (SYN → SYN/ACK → ACK)

Payload packets (PSH/ACK carrying application data)

Connection teardown (FIN/ACK sequence)

A sample capture file is included: echo_8080_lo.pacp.

Notes / Known Issues

Server is currently configured for IPv4 only (hints.ai_family = AF_INET).

client.c closes the socket twice at the end (close(sockfd); appears twice).
It usually won’t crash, but it’s best to remove the duplicate close to avoid undefined behavior.

```bash
make
