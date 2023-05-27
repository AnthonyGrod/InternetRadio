#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

void create_broadcast_receiver(int port) {
    int receiver_socket;
    struct sockaddr_in receiver_address;
    char buffer[BUFFER_SIZE];

    // Create a socket
    receiver_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Enable SO_REUSEADDR flag
    int reuseaddr = 1;
    if (setsockopt(receiver_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        perror("Failed to set SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a specific port
    memset(&receiver_address, 0, sizeof(receiver_address));
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_address.sin_port = htons(port);

    if (bind(receiver_socket, (struct sockaddr *)&receiver_address, sizeof(receiver_address)) < 0) {
        perror("Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // Receive and process broadcast messages
    while (1) {
        struct sockaddr_in sender_address;
        socklen_t sender_address_len = sizeof(sender_address);
        ssize_t data_len = recvfrom(receiver_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender_address, &sender_address_len);
        if (data_len < 0) {
            perror("Failed to receive data");
            exit(EXIT_FAILURE);
        }

        buffer[data_len] = '\0';
        printf("Received broadcast message: %s from %s\n", buffer, inet_ntoa(sender_address.sin_addr));
    }

    // Close the socket
    close(receiver_socket);
}

// Usage
int main() {
    int port = 12345;
    create_broadcast_receiver(port);
    return 0;
}
