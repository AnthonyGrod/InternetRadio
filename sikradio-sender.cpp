#include <ctime>
#include <iostream>
#include <netdb.h>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <boost/program_options.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "err.h"

using namespace std;
namespace po = boost::program_options;

void print_packet(char *p) {
    for (int i = 16; i < 16 + 512; i++) {
        printf("%c", p[i]);
    }
}

int set_parsed_arguments(po::variables_map &vm, int ac, char* av[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("DEST_ADDR,a", po::value<string>()->required(), "set receiver ip address")
        ("DATA_PORT,P", po::value<int>()->default_value(20000+(438477%10000)), "set receiver port")
        ("NAME,n", po::value<string>()->default_value("Nienazwany odbiornik"), "set sender's name")
        ("PSIZE,p", po::value<int>()->default_value(512), "set package size (in bytes)")
    ;


    po::store(po::parse_command_line(ac, av, desc), vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }

    po::notify(vm);
    return 0;  
}

uint16_t read_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%u is not a valid port number", port);
    }

    return (uint16_t) port;
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

sockaddr_in get_send_address(char *host, uint16_t port) {
    addrinfo hints;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo *address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =
            ((sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; // IP address
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

void send_message(int socket_fd, const sockaddr_in *send_address, const char *message, size_t psize) {
    size_t message_length = psize;
    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, message, message_length, send_flags,
                                 (sockaddr *) send_address, address_length);
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
}

int main(int ac, char* av[]) {
    // Declare the supported options.
    po::variables_map vm;
    if (set_parsed_arguments(vm, ac, av) == 1) {
        return 1;
    }
    string dest_addr_str = vm["DEST_ADDR"].as<string>();
    int data_port = vm["DATA_PORT"].as<int>();
    string name = vm["NAME"].as<string>();
    int psize = vm["PSIZE"].as<int>();

    // allocate memmnory for message.
    // First 8 bytes: uint64 session_id, Second 8 bytes: uint64 first_byte_num, Third 512 bytes: char[512] data
    char buffer[psize + 16];

    // create socket binded at data_port
    int socket_fd = bind_socket(data_port);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    int dest_addr_len = dest_addr_str.length();
    char *dest_addr = new char[dest_addr_len + 1];
    strcpy(dest_addr, dest_addr_str.c_str());

    // get receiver address
    sockaddr_in send_address = get_send_address(dest_addr, (uint16_t) data_port);

    size_t session_id = time(NULL);
    size_t first_byte_num = 0;
    while (1) {
        // read message from stdin and store it in char[512] data
        // parse session_id and first_byte_num
        size_t length = fread(buffer + 16, 1, psize, stdin);
        if (length % psize != 0 && feof(stdin)) {
            printf("chuj\n");
            break;
        }
        memcpy(buffer, &session_id, 8);
        memcpy(buffer + 8, &first_byte_num, 8);
        // print_packet(buffer);
        // send message to receiver
        send_message(socket_fd, &send_address, buffer, psize);
        first_byte_num += psize;
    }
    
    // close socket
    close(socket_fd);
}