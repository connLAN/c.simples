# Makefile for heartbeat server and tests

CC = gcc
CFLAGS = -Wall -Wextra -g -I./Unity/src
LDFLAGS = -lpthread -ljson-c

# Source files
SERVER_SRC = heartbeat_server.c main.c
TEST_SRC = test_heartbeat_new.c Unity/src/unity.c

# Default target
all: server test

# Build the server
server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o heartbeat_server $(SERVER_SRC) $(LDFLAGS)

# Build and run tests
test: $(TEST_SRC) heartbeat_server.c
	$(CC) $(CFLAGS) -o test_heartbeat $(TEST_SRC) heartbeat_server.c $(LDFLAGS)
	./test_heartbeat

# Clean build artifacts
clean:
	rm -f heartbeat_server test_heartbeat *.o

.PHONY: all server test clean