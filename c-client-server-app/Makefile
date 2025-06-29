CC=gcc
CFLAGS=-Wall -g -fPIC
SRC=src/client.c src/server.c src/utils.c src/log.c
OBJ=$(SRC:.c=.o)
LIB_OBJS=src/utils.o src/log.o
EXEC_CLIENT=client
EXEC_SERVER=server
LIB_NAME=libipfilter
INSTALL_PREFIX=/usr/local

all: $(EXEC_CLIENT) $(EXEC_SERVER) $(LIB_NAME).a $(LIB_NAME).so

$(EXEC_CLIENT): src/client.o src/utils.o
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC_SERVER): src/server.o src/utils.o src/log.o
	$(CC) $(CFLAGS) -o $@ $^

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