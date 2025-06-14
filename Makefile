# Compiler settings
CC = gcc
# Add -Wno-stringop-truncation if you want to suppress the strncpy warnings
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE -Wno-unused-parameter
LDFLAGS = -lpthread -lm -ljson-c # Added -ljson-c for json-c library
CLIENT_LDFLAGS = -lcurl -lpthread -lm -ljson-c  # Added -ljson-c here too

# Paths for sysinfo (adjust as needed)
CURL_INCLUDE ?= /usr/include
CURL_LIB ?= /usr/lib
JSON_C_INCLUDE ?= /usr/include/json-c  # Added JSON_C_INCLUDE
JSON_C_LIB ?= /usr/lib  # Added JSON_C_LIB

# Targets
SYSINFO_TARGET = sysinfo
HEARTBEAT_CLIENT = heartbeat_client
HEARTBEAT_SERVER = heartbeat_server

# Source files
SYSINFO_SRC = sysinfo.c
CLIENT_SRCS = heartbeat_client.c
SERVER_SRCS = heartbeat_server.c

# Object files
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

# Default target
all: $(SYSINFO_TARGET) $(HEARTBEAT_CLIENT) $(HEARTBEAT_SERVER)

# Sysinfo build
$(SYSINFO_TARGET): $(SYSINFO_SRC)
	$(CC) $(CFLAGS) -I$(CURL_INCLUDE) -I$(JSON_C_INCLUDE) -o $@ $< -L$(CURL_LIB) -L$(JSON_C_LIB) -lcurl -ljson-c -lm

# Heartbeat client build
$(HEARTBEAT_CLIENT): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(CLIENT_LDFLAGS)

# Heartbeat server build
$(HEARTBEAT_SERVER): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -I$(CURL_INCLUDE) -I$(JSON_C_INCLUDE) -c $< -o $@

# Cleanup
clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) \
	      $(HEARTBEAT_CLIENT) $(HEARTBEAT_SERVER) \
	      $(SYSINFO_TARGET)

# Install (optional)
install: all
	install -m 0755 $(HEARTBEAT_SERVER) /usr/local/bin/
	install -m 0755 $(HEARTBEAT_CLIENT) /usr/local/bin/
	install -m 0755 $(SYSINFO_TARGET) /usr/local/bin/

# Phony targets
.PHONY: all clean install
