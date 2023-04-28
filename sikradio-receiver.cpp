#include <ctime>
#include <iostream>
#include <netdb.h>
#include <string>
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

using namespace std;
namespace po = boost::program_options;

#define MAX_PACKET_SIZE 65535 // nie wiem czy nie powinno być 65536

#define ntohll(x) ((((uint64_t)ntohl(x)) << 32) + ntohl((x) >> 32))

uint8_t big_buff[MAX_PACKET_SIZE];
std::atomic<bool> is_running(true);
size_t psize_read = 0;
size_t last_packet_num_received = 0;
size_t packet_number = 0;

CycleBuff *cycle_buff_ptr = new CycleBuff();

int set_parsed_arguments(po::variables_map &vm, int ac, char* av[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")	// TODO: dest_addr should be optional but for debug purposes it's required.
        ("DEST_ADDR,a", po::value<string>()->default_value("localhost"), "set sender's ip address") // if it's -1 then it means that we want to receive packets from anyone
        ("DATA_PORT,P", po::value<int>()->default_value(20000 + (438477 % 10000)), "set sender's port")
        ("BSIZE,b", po::value<int>()->default_value(65536), "set buffor size (in bytes)")
    ;

    po::store(po::parse_command_line(ac, av, desc), vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }

    po::notify(vm);
    return 0;  
}

size_t get_first_byte_num() {
	return ntohll(((uint64_t *) big_buff)[1]);
}

size_t get_session_id() {
	return ntohll(((uint64_t *) big_buff)[0]);
}

// Calculating which packet the sender has sent us
size_t calculate_packet_number(size_t byte_zero, size_t first_byte_num, size_t psize) {
	// We know that both byte_zero and first_byte_num are divisible by psize. Packets are numbered from 0.
	assert(first_byte_num >= byte_zero);
	return ((first_byte_num - byte_zero) / (psize + 16));
}

void put_into_buff(size_t put_num, size_t newest_num, size_t psize) {
	//assert(put_num != newest_num);
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

// Function passed to a thread which is responsible for printing the buffer. 
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


int main(int ac, char* av[]) {
	po::variables_map vm;
    if (set_parsed_arguments(vm, ac, av) == 1) {
        return 1;
    }
    string dest_addr_str = vm["DEST_ADDR"].as<string>();
    int data_port = vm["DATA_PORT"].as<int>();
    size_t bsize = vm["BSIZE"].as<int>();
	int socket_fd = bind_socket(data_port);

	size_t session_id = 0, byte_zero = 0, first_byte_num = 0, prev_psize_read = MAX_PACKET_SIZE + 420;
	bool first_session_read = true;

	struct sockaddr_in client_address;
	std::thread _send_thread = std::thread(print_buffer);
	while (first_session_read) {
		// Receiving the packet into big_buffer
		// psize_read = recv(data_port, big_buff, MAX_PACKET_SIZE, 0);
		psize_read = read_message(socket_fd, &client_address, big_buff, MAX_PACKET_SIZE);
		if (psize_read < 16 || psize_read == SIZE_MAX) {
			continue;
		}
		psize_read -= 16;
		if (!(psize_read > 0 && psize_read <= bsize)) {
			continue;
		}
		size_t new_byte_zero = get_first_byte_num();
		size_t new_session_id = get_session_id();
		if (!(new_byte_zero % (psize_read + 16) == 0)) {
			continue;
		}

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
		// std::cerr << last_packet_num_received << std::endl; /// + 1 * (packet_number % 2 == 0)

		if (cycle_buff_ptr->is_three_quarters_full()) {
			cycle_buff_ptr->_was_three_quarters_full_flag = true;
			cycle_buff_ptr->_cond.notify_one();
		}

		lk.unlock();
	}

	_send_thread.join();
}