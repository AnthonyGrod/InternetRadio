#include <ctime>
#include <iostream>
#include <netdb.h>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>
#include <boost/program_options.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "err.h"
#include "common.h"

using namespace std;
namespace po = boost::program_options;

#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

void print_packet(uint8_t *p) {
    for (int i = 16; i < 16 + 512; i++) {
        printf("%c", p[i]);
    }
}

int set_parsed_arguments(po::variables_map &vm, int ac, char* av[]) {
    po::options_description desc("Allowed options");
        desc.add_options()
        ("help", "produce help message")
        // ("MULTICAST_ADDR,m", po::value<string>()->required(), "set multicast ip address")
        ("DEST_ADDR,a", po::value<string>()->required(), "set receiver ip address")
        ("DATA_PORT,P", po::value<int>()->default_value(20000 + (438477 % 10000)), "set receiver port")
        ("NAME,n", po::value<string>()->default_value("Nienazwany Nadajnik"), "set sender's name")
        ("PSIZE,p", po::value<int>()->default_value(512), "set package size (in bytes)")
        ("CTRL_PORT,C", po::value<int>()->default_value(30000 + (438477 % 10000)), "set control port")
        ("FSIZE,f", po::value<int>()->default_value(131072), "set file size (in bytes)")
        ("RTIME,R", po::value<int>()->default_value(250), "set retransmission time (in ms)")
        ;
    try {
    	po::store(po::parse_command_line(ac, av, desc), vm);
		po::notify(vm);
        int p = vm["PSIZE"].as<int>();
        if (p <= 0 || p > 65536 || vm["DATA_PORT"].as<int>() > 65536) {throw std::runtime_error("Invalid arguments");}
	} catch (const std::exception& e)  {std::cerr << "Bad arguments " << desc; exit(1);}
	if (!vm.count("DEST_ADDR")) {fatal("DEST_ADDR is required.");}

    if (vm.count("help")) {
        cout << desc << "\n";
        return 0;
    }

    return 0;  
}

void send_message(int socket_fd, const sockaddr_in *send_address, const uint8_t *message, size_t psize) {
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

void lookup_thread(uint32_t ctrl_port, std::string name) {
    // Set socket on control port with any address
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }
    int opt = 1;
    CHECK(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));
    opt = 1;
    CHECK(setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)));
    opt = 1;
    CHECK(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)));
    opt = 1;
    CHECK(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &opt, sizeof(opt)));
    bind_socket(socket_fd, 38477);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        uint8_t buffer[1 << 16];
        memset(buffer, 0, sizeof(buffer));
        ssize_t packet_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                      (struct sockaddr *) &client_address, &client_address_len);
        if (packet_len < 0) {
            PRINT_ERRNO();
        }
        printf("2. Received lookup message: %s \n", buffer);
        // Check if received message is "ZERO_SEVEN_COME_IN\n"
        if (packet_len == 19 && strncmp((char *) buffer, "ZERO_SEVEN_COME_IN\n", 19) == 0) {
            // Send "ROGER THAT\n" to client
            std::string message = "BOREWICZ_HERE 239.10.11.1 38477 " + name;
            message = message + "\n";
            ssize_t sent_len = sendto(socket_fd, message.c_str(), message.length(), 0,
                                      (struct sockaddr *) &client_address, client_address_len);
            printf("3. Sent answer for lookupmessage: %s\n\n", message.c_str());
            if (sent_len < 0) {
                PRINT_ERRNO();
            }
        }
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
    // First 8 bytes: uint64 session_id, Second 8 bytes: uint64 first_byte_num, Third 512 bytes: uint8_t[512] data
    uint8_t buffer[psize + 16];

    int dest_addr_len = dest_addr_str.length();
    char *dest_addr = new char[dest_addr_len + 1];
    strcpy((char*) dest_addr, dest_addr_str.c_str());

    // get receiver address
    sockaddr_in send_address = get_send_address(dest_addr, (uint16_t) data_port);

    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    int opt = 4; // ttlvalue
    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &opt, sizeof(opt));
    int enable = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    std::thread lookup_thrd = std::thread(lookup_thread, 30000 + (438477 % 10000), name);

    size_t session_id = time(NULL);
    size_t first_byte_num = 0;
    while (1) {
        // read message from stdin and store it in uint8_t[512] data
        // parse session_id and first_byte_num
        // std::cout << "Reading from stdin" << std::endl;
        size_t length = fread(buffer + 16, 1, psize, stdin);
        if (length % psize != 0 && feof(stdin)) {
            break;
        }
        size_t htonll_session_id = ntohll(session_id);
        size_t htonll_first_byte_num = ntohll(first_byte_num);
        memcpy(buffer, &htonll_session_id, 8);
        memcpy(buffer + 8, &htonll_first_byte_num, 8);
        // send message to receiver
        send_message(socket_fd, &send_address, buffer, psize);
        first_byte_num += psize;
    }
    
    // close socket
    close(socket_fd);
    lookup_thrd.join();
    return 0;
}