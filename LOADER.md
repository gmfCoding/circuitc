# Circuit Loader Documentation

## Overview

The CircuitC loader can parse and simulate circuits from CircuitJS format `.txt` files. This allows you to load circuits from the extensive CircuitJS circuit library and run them in the C-based simulator backend.

## Supported Element Types

The loader currently supports the following circuit elements from the CircuitJS format:

| Code | Element Type | Parameters | Notes |
|------|-------------|-----------|-------|
| `r` | Resistor | x1 y1 x2 y2 flags resistance | Ohms |
| `c` | Capacitor | x1 y1 x2 y2 flags capacitance | Farads |
| `l` | Inductor | x1 y1 x2 y2 flags inductance | Henries |
| `v` | Voltage Source | x1 y1 x2 y2 flags waveform voltage ... | DC voltage (V) |
| `i` | Current Source | x1 y1 x2 y2 flags current | Amperes |
| `d` | Diode | x1 y1 x2 y2 flags model | Silicon diode (0.7V drop) |
| `t` | Transistor (NPN/PNP) | x1 y1 x2 y2 flags | Beta=100, simple Ebers-Moll model |
| `s` | Switch | x1 y1 x2 y2 flags state | Open or closed |
| `S` | Switch | x1 y1 x2 y2 flags state ... | Alternative format |
| `w` | Wire | x1 y1 x2 y2 flags | Implemented as low-resistance resistor |
| `g` | Ground | x y flags | Connected to node 0 |
| `R` | Rail (Voltage) | x y targetX targetY flags waveform voltage ... | Power supply rail |

### Not Yet Supported

- MOSFETs (`M`)
- JFETs (`j`)
- Transformers
- Op-amps and custom components (numbered codes like `212`, `409`, etc.)
- Other specialized components

These elements are ignored with a warning message during loading.

## File Format

CircuitJS files have the following format:

```
<simulation_parameters>
<element_line_1>
<element_line_2>
...
```

### Simulation Parameters (First Line)

```
version timeStep maxTime flags steps flags2
```

Example: `1 0.000005 10.20027730826997 50 5 43`

- `timeStep`: Simulation time step in seconds (e.g., 5e-6 = 5 microseconds)

### Element Lines

Each element is specified with:
- Element type code (single character or number)
- Coordinates (x1, y1, x2, y2) - used for node mapping
- Flags and additional parameters specific to the element type

## Usage

### Loading from File

```c
#include "circuit.h"

Circuit *circuit = circuit_load_from_file("path/to/circuit.txt");
if (!circuit) {
    fprintf(stderr, "Failed to load circuit\n");
    return 1;
}

// Analyze and simulate
circuit_analyze(circuit);
circuit_step(circuit);

// Cleanup
circuit_destroy(circuit);
```

### Loading from String

```c
const char *circuit_text = 
    "$ 1 0.000005 10 50 5 43\n"
    "v 208 128 208 208 0 0 40 5 0 0 0.5\n"
    "r 208 128 336 128 0 1000\n"
    "g 208 208 208 240 0\n";

Circuit *circuit = circuit_load_from_string(circuit_text);
```

### Command-Line Test Program

A test program `load_circuit` is provided:

```bash
./build/load_circuit <circuit_file.txt> [num_steps]
```

Example:
```bash
./build/load_circuit /workspaces/circuitjs1_original/tests/cir-amp-741.txt 100
```

This will:
1. Load the circuit from the file
2. Display circuit information (elements, nodes, voltage sources)
3. Analyze the circuit
4. Simulate for the specified number of steps
5. Display node voltages and element currents

## Example Circuits

### Simple Voltage Divider

```
$ 1 0.000005 10.20027730826997 50 5 43
v 208 128 208 208 0 0 40 5 0 0 0.5
r 208 128 336 128 0 1000
r 336 128 336 208 0 2000
w 336 208 208 208 0
g 208 208 208 240 0
```

This creates:
- 5V voltage source
- 1kΩ resistor
- 2kΩ resistor
- Ground connection

The voltage divider produces ~3.33V at the middle node.

### Diode Circuit

```
$ 1 0.000005 10 50 5 43
v 100 100 100 200 0 0 40 5 0 0 0.5
r 100 100 200 100 0 1000
d 200 100 200 200 0 default
w 200 200 100 200 0
g 100 200 100 240 0
```

This creates:
- 5V voltage source
- 1kΩ current-limiting resistor
- Silicon diode (forward-biased)
- Ground connection

The diode drops approximately 0.7V when conducting.

### Switch Circuit

```
$ 1 0.000005 10 50 5 43
v 100 100 100 200 0 0 40 10 0 0 0.5
S 100 100 200 100 0 false 0 2
r 200 100 200 200 0 1000
w 200 200 100 200 0
g 100 200 100 240 0
```

This creates:
- 10V voltage source
- Switch (initially open)
- 1kΩ load resistor
- Ground connection

The switch can be open (high resistance) or closed (low resistance).

### Testing with CircuitJS Files

The loader has been tested with various circuits from `/workspaces/circuitjs1_original/tests/`:

```bash
# Op-amp circuits
./build/load_circuit /workspaces/circuitjs1_original/tests/cir-amp-741.txt 100
./build/load_circuit /workspaces/circuitjs1_original/tests/vcvs-opamp.txt 50

# Full-wave rectifier (requires diode support)
./build/load_circuit /workspaces/circuitjs1_original/tests/fullrect.txt 50

# Custom test circuit
./build/load_circuit examples/test_simple.txt 100
```

## Node Mapping

The loader automatically maps (x,y) coordinates to node numbers:
- Each unique coordinate pair gets assigned a sequential node number (starting from 1)
- Node 0 is reserved for ground
- Elements sharing the same coordinate are electrically connected

## Current Limitations

1. **MOSFETs and JFETs**: Not yet implemented
2. **Time-Varying Sources**: Only DC sources are supported; AC and other waveforms are treated as DC
3. **Custom Components**: Subcircuits and macro components (numbered types) are not supported
4. **Coordinate Overlap**: Elements must share exact coordinates to be connected; graphical overlaps are not detected
5. **Transistor Model**: Simplified model - does not include full Early effect, temperature dependence, or parasitic capacitances

## Implementation Details

### Files
- `src/loader.c` - Circuit file parser implementation
- `src/circuit.h` - Function declarations
- `examples/load_circuit.c` - Test program

### Key Functions

```c
// Load circuit from file
Circuit* circuit_load_from_file(const char *filename);

// Load circuit from string
Circuit* circuit_load_from_string(const char *content);
```

Both functions return a fully constructed `Circuit` object ready for analysis and simulation.

## Future Enhancements

1. Diode and transistor support
2. AC source waveforms (sine, square, triangle)
3. Subcircuit expansion
4. Better error reporting and validation
5. Export to other formats
