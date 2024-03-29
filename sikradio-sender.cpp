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
#include "RadioStation.hpp"

using namespace std;
namespace po = boost::program_options;

#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#define htonll(x) (((uint64_t)htonl(x)) << 32 | htonl(x >> 32))

void print_packet(uint8_t *p) {
    for (int i = 16; i < 16 + 512; i++) {
        printf("%c", p[i]);
    }
}

int set_parsed_arguments(po::variables_map &vm, int ac, char* av[]) {
    po::options_description desc("Allowed options");
        desc.add_options()
        ("help", "produce help message")
        ("MCAST_ADDR,a", po::value<string>()->required(), "set multicast address")
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
        int c = vm["CTRL_PORT"].as<int>();
        int f = vm["FSIZE"].as<int>();
        int r = vm["RTIME"].as<int>();
        int port = vm["DATA_PORT"].as<int>();
        if (p <= 0 || f <= 0 || r <= 0 || c <= 0 || port <= 0 || p > 65535 || 
            port > 65535 || c > 65535) {
            throw std::runtime_error("Invalid arguments");
        }
        if (!RadioStation::isNameValid(vm["NAME"].as<string>()) || !RadioStation::isValidMulticastIPv4(vm["MCAST_ADDR"].as<string>())) {
			throw std::runtime_error("Invalid name");
		}
	} catch (...)  {std::cerr << "Bad arguments " << desc; exit(1);}
	if (!vm.count("MCAST_ADDR")) {fatal("DEST_ADDR is required.");}

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

void lookup_thread(uint32_t ctrl_port, std::string name, uint32_t data_port, std::string multicast_address) {
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
    bind_socket(socket_fd, ctrl_port);

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
        // Check if received message is "ZERO_SEVEN_COME_IN\n"
        if (packet_len == 19 && strncmp((char *) buffer, "ZERO_SEVEN_COME_IN\n", 19) == 0) {
            std::stringstream messagess;
            messagess << "BOREWICZ_HERE " << multicast_address.c_str() << " " << data_port << " " << name << "\n";
            std::string message = messagess.str();
            ssize_t sent_len = sendto(socket_fd, message.c_str(), message.length(), 0,
                                      (struct sockaddr *) &client_address, client_address_len);
            
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
    string dest_addr_str = vm["MCAST_ADDR"].as<string>();
    int data_port = vm["DATA_PORT"].as<int>();
    string name = vm["NAME"].as<string>();
    int psize = vm["PSIZE"].as<int>();
    int ctrl_port = vm["CTRL_PORT"].as<int>();

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

    std::thread lookup_thrd = std::thread(lookup_thread, ctrl_port, name, data_port, dest_addr_str); // changed

    size_t session_id = time(NULL);
    size_t first_byte_num = 0;
    while (1) {
        size_t length = fread(buffer + 16, 1, psize, stdin);
        if (length % psize != 0 && feof(stdin)) {
            break;
        }
        size_t htonll_session_id = htonll(session_id);
        size_t htonll_first_byte_num = htonll(first_byte_num);
        memcpy(buffer, &htonll_session_id, 8);
        memcpy(buffer + 8, &htonll_first_byte_num, 8);
        // send message to receiver
        send_message(socket_fd, &send_address, buffer, psize + 16);
        first_byte_num += psize;
    }
    
    lookup_thrd.join();
    close(socket_fd);
    return 0;
}