#ifndef CYCLE_BUFF_HPP
#define CYCLE_BUFF_HPP

#include <iostream>
#include <cstddef>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstring>

class CycleBuff {
public:
    size_t _size;                     // size of the buffer
    size_t _capacity;                 // capacity of the buffer. It is equal to floor(BSIZE / PSIZE)
    size_t _taken_capacity;           // number of chunks taken by data
    size_t _head;                     // head points to the chunk index just after last byte taken by data    
    size_t _tail;                     // tail points to first chunk position taken by data
    size_t _right_wall;               // last chunk index of the data we can write (equal to floor(BSIZE / PSIZE) - 1
    bool _was_three_quarters_full_flag; // flag that is set to true if buffer was three quarters full
    std::mutex _mutex;                // mutex for monitor
    std::condition_variable _cond;    // condition variable for monitor
    std::vector<uint8_t> _data;          // pointer to the data. All members will have guarenteed atomicity
	std::vector<bool> _is_missing;

    // initialize buffer
    CycleBuff(const size_t PSIZE, const size_t BSIZE);

    // default constructor
    CycleBuff();

	// Clear buffer and change size
    void change_size(const size_t PSIZE, const size_t BSIZE);

    // Fill whole buffer with zeros
    void clear();

    // Fill buffer with zeros from start * psize to start * psize + nbytes.
    void clear(const size_t start, const size_t nbytes, const size_t psize);

	// Fill buffer with zeros and mark them as lost besides last index which we put psize bytes from big_buff
    void all_overriden(uint8_t *big_buff, size_t psize);

    // Copy nbytes from src to buffer starting from start * psize index.
    void memcpy(const uint8_t* src, const size_t start, const size_t psize);

    // Checks if buffer is full with more than 3/4 of data
    bool is_three_quarters_full();

	// Checks if buffer is empty
    bool is_empty();

	// Checks if index is between _tail and _head included in both ends
    bool is_index_inside_data(size_t index);

	// Print which packet numebrs are missing
	void print_missing(size_t head_packet_num);

};

#endif


CycleBuff::CycleBuff(const size_t PSIZE, const size_t BSIZE)
  	: _size(BSIZE)
	, _capacity(floor(BSIZE / PSIZE))
	, _taken_capacity(0)
  	, _head(0)
  	, _tail(0)
	, _right_wall(_capacity - 1)
	, _was_three_quarters_full_flag(false)
  	, _mutex()
	, _cond()
	, _data(_capacity * PSIZE)
	, _is_missing(_capacity * PSIZE) {}

CycleBuff::CycleBuff()
    : _size(0)
    , _capacity(0)
	, _taken_capacity(0)
    , _head(0)
    , _tail(0)
	, _right_wall(0)
	, _was_three_quarters_full_flag(false)
    , _mutex()
	, _cond()
	, _data(0)
	, _is_missing(0) {}

void CycleBuff::change_size(const size_t PSIZE, const size_t BSIZE) {
	_head = 0;
  	_tail = 0;
	_size = BSIZE;
	_capacity = floor(BSIZE / PSIZE);
	_right_wall = _capacity - 1;
	_data.resize(_capacity * PSIZE);
	_is_missing.resize(_capacity * PSIZE);
	_taken_capacity = 0;
	fill(_data.begin(), _data.end(), 0);
	fill(_is_missing.begin(), _is_missing.end(), 0);
}

void CycleBuff::clear() {
    _head = 0;
    _tail = 0;
	_taken_capacity = 0;
    fill(_data.begin(), _data.end(), 0);
	fill(_is_missing.begin(), _is_missing.end(), 0);
}

void CycleBuff::all_overriden(uint8_t *big_buff, size_t psize) {
	_tail = 0;
	_head = _capacity - 1;
	_taken_capacity = _capacity;
	fill(_data.begin(), _data.end(), 0);
	fill(_is_missing.begin(), _is_missing.end(), true);					// Every packet is missing besides the one on the last index
	_is_missing[_capacity - 1] = false;
	CycleBuff::memcpy(big_buff + 16, _capacity - 1, psize);
}

void CycleBuff::clear(const size_t start, const size_t nbytes, const size_t psize) {
    assert(nbytes % psize == 0);
	assert(nbytes <= _capacity * psize);
    
    if (start * psize + nbytes <= _capacity * psize) {	// if we can clear all data required in one go
		::memset(_data.data() + start * psize, 0, nbytes);
    } else { 						   // else we need to clear data in two parts
		::memset(_data.data() + start * psize, 0, (_capacity - start) * psize);
		::memset(_data.data(), 0, nbytes - (_capacity - start) * psize);
	}
}

void CycleBuff::memcpy(const uint8_t* src, const size_t start, const size_t psize) {;
	::memcpy(_data.data() + start * psize, src, psize);
}

bool CycleBuff::is_three_quarters_full() {
	if (_taken_capacity >= (_capacity * 0.75) || _was_three_quarters_full_flag) {
		_was_three_quarters_full_flag = true;
		return true;
	}
	return false;
}

bool CycleBuff::is_empty() {
	return (_taken_capacity == 0);
}

bool CycleBuff::is_index_inside_data(size_t index) {
	if (_head < _tail) {
		if (index >= _tail || index <= _head) {
			return true;
		}
	} else if (_head == _tail) {
		if (index == _head) {
			return true;
		}
	} else {	// _tail < _head
		if (index >= _tail && index <= _head) {
			return true;
		}
	}
	return false;
}

void CycleBuff::print_missing(size_t head_packet_num) {
	size_t it = _tail;
	size_t tail_packet_num = head_packet_num - _taken_capacity + 1;
	size_t curr_packet_num = tail_packet_num;
	for (size_t i = 0; i < _taken_capacity; i++) {
		if (_is_missing[it] == true) {
			// std::cerr << "MISSING: BEFORE " << head_packet_num << " EXPECTED " << curr_packet_num << std::endl;
		}
		curr_packet_num++;
		it = (it + 1) % _capacity;
	}
}
