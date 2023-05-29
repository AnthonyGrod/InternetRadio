#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <signal.h>
#include <chrono>
#include <iostream>

#include "err.h"

#define NO_FLAGS 0

inline static uint16_t read_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%ul is not a valid port number", port);
    }

    return (uint16_t) port;
}

inline static uint16_t get_port(struct sockaddr_in *address) {
    return ntohs(address->sin_port);
}

inline static char *get_ip(struct sockaddr_in *address) {
    return inet_ntoa(address->sin_addr);
}

inline static uint16_t get_port_from_socket(int socket_fd) {
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    CHECK_ERRNO(getsockname(socket_fd, (struct sockaddr *) &address, &address_length));
    return get_port(&address);
}

inline static char *get_ip_from_socket(int socket_fd) {
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    CHECK_ERRNO(getsockname(socket_fd, (struct sockaddr *) &address, &address_length));
    return get_ip(&address);
}

inline static struct sockaddr_in get_address(char *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    struct sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr =
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; // IP address
    address.sin_port = htons(port);

    freeaddrinfo(address_result);

    return address;
}

inline static int open_socket() {
    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    return socket_fd;
}

inline static void bind_socket(int socket_fd, uint16_t port) {
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address)));
}

int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd >= 0);
    // after socket() call; we should close(sock) on any execution path;

    sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (sockaddr *) &server_address,
                        (socklen_t) sizeof(server_address)));

    return socket_fd;
}

inline static uint16_t bind_socket_to_any_port(int socket_fd) {
    bind_socket(socket_fd, 0);
    return get_port_from_socket(socket_fd);
}

inline static void start_listening(int socket_fd, size_t queue_length) {
    CHECK_ERRNO(listen(socket_fd, queue_length));
}

inline static int accept_connection(int socket_fd, struct sockaddr_in *client_address) {
    socklen_t client_address_length = (socklen_t) sizeof(*client_address);

    int client_fd = accept(socket_fd, (struct sockaddr *) client_address, &client_address_length);
    if (client_fd < 0) {
        PRINT_ERRNO();
    }

    return client_fd;
}

inline static void connect_socket(int socket_fd, const struct sockaddr_in *address) {
    CHECK_ERRNO(connect(socket_fd, (struct sockaddr *) address, sizeof(*address)));
}

inline static size_t receive_message(int socket_fd, void *buffer, size_t max_length, int flags) {
    errno = 0;
    ssize_t received_length = recv(socket_fd, buffer, max_length, flags);
    if (received_length < 0) {
        PRINT_ERRNO();
    }
    return (size_t) received_length;
}

inline static void install_signal_handler(int signal, void (*handler)(int)) {
    struct sigaction action;
    sigset_t block_mask;

    sigemptyset(&block_mask);
    action.sa_handler = handler;
    action.sa_mask = block_mask;
    action.sa_flags = 0;

    CHECK_ERRNO(sigaction(signal, &action, NULL));
}

inline static void set_port_reuse(int socket_fd) {
    int option_value = 1;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &option_value, sizeof(option_value)));
}

inline static int open_udp_socket() {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    return socket_fd;
}

inline static void send_message(int socket_fd, const void *message, size_t length, int flags) {
    errno = 0;
    ssize_t sent_length = send(socket_fd, message, length, flags);
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
    ENSURE(sent_length == (ssize_t) length);
}

void send_broadcast_message(int socket_fd, const sockaddr_in* send_address, const uint8_t* message, size_t message_length) {
    int broadcast_enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        PRINT_ERRNO();
    }

    // Broadcast address
    sockaddr_in broadcast_address;
    memset(&broadcast_address, 0, sizeof(broadcast_address));
    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_address.sin_port = send_address->sin_port;

    ssize_t sent_length = sendto(socket_fd, message, message_length, 0, (struct sockaddr*)&broadcast_address, sizeof(broadcast_address));
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
}

void receive_broadcast_message(int socket_fd, uint8_t* buffer, size_t buffer_length, uint32_t RECEIVE_PORT) {
    // Enable receiving broadcast messages
    int broadcast_enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        PRINT_ERRNO();
    }

    // Set up the local address to bind and receive broadcast messages
    sockaddr_in local_address;
    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(RECEIVE_PORT); // Replace RECEIVE_PORT with the appropriate port number

    // Bind the socket to the local address
    if (bind(socket_fd, (struct sockaddr*)&local_address, sizeof(local_address)) < 0) {
        PRINT_ERRNO();
    }

    // Receive broadcast message
    sockaddr_in sender_address;
    socklen_t sender_address_length = sizeof(sender_address);
    ssize_t received_length = recvfrom(socket_fd, buffer, buffer_length, 0, (struct sockaddr*)&sender_address, &sender_address_length);
    if (received_length < 0) {
        PRINT_ERRNO();
    }
}

void receive_multicast_message(const char* multicast_address, uint16_t local_port, uint32_t BSIZE) {
    /* Open the UDP socket */
    int socket_fd = open_udp_socket();

    /* Join the multicast group */
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_address, &ip_mreq.imr_multiaddr) == 0) {
        fatal("inet_aton - invalid multicast address\n");
    }

    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq)));

    /* Bind the socket to the local address and port */
    bind_socket(socket_fd, local_port);

    /* Read the received messages */
    char buffer[BSIZE];
    size_t received_length = receive_message(socket_fd, buffer, sizeof(buffer), NO_FLAGS);

    // /* Leave the multicast group */
    // CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq)));

    // /* Close the socket */
    // CHECK_ERRNO(close(socket_fd));
}

void send_multicast_message(const char* remote_address, uint16_t remote_port, uint32_t BSIZE) {
    /* Open the UDP socket */
    int socket_fd = open_udp_socket();
    uint8_t TTL_VALUE = 4;

    /* Enable broadcast */
    int optval = 1;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof(optval)));

    /* Set TTL for multicast datagrams */
    optval = TTL_VALUE;
    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof(optval)));

    /* Set the address and port of the recipient */
    struct sockaddr_in remote_sockaddr;
    remote_sockaddr.sin_family = AF_INET;
    remote_sockaddr.sin_port = htons(remote_port);
    if (inet_aton(remote_address, &remote_sockaddr.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    /* Bind the socket to the recipient's address and port to use write instead of sendto */
    connect_socket(socket_fd, &remote_sockaddr);

    /* Joyful broadcasting of time */
    char buffer[BSIZE];
    time_t time_buffer;
    // for (int i = 0; i < REPEAT_COUNT; ++i) {
    //     time(&time_buffer);
    //     strncpy(buffer, ctime(&time_buffer), BSIZE);
    //     size_t length = strnlen(buffer, BSIZE);
    //     send_message(socket_fd, buffer, length, NO_FLAGS);
    //     sleep(SLEEP_TIME);
    // }
}

int create_broadcast_reuse_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {fatal("setsockopt");}
    opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {fatal("setsockopt");}
    return sock;
}

int recvfromWithTimeout(int sockfd, void* buffer, size_t length, int flags, struct sockaddr* addr, socklen_t* addrlen, int timeout) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int result = select(sockfd + 1, &fds, nullptr, nullptr, &tv);
    if (result == -1) {
        std::cerr << "Error in select" << std::endl;
        return -1;
    } else if (result == 0) {
        // Timeout reached
        std::cerr << "Timeout reached" << std::endl;
        return 0;
    }

    return recvfrom(sockfd, buffer, length, flags, addr, addrlen);
}
