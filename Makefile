CC = g++
CFLAGS = -Wall -Wextra -O2 -std=c++20
LDFLAGS = -lboost_program_options -pthread

all: sikradio-sender sikradio-receiver

sikradio-sender sikradio-receiver:
	$(CC) $(CFLAGS) $@.cpp err.h common.h CycleBuff.hpp RadioStation.hpp UIHandler.hpp -o $@ $(LDFLAGS)

clean:
	rm -f *.o sikradio-sender sikradio-receiver