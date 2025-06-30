#!/bin/bash

# Start the server in background
./server &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"

# Wait for server to start
sleep 2

# Test localhost connection (should work)
echo "Testing localhost connection..."
curl -s http://localhost:8080 > /dev/null
if [ $? -eq 0 ]; then
  echo "✓ Localhost connection successful"
else
  echo "✗ Localhost connection failed"
fi

# Test blacklisted IP (should fail)
echo "Testing blacklisted IP (192.168.1.1)..."
curl -s --interface 192.168.1.1 http://localhost:8080 > /dev/null
if [ $? -ne 0 ]; then
  echo "✓ Blacklisted IP correctly blocked"
else
  echo "✗ Blacklisted IP was allowed"
fi

# Test status display
echo "Requesting status via SIGUSR1..."
kill -SIGUSR1 $SERVER_PID
sleep 1 # Give server time to display status

# Stop server
echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID
echo "Smoke test complete"