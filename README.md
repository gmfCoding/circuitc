# CircuitC

Electronic circuit simulator written in C, based on [CircuitJS](https://github.com/pfalstad/circuitjs1) by Paul Falstad and Iain Sharp's [CircuitJS1](https://github.com/sharpie7/circuitjs1).

## Overview

CircuitC is a C implementation of the circuit simulation algorithms used in CircuitJS. It uses **Modified Nodal Analysis (MNA)** to simulate electronic circuits, solving the system of equations via LU decomposition with partial pivoting.

## Quick Start

### Web Interface (Recommended)

The simplest way to use CircuitC is through the web interface:

```bash
# Build and start the server (serves HTTP + WebSocket on one port)
cd web/backend
make
./circuit_server 8080

# Or use the convenience script from the project root:
./run_server.sh 8080
```

Then open your browser to `http://localhost:8080/` to access the interactive circuit simulator.

The single `circuit_server` executable handles:
- **Static file serving** (HTML, CSS, JavaScript)
- **WebSocket simulation backend** (real-time circuit simulation)
- **Circuit loading** from CircuitJS format files

### Command Line Interface

For batch processing or integration into other tools:

```bash
# Build the project
make

# Load and simulate a circuit from CircuitJS format
./build/load_circuit examples/test_simple.txt 100

# Or use circuits from CircuitJS library
./build/load_circuit /workspaces/circuitjs1_original/tests/cir-amp-741.txt 100
```

See [LOADER.md](LOADER.md) for detailed documentation on the circuit file format and supported elements.

## Features

- **Core simulation engine** with matrix-based circuit analysis
- **Circuit file loader** - Load circuits from CircuitJS format `.txt` files
- **Linear circuit elements:**
  - Resistors
  - Capacitors (with trapezoidal and backward Euler integration)
  - Inductors
  - Voltage sources
  - Current sources
- **Nonlinear elements:**
  - Diodes (Shockley equation with companion model)
  - NPN/PNP Bipolar transistors (simplified Ebers-Moll model)
  - Switches (open/closed states)
- **Time-domain simulation** with configurable timestep
- **Nonlinear circuit solver** with iterative convergence
- **Efficient LU factorization** solver

## Quick Start

### Loading and Simulating a Circuit File

```bash
# Build the project
make

# Load and simulate a circuit from CircuitJS format
./build/load_circuit examples/test_simple.txt 100

# Or use circuits from CircuitJS library
./build/load_circuit /workspaces/circuitjs1_original/tests/cir-amp-741.txt 100
```

See [LOADER.md](LOADER.md) for detailed documentation on the circuit file format and supported elements.

## Project Structure

```
circuitc/
├── src/
│   ├── circuit.h        # Main header with data structures
│   ├── solver.c         # LU decomposition matrix solver
│   ├── simulation.c     # Core simulation and stamping functions
│   ├── elements.c       # Circuit element implementations
│   └── loader.c         # Circuit file loader (CircuitJS format)
├── examples/
│   ├── examples.c       # Example circuits and test programs
│   ├── load_circuit.c   # Circuit file loader test program
│   └── test_simple.txt  # Example circuit file
├── Makefile
├── README.md
└── LOADER.md            # Circuit loader documentation
```

## Building

Requirements:
- GCC or compatible C compiler
- Make
- Standard C math library

Build the project:

```bash
make
```

This creates executable examples in the `build/` directory.

## Running Examples

Run the example circuits:

```bash
make test
```

Or run directly:

```bash
./build/examples
```

## Example Circuits

The examples demonstrate:

1. **Voltage Divider** - Simple resistive circuit
2. **RC Circuit** - Capacitor charging through resistor
3. **LC Oscillator** - Resonant circuit with inductor and capacitor
4. **Complex Circuit** - Multi-node circuit with multiple sources

## How It Works

CircuitC implements Modified Nodal Analysis:

1. **Build Circuit Matrix**: Each element "stamps" its contribution into a system matrix using Kirchhoff's laws
2. **LU Factorization**: The matrix is factored once (for linear circuits) or each iteration (for nonlinear circuits)
3. **Solve System**: Forward/backward substitution solves for node voltages
4. **Time Integration**: Capacitors and inductors use companion models with numerical integration

### Stamping Examples

**Resistor** (conductance method):
```
G = 1/R
Matrix[n1][n1] += G
Matrix[n1][n2] -= G
Matrix[n2][n1] -= G
Matrix[n2][n2] += G
```

**Voltage Source** (adds equation and unknown current):
```
Matrix[vn][n1] = -1
Matrix[vn][n2] = 1
RightSide[vn] = V
Matrix[n1][vn] = 1
Matrix[n2][vn] = -1
```

**Capacitor** (companion model):
```
Equivalent to current source in parallel with resistor
R_comp = Δt / (2C)  (for trapezoidal integration)
I_source = C * (V_new - V_old) / Δt
```

## API Usage

```c
#include "circuit.h"

// Create circuit
Circuit *circuit = circuit_create();
set_time_step(circuit, 1e-5);  // 10 microseconds

// Add elements (nodes numbered 1, 2, 3..., 0 is ground)
circuit_add_element(circuit, element_create_voltage_source(1, 0, 5.0));
circuit_add_element(circuit, element_create_resistor(1, 2, 1000.0));
circuit_add_element(circuit, element_create_capacitor(2, 0, 10e-6));

// Analyze circuit (builds and factors matrix)
circuit_analyze(circuit);

// Simulate
for (int i = 0; i < 1000; i++) {
    circuit_step(circuit);
    double voltage = get_node_voltage(circuit, 2);
    printf("V = %.6f\n", voltage);
}

// Cleanup
circuit_destroy(circuit);
```

## Web Frontend

CircuitC includes a complete web-based circuit viewer with real-time visualization:

### Features
- **WebSocket server** (C, raw implementation with binary protocol)
- **HTML/CSS/JavaScript frontend** with Canvas rendering
- **Real-time visualization** of voltages and currents
- **Interactive controls** (start/stop/step/reset)
- **Adjustable simulation speed** (0.1x to 5x)
- **Visual feedback** (voltage colors, current-based wire thickness)

### Quick Start

```bash
# Build and start the web server
cd web/backend
make
./circuit_server 8080

# In another terminal, serve the frontend
cd web/frontend
python3 -m http.server 8000

# Open browser to http://localhost:8000
```

Or use the quick start script:

```bash
cd web
./start.sh
```

### Usage
1. Enter circuit file path (e.g., `../../examples/test_comprehensive.txt`)
2. Click "Load Circuit"
3. Use controls to start/stop/step simulation
4. Adjust speed and current visualization sliders
5. Watch real-time voltage/current updates on the canvas

See [web/README.md](web/README.md) and [WEB_IMPLEMENTATION.md](WEB_IMPLEMENTATION.md) for detailed documentation.

## References

- [CircuitJS1 by Iain Sharp](https://github.com/sharpie7/circuitjs1) - JavaScript/GWT implementation
- [Original CircuitJS by Paul Falstad](http://www.falstad.com/circuit/)
- "Electronic Circuit and System Simulation Methods" by Pillage et al.

## License

Based on CircuitJS which is licensed under GPLv2.

This implementation follows the same license: **GNU General Public License v2.0**

## Credits

- **Paul Falstad** - Original CircuitJS Java applet
- **Iain Sharp** - CircuitJS1 browser version
- Circuit simulation algorithms based on Modified Nodal Analysis

## Future Enhancements

Completed:
- ✅ More circuit elements (diodes, transistors, switches)
- ✅ Circuit file format parser (CircuitJS `.txt` format)
- ✅ Graphical output (web-based Canvas visualization)
- ✅ Interactive GUI (HTML/CSS/JavaScript frontend)

Potential additions:
- AC analysis (frequency domain)
- More element types (op-amps, MOSFETs, transformers)
- Circuit editing in browser
- Waveform plotting (oscilloscope view)
- Performance optimizations (sparse matrices)
- WebAssembly port for client-side simulation

# License
This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.