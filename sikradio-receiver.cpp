// #include <ctime>
// #include <iostream>
// #include <netdb.h>
// #include <string>
// #include <stdint.h>
// #include <stdio.h>
// #include <string>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <sys/types.h>

// #include "err.h"

// #include <unistd.h>
// #include <cstring>
// #include <cassert>

// #include <algorithm>

// cyclical_buffer::side cyclical_buffer::sideof(const size_t idx) {
//     if (_tail <= _head)
//         if (_tail <= idx && idx <= _head)
//             return LEFT;
//     else
//         if (idx <= _head)
//             return LEFT;
//         else if (_tail <= idx && idx < _capacity)
//             return RIGHT;
//     return NONE;
// }

// // bool cyclical_buffer::contains(const size_t idx) {
// //     return sideof(idx) != NONE;
// // }

// // size_t cyclical_buffer::left_chunk() {
// //     if (_tail <= _head)
// //         return _head - _tail;
// //     else 
// //         return _head;
// // } 

// // size_t cyclical_buffer::right_chunk() {
// //     if (_tail <= _head)
// //         return 0;
// //     else 
// //         return _capacity - _tail + 1;
// // } 

// // size_t cyclical_buffer::size() {
// //     return left_chunk() + right_chunk();
// // }

// void cyclical_buffer::memset(const size_t byte) {
//     ::memset(_data, _capacity, 0);
// }

// void cyclical_buffer::memset(const size_t byte, const size_t start, const size_t nbytes) {
//     auto side = sideof(start);
//     size_t end = start + nbytes - 1;
//     if (_tail <= _head) {
//         assert(side == LEFT);
//         assert(sideof(end) == LEFT);
//         ::memset(_data + _tail + start, byte, nbytes);
//     }
//     else {
//         assert(side != NONE);
//         if (side == RIGHT) {
//             size_t fst_chunk = std::max(nbytes, _capacity - start + 1);
//             size_t snd_chunk = nbytes - fst_chunk;
//             assert(sideof(snd_chunk) == LEFT);
//             ::memset(_data + _tail + start, byte, fst_chunk);
//             ::memset(_data, byte, snd_chunk);
//         } else {
//             assert(sideof(end) == LEFT);
//             ::memset(_data + start, byte, nbytes);
//         }
//     }
// }

// size_t cyclical_buffer::write(const int fd, const size_t start, const size_t nbytes) {
//     auto side = sideof(start);
//     size_t end = start + nbytes - 1;
//     if (_tail <= _head) {
//         assert(side == LEFT);
//         assert(sideof(end) == LEFT);
//         ENSURE(nbytes == ::write(fd, _data + _tail + start, nbytes));
//     }
//     else {
//         assert(side != NONE);
//         if (side == RIGHT) {
//             size_t fst_chunk = std::max(nbytes, _capacity - start + 1);
//             size_t snd_chunk = nbytes - fst_chunk;
//             assert(sideof(snd_chunk) == LEFT);
//             ENSURE(fst_chunk == ::write(fd, _data + _tail + start, fst_chunk));
//             ENSURE(snd_chunk == ::write(fd, _data, snd_chunk));
//         } else {
//             assert(sideof(end) == LEFT);
//             ENSURE(nbytes == ::write(fd, _data + start, nbytes));
//         }
//     }
// }

// // start must be a multiple of psize
// size_t cyclical_buffer::memcpy(const uint8_t* src, const size_t start, const size_t nbytes) {
//     auto side = sideof(start);
//     size_t end = start + nbytes - 1;
//     if (_tail <= _head) {
//         assert(side == LEFT);
//         assert(sideof(end) == LEFT);
//         ::memcpy(_data + _tail + start, src, nbytes);
//     }
//     else {
//         assert(side != NONE);
//         if (side == RIGHT) {
//             size_t fst_chunk = std::max(nbytes, _capacity - start + 1);
//             size_t snd_chunk = nbytes - fst_chunk;
//             assert(sideof(snd_chunk) == LEFT);
//             ::memcpy(_data + _tail + start, src, fst_chunk);
//             ::memcpy(_data, src, snd_chunk);
//         } else {
//             assert(sideof(end) == LEFT);
//             ::memcpy(_data + start, src, nbytes);
//         }
//     }
// }