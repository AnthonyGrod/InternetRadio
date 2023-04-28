CC = g++
CFLAGS = -Wall -Wextra -O2 -std=c++20
LDFLAGS = -lboost_program_options -pthread
INCLUDES = -I/usr/include/boost

SENDER_SOURCES := src/sender.cc $(COMMON_SOURCES)
RECEIVER_SOURCES := src/receiver.cc $(COMMON_SOURCES)

# List of object files
SENDER_OBJECTS := $(patsubst %.cc, %.o, $(SENDER_SOURCES))
RECEIVER_OBJECTS := $(patsubst %.cc, %.o, $(RECEIVER_SOURCES))

# Targets
all: sikradio-sender sikradio-receiver

sikradio-sender: $(SENDER_OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SENDER_OBJECTS) -o sikradio-sender $(LDFLAGS)

sikradio-receiver: $(RECEIVER_OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) $(RECEIVER_OBJECTS) -o sikradio-receiver $(LDFLAGS)

# Object files
%.o: %.cc
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(SENDER_OBJECTS) $(RECEIVER_OBJECTS) sikradio-sender sikradio-receiver