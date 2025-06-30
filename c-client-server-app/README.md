# C Client-Server Application

This project implements a simple client-server architecture in C, featuring IP address filtering through a blacklist and whitelist mechanism with dynamic configuration reloading.

## Core Features

- **Client-Server Communication**:
  - TCP-based communication on port 8080
  - Client sends "Hello from client" message
  - Server responds with acknowledgment

- **IP Address Filtering**:
  - Blacklist/Whitelist system
  - Supports up to 100 IP addresses
  - Dynamic reloading via SIGHUP signal (kill -HUP <pid>)

- **Logging System**:
  - Detailed debug output
  - Connection attempts logging
  - Filtering decisions logging

## Project Structure

```
c-client-server-app
├── src
│   ├── client.c        # Client implementation
│   │   - Creates TCP socket
│   │   - Connects to server
│   │   - Sends test message
│   │   - Receives server response
│   ├── server.c        # Server implementation
│   │   - Creates TCP server
│   │   - Binds to port 8080
│   │   - Implements IP filtering
│   │   - Handles SIGHUP for config reload
│   ├── utils.c         # Utility functions
│   │   - IP list loading
│   │   - IP validation
│   │   - Debug output
│   ├── blacklist.txt    # Blocked IP addresses
│   ├── whitelist.txt    # Allowed IP addresses
│   └── Makefile         # Build rules
└── README.md            # Project documentation
```

## Getting Started

### Prerequisites

- GCC compiler (version 9.4.0 or higher)
- Make utility
- Linux/Unix environment (for signal handling)

### Building the Project

```bash
make
```

This will compile both client and server executables in the `src` directory.

### Running the Server

```bash
./src/server
```

Server options:
- Runs on port 8080 by default
- Uses `blacklist.txt` and `whitelist.txt` from current directory
- Logs connections and filtering decisions to stdout

### Running the Client

```bash
./src/client
```

Client options:
- Connects to localhost:8080 by default
- Sends test message and displays server response

## IP Address Filtering Configuration

### File Format

Example `blacklist.txt`:
```
# Blocked IP addresses
192.168.1.100
10.0.0.5
# Malicious IP
203.0.113.42
```

Example `whitelist.txt`:
```
# Allowed IP addresses
192.168.1.101
10.0.0.1
```

Rules:
- One IP address per line
- Lines starting with # are comments
- Maximum 100 IP addresses per list
- Whitelist takes precedence over blacklist

### Dynamic Reloading

To reload configuration without restarting server:
```bash
kill -HUP $(pgrep server)
```

## Example Usage Scenarios

1. Basic Communication:
```bash
# Terminal 1
./src/server

# Terminal 2
./src/client
```

2. IP Filter Testing:
```bash
# Add client IP to blacklist
echo "127.0.0.1" > src/blacklist.txt
kill -HUP $(pgrep server)

# Client will now be blocked
./src/client
```

## Troubleshooting

**Server won't start:**
- Check if port 8080 is available: `netstat -tuln | grep 8080`
- Ensure you have permissions to bind to the port

**Client can't connect:**
- Verify server is running
- Check firewall settings
- Ensure client IP isn't in blacklist

**Configuration not reloading:**
- Verify server process ID is correct
- Check file permissions on blacklist.txt/whitelist.txt

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/fooBar`)
3. Commit your changes (`git commit -am 'Add some fooBar'`)
4. Push to the branch (`git push origin feature/fooBar`)
5. Create a new Pull Request

Testing requirements:
- All new code should include unit tests
- Manual testing of IP filtering scenarios
- Verify signal handling works correctly

## License

MIT License - see [LICENSE](LICENSE) file for details.