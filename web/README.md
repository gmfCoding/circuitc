# CircuitC Web Viewer

A web-based circuit simulator viewer with real-time visualization using WebSocket communication.

## Architecture

- **Backend**: C WebSocket server (`circuit_server`)
  - Raw WebSocket implementation (no external libraries except OpenSSL for handshake)
  - Binary protocol for efficient data transfer
  - Real-time circuit simulation with adjustable speed
  - Multi-threaded (simulation thread per client)

- **Frontend**: HTML/CSS/JavaScript
  - Canvas-based circuit visualization
  - Live voltage and current display
  - Interactive controls (start/stop/step/reset)
  - Adjustable simulation speed and current visualization

## Protocol

All messages use binary format with network byte order (big-endian).

### Message Types

#### Circuit Data (0x01)
Sent by server when circuit is loaded.

```
[0x01][element_count:2][node_count:2][element_data...]
```

Each element:
```
[type:1][node0:2][node1:2][node2:2][value:8][current:8][v0:8][v1:8][v2:8]
```

#### Simulation Update (0x02)
Sent by server during simulation.

```
[0x02][time:8][step_count:4][element_updates...]
```

Each element update:
```
[current:8][v0:8][v1:8][v2:8]
```

#### Control (0x03)
Sent by client to control simulation.

```
[0x03][command:1][optional_data...]
```

Commands:
- `CMD_START (0x01)`: Start simulation
- `CMD_STOP (0x02)`: Stop simulation
- `CMD_STEP (0x03)`: Execute single step
- `CMD_RESET (0x04)`: Reset circuit
- `CMD_SET_SPEED (0x05) + [speed:4]`: Set speed multiplier
- `CMD_SET_CURRENT_VIS (0x06) + [factor:4]`: Set current visualization factor

#### Load File (0x04)
Sent by client to load a circuit file.

```
[0x04][filename_string...]
```

#### Error (0xFF)
Sent by server on error.

```
[0xFF][error_code:1]
```

Error codes:
- `0x01`: Load error
- `0x02`: Simulation error

## Building

```bash
cd web/backend
make
```

Requirements:
- GCC
- OpenSSL development libraries (`libssl-dev`)
- pthread

## Running

### Start Server

```bash
cd web/backend
./circuit_server [port]
```

Default port is 8080.

### Open Frontend

Open `web/frontend/index.html` in a web browser. Or serve with:

```bash
cd web/frontend
python3 -m http.server 8000
```

Then navigate to `http://localhost:8000`

## Usage

1. **Start the server**: `./circuit_server 8080`

2. **Open the web interface**: Open `index.html` in a browser

3. **Load a circuit**: 
   - Enter the path to a circuit file (e.g., `../../examples/test_comprehensive.txt`)
   - Click "Load Circuit"
   - The circuit topology will appear on the canvas

4. **Control simulation**:
   - **Start**: Begin continuous simulation
   - **Stop**: Pause simulation
   - **Step**: Execute single timestep
   - **Reset**: Reset to initial state

5. **Adjust visualization**:
   - **Speed slider**: Control simulation speed (0.1x to 5x)
   - **Current vis slider**: Adjust wire thickness based on current (0.1x to 10x)

## Visualization

- **Wire color**: Indicates voltage level
  - Red: High positive voltage (>3V)
  - Orange: Medium voltage (1-3V)
  - Green: Near ground (-1 to 1V)
  - Blue: Negative voltage (<-1V)
  
- **Wire thickness**: Indicates current magnitude (scaled by current vis factor)

- **Element symbols**:
  - Resistor: Zigzag line
  - Capacitor: Two parallel plates
  - Inductor: Coiled line
  - Voltage source: Circle with 'V'
  - Current source: Circle with 'I'
  - Diode: Triangle with line
  - Transistor: NPN/PNP symbol with arrow
  - Switch: Hinged contact

## Example Circuits

Available in `examples/`:
- `test_simple.txt`: Basic RC circuit
- `test_diode.txt`: Diode rectifier
- `test_switch.txt`: Switch circuit
- `test_comprehensive.txt`: Mixed components

You can also load CircuitJS format files (`.txt` files from CircuitJS).

## Development

### Backend Structure

```
web/backend/
├── websocket.h       # WebSocket protocol definitions
├── websocket.c       # WebSocket implementation
├── server.c          # Main server and message handlers
└── Makefile          # Build configuration
```

### Frontend Structure

```
web/frontend/
├── index.html        # Main page structure
├── style.css         # Styling
└── circuit.js        # Circuit viewer and WebSocket client
```

## Troubleshooting

**Connection refused**: Ensure the server is running on the correct port.

**Circuit not loading**: Check the file path is correct relative to the server's working directory.

**No visualization**: Ensure WebSocket connection is established (status indicator shows "Connected").

**Slow simulation**: Reduce speed multiplier or decrease current visualization factor.
