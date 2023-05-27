#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

void send_broadcast_message(int port, const char *message) {
    int sender_socket;
    struct sockaddr_in discover_addr;
    char buffer[BUFFER_SIZE];

    // Create a socket
    sender_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Enable SO_BROADCAST option
    int broadcast_enable = 1;
    if (setsockopt(sender_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Failed to set SO_BROADCAST option");
        exit(EXIT_FAILURE);
    }

    // Set up the destination address
    memset(&discover_addr, 0, sizeof(discover_addr));
    discover_addr.sin_family = AF_INET;
    discover_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    discover_addr.sin_port = htons(port);

    // Send the broadcast message
    strncpy(buffer, message, BUFFER_SIZE - 1);
    ssize_t data_len = sendto(sender_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&discover_addr, sizeof(discover_addr));
    if (data_len < 0) {
        perror("Failed to send data");
        exit(EXIT_FAILURE);
    }

    printf("Broadcast message sent: %s\n", message);

    // Close the socket
    close(sender_socket);
}

// Usage
int main() {
    int port = 12345;
    const char *message = "Hello, world!";
    send_broadcast_message(port, message);
    return 0;
}
