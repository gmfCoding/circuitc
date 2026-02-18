#!/bin/bash
# Quick start script for CircuitC Web Viewer

echo "Starting CircuitC Web Viewer..."

# Check if server is built
if [ ! -f "backend/circuit_server" ]; then
    echo "Building server..."
    cd backend
    make
    cd ..
fi

# Start server in background
echo "Starting WebSocket server on port 8080..."
cd backend
./circuit_server 8080 &
SERVER_PID=$!
cd ..

echo "Server PID: $SERVER_PID"
echo ""
echo "Opening web interface..."

# Wait a moment for server to start
sleep 1

# Open browser (try different commands)
if command -v xdg-open > /dev/null; then
    xdg-open "http://localhost:8000/frontend/index.html"
elif command -v open > /dev/null; then
    open "http://localhost:8000/frontend/index.html"
else
    echo "Please open http://localhost:8000/frontend/index.html in your browser"
fi

# Start simple HTTP server for frontend
echo "Starting HTTP server on port 8000..."
cd frontend
python3 -m http.server 8000

# Cleanup on exit
trap "kill $SERVER_PID" EXIT
