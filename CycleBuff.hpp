#ifndef CYCLE_BUFF_HPP
#define CYCLE_BUFF_HPP

#include <iostream>
#include <cstddef>
#include <pthread.h>
#include <vector>
#include <cmath>

class CycleBuff {
public:
    size_t _size;                     // size of the buffer
    size_t _capacity;                 // capacity of the buffer. It is equal to floor(BSIZE / PSIZE)
    size_t _taken_capacity;           // number of chunks taken by data
    size_t _head;                     // head points to the chunk index just after last byte taken by data    
    size_t _tail;                     // tail points to first chunk position taken by data
    size_t _right_wall;               // last chunk index of the data we can write (equal to floor(BSIZE / PSIZE) - 1
    bool _was_three_quarters_full_flag; // flag that is set to true if buffer was three quarters full
    std::thread* _rec_thread_ptr;     // pointer to the thread that will receive data from nadajnik
    std::thread* _send_thread_ptr;    // pointer to the thread that will send data to stdout
    std::mutex _mutex;                // mutex for monitor
    std::condition_variable _cond;    // condition variable for monitor
    std::vector<char> _data;          // pointer to the data. All members will have guarenteed atomicity

    // initialize buffer
    CycleBuff(const size_t PSIZE, const size_t BSIZE);

    // default constructor
    CycleBuff();

    void change_size(const size_t PSIZE, const size_t BSIZE);

    // Fill whole buffer with zeros
    void clear();

    // Fill buffer with zeros from start * psize to start * psize + nbytes.
    void clear(const size_t start, const size_t nbytes, const size_t psize);

    void all_overriden(char *big_buff, size_t psize);

    // Write nbytes from buffer to fd starting from start * psize index.
    void write_from_buffer(const int fd, const size_t start, const size_t nbytes, const size_t psize);

    // Copy nbytes from src to buffer starting from start * psize index.
    void memcpy(const char* src, const size_t start, const size_t psize);

    // Checks if buffer is full with more than 3/4 of data
    bool is_three_quarters_full();

    bool is_empty();

    bool is_index_inside_data(size_t index);

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
	, _rec_thread_ptr(NULL)
	, _send_thread_ptr(NULL)
    , _mutex()
	, _cond()
	, _data(_capacity * PSIZE) {}

CycleBuff::CycleBuff()
    : _size(0)
    , _capacity(0)
	, _taken_capacity(0)
    , _head(0)
    , _tail(0)
	, _right_wall(0)
	, _was_three_quarters_full_flag(false)
	, _rec_thread_ptr(NULL)
	, _send_thread_ptr(NULL)
    , _mutex()
	, _cond()
	, _data(0) {}

// Change size of the buffer
void CycleBuff::change_size(const size_t PSIZE, const size_t BSIZE) {
	_head = 0;
    _tail = 0;
	_size = BSIZE;
	_capacity = floor(BSIZE / PSIZE);
	_right_wall = _capacity - 1;
	_data.resize(_capacity * PSIZE);
	_taken_capacity = 0;
	fill(_data.begin(), _data.end(), 0);
}

// Fill whole buffer with zeros
void CycleBuff::clear() {
    _head = 0;
    _tail = 0;
		_taken_capacity = 0;
    fill(_data.begin(), _data.end(), 0);
}

// Fill buffer with zeroes besides last index in which copy big_buff psize bytes.
void CycleBuff::all_overriden(char *big_buff, size_t psize) {
	_tail = 0;
	_head = _capacity - 1;
	_taken_capacity = _capacity;
	fill(_data.begin(), _data.end(), 0);
	CycleBuff::memcpy(big_buff + 16, _capacity - 1, psize);
}

// Fill buffer with zeros from start * psize to start * psize + nbytes. 
// CAUTION: nbytes <= _capacity * psize
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

// Write nbytes from buffer to fd starting from start index. Start index, nbytes must be divisible by PSIZE
// CAUTION: nbytes <= _capacity
void CycleBuff::write_from_buffer(const int fd, const size_t start, const size_t nbytes, const size_t psize) {
    assert(nbytes % psize == 0);
	assert(nbytes <= _capacity * psize);

	::write(fd, _data.data() + start * psize, nbytes);
}

// Copy psize bytes from src to buffer starting from start chunk index.
// CAUTION: nbytes <= _capacity
void CycleBuff::memcpy(const char* src, const size_t start, const size_t psize) {;
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
