#!/bin/bash
# Distributed Worker System Smoke Test

echo "Starting smoke test..."
echo "----------------------"

# Clean previous build
echo "[1/6] Cleaning and rebuilding..."
make clean && make || { echo "Build failed"; exit 1; }

# Start services
echo "[2/6] Starting central server..."
./build/central_server_exec > central_server.log 2>&1 &
CENTRAL_PID=$!

echo "[3/6] Starting worker server..."
./build/worker_server_exec > worker_server.log 2>&1 &
WORKER_SERVER_PID=$!

echo "[4/6] Starting worker node..."
./build/worker_server_exec > worker.log 2>&1 &
WORKER_PID=$!

# Wait for services to initialize
sleep 5

# Run test client
echo "[5/6] Running test client..."
./build/client_exec -j 1 -d "test_data" -p 8888 -w || { echo "Client test failed"; exit 1; }

# Verify results
echo "[6/6] Verifying results..."
grep -q "Task completed" worker.log && echo "Test PASSED" || echo "Test FAILED"

# Cleanup
echo "Cleaning up..."
kill $CENTRAL_PID $WORKER_SERVER_PID $WORKER_PID
wait
