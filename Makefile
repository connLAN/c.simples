CC = gcc
CFLAGS = -Wall -O2 -I/path/to/curl/include
LDFLAGS = -L/path/to/curl/lib -lcurl

TARGET = sysinfo
SRC = sysinfo.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)