#pragma once

#include <cstddef>
#include <cstdint>
#include "./err.hh"
#include <unistd.h>
#include <cstring>
#include <algorithm>

static void dump(void* data, const size_t nbytes) {
    ENSURE(nbytes == ::write(STDOUT_FILENO, data, nbytes));
    memset(data, 0, nbytes);
}

class cyclical_buffer {
public:
    cyclical_buffer();  
    void dump(const size_t nbytes);
    void copy_pkt(const uint8_t* src, const size_t offset);
    void clear();
    void advance(const uint8_t* src, const size_t offset);

    size_t   psize;
    uint8_t* data;
    size_t   tail, head;
    size_t   capacity;

private:
    enum side { LEFT, RIGHT, NONE, };
    side sideof(const size_t idx);
};

cyclical_buffer::cyclical_buffer() 
    : data(nullptr), capacity(0), tail(0), head(0) {}

cyclical_buffer::side cyclical_buffer::sideof(const size_t idx) {
    if (tail <= head)
        if (tail <= idx && idx <= head)
            return LEFT;
    else
        if (idx <= head)
            return LEFT;
        else if (tail <= idx && idx < capacity)
            return RIGHT;
    return NONE;
}

void cyclical_buffer::clear() {
    tail = head = 0;
    ::memset(data, capacity, 0);
}

void cyclical_buffer::dump(const size_t nbytes) {
    ENSURE(nbytes % psize == 0);
    size_t end = tail + nbytes - 1;
    if (tail <= head) {
        ENSURE(sideof(end) == LEFT);
        ::dump(data + tail, nbytes);
    } else {
        size_t fst_chunk = std::max(nbytes, capacity - tail + 1) / psize * psize;
        size_t snd_chunk = nbytes - fst_chunk;
        ENSURE(sideof(snd_chunk) == LEFT);
        ::dump(data + tail, fst_chunk);
        ::dump(data, snd_chunk);
    }
    tail = (tail + nbytes) % capacity;
}

void cyclical_buffer::copy_pkt(const uint8_t* src, const size_t offset) {
    auto side = sideof(offset);
    ENSURE(offset % psize == 0);
    ENSURE(side != NONE);
    if (tail <= head)
        ::memcpy(data + tail + offset, src, psize);
    else 
        if (side == RIGHT)
            ::memcpy(data + tail + offset, src, psize);
        else
            ::memcpy(data + (offset - (capacity - tail + 1)), src, psize);
    
}