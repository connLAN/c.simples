# Makefile for Distributed Worker System

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

# Directories
COMMON_DIR = common
CENTRAL_DIR = central_server
WORKER_DIR = worker_server
CLIENT_DIR = client
BUILD_DIR = build

# Common source files
COMMON_SRCS = $(COMMON_DIR)/logger.c \
              $(COMMON_DIR)/net_utils.c \
              $(COMMON_DIR)/ring_buffer.c \
              $(COMMON_DIR)/protocol.c \
              $(COMMON_DIR)/common.c

# Central server source files
CENTRAL_SRCS = $(CENTRAL_DIR)/central_server.c \
               $(CENTRAL_DIR)/worker_registry.c \
               $(CENTRAL_DIR)/task_manager.c

# Worker server source files
WORKER_SRCS = $(WORKER_DIR)/worker_server.c

# Client source files
CLIENT_SRCS = $(CLIENT_DIR)/client.c \
              $(CLIENT_DIR)/client_main.c

# Object files
COMMON_OBJS = $(COMMON_SRCS:%.c=$(BUILD_DIR)/%.o)
CENTRAL_OBJS = $(CENTRAL_SRCS:%.c=$(BUILD_DIR)/%.o)
WORKER_OBJS = $(WORKER_SRCS:%.c=$(BUILD_DIR)/%.o)
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(BUILD_DIR)/%.o)

# Executables
CENTRAL_SERVER = $(BUILD_DIR)/central_server_exec
WORKER_SERVER = $(BUILD_DIR)/worker_server_exec
CLIENT = $(BUILD_DIR)/client_exec

# Default target
all: directories $(CENTRAL_SERVER) $(WORKER_SERVER) $(CLIENT)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$(COMMON_DIR)
	@mkdir -p $(BUILD_DIR)/$(CENTRAL_DIR)
	@mkdir -p $(BUILD_DIR)/$(WORKER_DIR)
	@mkdir -p $(BUILD_DIR)/$(CLIENT_DIR)

# Central server
$(CENTRAL_SERVER): $(COMMON_OBJS) $(CENTRAL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Worker server
$(WORKER_SERVER): $(COMMON_OBJS) $(WORKER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Client
$(CLIENT): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile common source files
$(BUILD_DIR)/$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile central server source files
$(BUILD_DIR)/$(CENTRAL_DIR)/%.o: $(CENTRAL_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile worker server source files
$(BUILD_DIR)/$(WORKER_DIR)/%.o: $(WORKER_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile client source files
$(BUILD_DIR)/$(CLIENT_DIR)/%.o: $(CLIENT_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Run central server
run-central: $(CENTRAL_SERVER)
	$(CENTRAL_SERVER)

# Run worker server
run-worker: $(WORKER_SERVER)
	$(WORKER_SERVER)

# Run client
run-client: $(CLIENT)
	$(CLIENT)

# Phony targets
.PHONY: all clean directories run-central run-worker run-client