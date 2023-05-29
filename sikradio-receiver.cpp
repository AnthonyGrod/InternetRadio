#include <ctime>
#include <iostream>
#include <netdb.h>
#include <string>
#include <regex>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <boost/program_options.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "err.h"
#include "common.h"
#include "CycleBuff.hpp"
#include "UIHandler.hpp"

using namespace std;
namespace po = boost::program_options;

#define MAX_PACKET_SIZE 65535

#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

uint8_t big_buff[MAX_PACKET_SIZE];
std::atomic<bool> is_running(true);
size_t psize_read = 0;
size_t last_packet_num_received = 0;
size_t packet_number = 0;

RadioStation selected_radio_station = RadioStation();

CycleBuff *cycle_buff_ptr = new CycleBuff();

int set_parsed_arguments(po::variables_map &vm, int ac, char* av[]) {
    po::options_description desc("Allowed options");
		desc.add_options()
        ("help", "produce help message")
        // ("DEST_ADDR,a", po::value<string>()->required(), "set sender's ip address")
		("CTRL_PORT,C", po::value<int>()->default_value(30000 + (438477 % 10000)), "set control port")
		("UI_PORT,U", po::value<int>()->default_value(10000 + (438477 % 10000)), "set UI port")
		("DISCOVER_ADDR,d", po::value<string>()->default_value("255.255.255.255"))
        ("DATA_PORT,P", po::value<int>()->default_value(20000 + (438477 % 10000)), "set sender's port")
        ("BSIZE,b", po::value<int>()->default_value(65536), "set buffor size (in bytes)")
		("NAME,n", po::value<string>()->default_value("Nienazwany Nadajnik"), "set sender's name")
		("RTIME,R", po::value<int>()->default_value(250), "set retransmission time (in ms)")
    	;
	try {
    	po::store(po::parse_command_line(ac, av, desc), vm);
		po::notify(vm);
		int b = vm["BSIZE"].as<int>();
        if (b <= 0 || vm["DATA_PORT"].as<int>() > 65536) {throw std::runtime_error("Invalid arguments");}
	} catch (const std::exception& e)  {std::cerr << "Bad arguments " << desc; exit(1);}

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }

    return 0;  
}

size_t get_first_byte_num() {
	uint64_t first_byte_num = ((uint64_t *) big_buff)[1];
	return htonll(first_byte_num);
}

size_t get_session_id() {
	uint64_t session_id = ((uint64_t *) big_buff)[0];
	return htonll(session_id);
}

size_t calculate_packet_number(size_t byte_zero, size_t first_byte_num, size_t psize) {
	// We know that both byte_zero and first_byte_num are divisible by psize. Packets are numbered from 0.
	assert(first_byte_num >= byte_zero);
	return ((first_byte_num - byte_zero) / (psize));
}

void put_into_buff(size_t put_num, size_t newest_num, size_t psize) {
	if (put_num > newest_num) {
		size_t packets_to_put = put_num - newest_num;
		size_t it = (cycle_buff_ptr->_head + 1) % cycle_buff_ptr->_capacity;

		for (size_t i = 0; i < packets_to_put; i++) {
			if (cycle_buff_ptr->is_index_inside_data(it)) {
				if (it == cycle_buff_ptr->_head) {
					cycle_buff_ptr->all_overriden(big_buff, psize);
					return;
				} else {
					if (i == packets_to_put - 1) {
						cycle_buff_ptr->memcpy(big_buff + 16, it, psize);
						cycle_buff_ptr->_tail = (it + 1) % cycle_buff_ptr->_capacity;
						cycle_buff_ptr->_head = it;
						cycle_buff_ptr->_is_missing[it] = false;
						return;
					} else {
						cycle_buff_ptr->clear(it, psize, psize);
						cycle_buff_ptr->_is_missing[it] = true;
					}
				}
			} else {
				if (i == packets_to_put - 1) {
					cycle_buff_ptr->memcpy(big_buff + 16, it, psize);
					cycle_buff_ptr->_head = it;
					cycle_buff_ptr->_taken_capacity++;
					cycle_buff_ptr->_is_missing[it] = false;
					return;
				} 
				cycle_buff_ptr->clear(it, psize, psize);
				cycle_buff_ptr->_is_missing[it] = true;
				cycle_buff_ptr->_taken_capacity++;
			}

			it = (it + 1) % cycle_buff_ptr->_capacity;
		}
	} else {
		size_t packet_diff = newest_num - put_num;
		if (cycle_buff_ptr->_taken_capacity < packet_diff) {
			return; // packet too old
		}
		size_t it = cycle_buff_ptr->_head;
		for (size_t i = 0; i < packet_diff; i++) {
			if (it == 0) {
				it = cycle_buff_ptr->_capacity - 1;
			} else {
				it--;
			}
		}
		cycle_buff_ptr->_is_missing[it] = false;
		cycle_buff_ptr->memcpy( big_buff + 16, it, psize );
	}
}

void print_buffer() {
	size_t helper = 0;
	while (is_running) {
		std::unique_lock lk(cycle_buff_ptr->_mutex);

		cycle_buff_ptr->_cond.wait(lk, []{
			return (cycle_buff_ptr->_was_three_quarters_full_flag && !cycle_buff_ptr->is_empty()); 
		});

		// Copying data from operational buffer to local buffer
		std::vector<uint8_t> local_buffer(psize_read);
		::memcpy(local_buffer.data(), cycle_buff_ptr->_data.data() + (cycle_buff_ptr->_tail) * psize_read, psize_read);
		cycle_buff_ptr->_taken_capacity--;
		cycle_buff_ptr->_tail = (cycle_buff_ptr->_tail + 1) % cycle_buff_ptr->_capacity;

		lk.unlock();

		// Printing data from local buffer to stdout
		::fwrite(local_buffer.data(), 1, psize_read, stdout);
		cycle_buff_ptr->print_missing(last_packet_num_received);
		helper++;
	}
}

size_t read_message(int socket_fd, struct sockaddr_in *client_address, uint8_t *buffer, size_t max_length) {
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0; // we do not request anything special
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           (struct sockaddr *) client_address, &address_length);
    if (len < 0) {
        PRINT_ERRNO();
    }
    return (size_t) len;
}

void scanner(int socket_fd) {
    struct addrinfo hint = {0}, *res;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo("255.255.255.255",  NULL, &hint, &res) != 0) {fatal("getaddrinfo");}

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(38477);
    addr.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
    freeaddrinfo(res);

    while (1) {
        char message[20] = "ZERO_SEVEN_COME_IN\n";
        if (sendto(socket_fd, message, 19, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {fatal("sendto");}
        std::cerr << "1. Sent lookup message to " << inet_ntoa(addr.sin_addr) << std::endl;
        sleep(5);
    }
}

void receive_lookup(int socket_fd) {
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        char buffer[1 << 16];
		memset(buffer, 0, sizeof(buffer));
        ssize_t packet_len = recvfromWithTimeout(socket_fd, buffer, sizeof(buffer), 0,
                                      (struct sockaddr *)&client_address, &client_address_len, 2);
									
		
		printf("Received message: %s\n", buffer);
        if (packet_len < 0) {
            PRINT_ERRNO();
        }

        // Convert the received message buffer to a string
        std::string received_message(reinterpret_cast<char *>(buffer), packet_len);
		std::regex pattern("^(BOREWICZ_HERE)\\s(\\S+)\\s(\\d+)\\s([\\x20-\\x7F]+)\\n$");
    	std::smatch matches;

		std::string input((char *) buffer);
		if (std::regex_match(input, matches, pattern)) {
			std::string station_addr = matches[2].str();
			std::string station_port = matches[3].str();
			std::string station_name = matches[4].str();
			RadioStation radio_station(station_name, station_addr, std::stoi(station_port));
			if (!(UIHandler::doesRadioStationExist(radio_station))) {
				UIHandler::addRadioStation(radio_station);
			}
            std::cerr << "4. Received lookup response from " << inet_ntoa(client_address.sin_addr) << std::endl << std::endl;
        }

		UIHandler::removeInactiveRadioStations();
    }
}

// void receiver_v2(size_t bsize) {
// 	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
// 	if (socket_fd < 0) {
// 		PRINT_ERRNO();
// 	}
//     struct addrinfo addrinfo;
//     struct addrinfo hints;
//     memset(&hints, 0, sizeof(hints));
//     hints.ai_family = AF_INET;
//     hints.ai_socktype = SOCK_DGRAM;
//     hints.ai_flags = AI_PASSIVE;

// 	struct ip_mreq mreq;

// 	size_t session_id = 0, byte_zero = 0, first_byte_num = 0, prev_psize_read = MAX_PACKET_SIZE + 420;
// 	bool running = true;
// 	struct sockaddr_in client_address;

// 	std::thread _send_thread = std::thread(print_buffer);

// 	// Create a file descriptor set to be used with select()
//     fd_set readFds;
//     int maxFd = std::max(socket_fd, pipefd[0]) + 1;

// 	while (running) {
// 		FD_ZERO(&readFds);
//         FD_SET(socket_fd, &readFds);
//         FD_SET(pipefd[0], &readFds);
// 		int readyFds = select(maxFd, &readFds, nullptr, nullptr, nullptr);
// 		if (readyFds == -1) {
//             std::cerr << "Failed to select" << std::endl;
//             return;
//         }
// 		if (FD_ISSET(pipefd[0], &readFds)) {
//             // Handle message from UIHandler pipe
//             int message;
//             if (read(pipefd[0], &message, sizeof(message)) == -1) {
//                 std::cerr << "Failed to read from pipe" << std::endl;
//                 return;
//             }
//             // Process the received message
//             std::lock_guard<std::mutex> lock(selectionMutex);
// 			if (UIHandler::getSelectedStationIndex() != message) {
// 				// switch to new station
// 				selected_radio_station = UIHandler::getRadioStation(message);

// 			}
//         }
// 		else {
// 			// We continue reading from the socket from selected_radio_station
// 			psize_read = read_message(socket_fd, &client_address, big_buff, MAX_PACKET_SIZE);
// 		}
// 	}

// }

void receiver(size_t bsize) {
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0) {
		PRINT_ERRNO();
	}
    struct addrinfo addrinfo;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct ip_mreq mreq;

	size_t session_id = 0, byte_zero = 0, first_byte_num = 0, prev_psize_read = MAX_PACKET_SIZE + 420;
	bool running = true;
	struct sockaddr_in client_address;

	std::thread _send_thread = std::thread(print_buffer);

	// Create a file descriptor set to be used with select()
    fd_set readFds;
    int maxFd = std::max(socket_fd, UIHandler::pipefd[0]) + 1;

	while (running) {
		FD_ZERO(&readFds);
        FD_SET(socket_fd, &readFds);
        FD_SET(UIHandler::pipefd[0], &readFds);
		int readyFds = select(maxFd, &readFds, nullptr, nullptr, nullptr);
		if (readyFds == -1) {
            std::cerr << "Failed to select" << std::endl;
            return;
        }
		if (FD_ISSET(UIHandler::pipefd[0], &readFds)) {
            // Handle message from UIHandler pipe
            int message;
            if (read(UIHandler::pipefd[0], &message, sizeof(message)) == -1) {
                std::cerr << "Failed to read from pipe" << std::endl;
                return;
            }
			// The selected station has changed, break from the loop
			mreq.imr_multiaddr.s_addr =  inet_addr(selected_radio_station.ip_address.c_str()); // od tego co przyjmuje
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);
			setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
			int enable = 1;
			setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
			enable = 1;
			setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
			running = false;
			break;
        }

		psize_read = read_message(socket_fd, &client_address, big_buff, MAX_PACKET_SIZE);
		if (psize_read < 16 || psize_read == SIZE_MAX)
			continue;
		psize_read -= 16;
		if (!(psize_read > 0 && psize_read <= bsize))
			continue;
		size_t new_byte_zero = get_first_byte_num();
		size_t new_session_id = get_session_id();
		if (new_session_id > session_id) {
			std::unique_lock lk(cycle_buff_ptr->_mutex);
			session_id = new_session_id;
			byte_zero = new_byte_zero;
			first_byte_num = new_byte_zero;
			prev_psize_read = psize_read;
			cycle_buff_ptr->change_size(psize_read, bsize);
			cycle_buff_ptr->clear();
			cycle_buff_ptr->_was_three_quarters_full_flag = false;
			lk.unlock();
		} else if (new_session_id < session_id) {
			continue;
		} else {	// new_session_id == session_id
			if (!(prev_psize_read == psize_read)) {
				assert(prev_psize_read == psize_read);
				continue;
			}
			first_byte_num = new_byte_zero;
		}

		std::unique_lock lk(cycle_buff_ptr->_mutex);
		packet_number = calculate_packet_number(byte_zero, first_byte_num, psize_read);
		put_into_buff(packet_number, last_packet_num_received, psize_read);
		last_packet_num_received = packet_number;
		if (cycle_buff_ptr->is_three_quarters_full()) {
			cycle_buff_ptr->_was_three_quarters_full_flag = true;
			cycle_buff_ptr->_cond.notify_one();
		}
		lk.unlock();
	}
	_send_thread.join();
}

void control_thread(int socket_fd, size_t bsize) {
	std::thread scanner_thread(scanner, socket_fd);
	std::thread receive_lookup_thread(receive_lookup, socket_fd);
    std::thread serverThread(&UIHandler::runTelnetServer);

	RadioStation radio_station1("Radio Maryja", "localhost", 20000);
	RadioStation radio_station2("Radio Zet", "localhost", 20000);

	UIHandler::addRadioStation(radio_station1);
	UIHandler::addRadioStation(radio_station2);

	while (1) {
		std::thread receiver_thread(receiver, bsize);

		receiver_thread.join();
	}

	serverThread.join();
	scanner_thread.join();
	receive_lookup_thread.join();
}

int main(int ac, char* av[]) {
	po::variables_map vm;
    if (set_parsed_arguments(vm, ac, av) == 1)
        return 1;
    size_t bsize = vm["BSIZE"].as<int>();
	std::string name = vm["NAME"].as<string>();
	int ctrl_socket = create_broadcast_reuse_socket();
	std::thread _control_thread = std::thread(control_thread, ctrl_socket, bsize);
	_control_thread.join();
	return 0;
}