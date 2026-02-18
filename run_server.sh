#!/bin/bash
# Single executable circuit simulator server
# Serves both HTTP content and WebSocket simulation

PORT="${1:-8080}"

echo "==================================="
echo "Circuit Simulator Server"
echo "==================================="
echo ""
echo "Starting server on port $PORT..."
echo ""
echo "Access the simulator at:"
echo "  http://localhost:$PORT/"
echo ""
echo "The server handles:"
echo "  - Static files (HTML, CSS, JS)"
echo "  - WebSocket circuit simulation"
echo ""
echo "Press Ctrl+C to stop the server"
echo "==================================="
echo ""

cd "$(dirname "$0")/web/backend"
exec ./circuit_server "$PORT"
