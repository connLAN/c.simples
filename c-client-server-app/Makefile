CC=gcc
CFLAGS=-Wall -g -fPIC -I/usr/local/include
SERVER_CFLAGS=$(CFLAGS) -DUSE_LOGGING -DUSE_JSON
CLIENT_CFLAGS=$(CFLAGS)
SRC=src/client.c src/server.c src/utils.c src/log.c
OBJ=$(SRC:.c=.o)
LIB_OBJS=src/utils.o src/log.o
EXEC_CLIENT=client
EXEC_SERVER=server
LIB_NAME=libipfilter
INSTALL_PREFIX=/usr/local

# cJSON settings
CJSON_CFLAGS=-I/usr/local/include
CJSON_LIBS=-L/usr/local/lib -lcjson
SERVER_CFLAGS += $(CJSON_CFLAGS)

all: $(EXEC_CLIENT) $(EXEC_SERVER) $(LIB_NAME).a $(LIB_NAME).so

$(EXEC_CLIENT): src/client.o src/utils.o
	$(CC) $(CLIENT_CFLAGS) -o $@ $^

$(EXEC_SERVER): src/server.o src/utils.o src/log.o
	$(CC) $(SERVER_CFLAGS) -o $@ $^ $(CJSON_LIBS)

$(LIB_NAME).a: $(LIB_OBJS)
	ar rcs $@ $^

$(LIB_NAME).so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

install: $(LIB_NAME).a $(LIB_NAME).so
	mkdir -p $(INSTALL_PREFIX)/lib
	mkdir -p $(INSTALL_PREFIX)/include
	cp $(LIB_NAME).* $(INSTALL_PREFIX)/lib/
	cp src/utils.h src/logger.h $(INSTALL_PREFIX)/include/

clean:
	rm -f $(OBJ) $(EXEC_CLIENT) $(EXEC_SERVER) $(LIB_NAME).a $(LIB_NAME).so test_runner

# Add to existing Makefile
TEST_SRC = tests/test_utils.c tests/Unity/unity/unity.c src/utils.c
TEST_EXEC = test_runner

test:
	$(CC) $(CFLAGS) -I. -Itests/Unity/unity $(TEST_SRC) -o $(TEST_EXEC)
	./$(TEST_EXEC)