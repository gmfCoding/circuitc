# CircuitC

Electronic circuit simulator written in C, based on [CircuitJS](https://github.com/pfalstad/circuitjs1) by Paul Falstad and Iain Sharp's [CircuitJS1](https://github.com/sharpie7/circuitjs1).

## Overview

CircuitC is a C implementation of the circuit simulation algorithms used in CircuitJS. It uses **Modified Nodal Analysis (MNA)** to simulate electronic circuits, solving the system of equations via LU decomposition with partial pivoting.

## Features

- **Core simulation engine** with matrix-based circuit analysis
- **Basic circuit elements:**
  - Resistors
  - Capacitors (with trapezoidal and backward Euler integration)
  - Inductors
  - Voltage sources
  - Current sources
- **Time-domain simulation** with configurable timestep
- **Nonlinear circuit support** (framework in place for diodes, transistors, etc.)
- **Efficient LU factorization** solver

## Project Structure

```
circuitc/
├── src/
│   ├── circuit.h        # Main header with data structures
│   ├── solver.c         # LU decomposition matrix solver
│   ├── simulation.c     # Core simulation and stamping functions
│   └── elements.c       # Circuit element implementations
├── examples/
│   └── examples.c       # Example circuits and test programs
├── Makefile
└── README.md
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

Potential additions:
- More circuit elements (diodes, transistors, op-amps)
- AC analysis (frequency domain)
- Graphical output (using SDL or similar)
- Circuit file format parser
- Interactive GUI
- Performance optimizations (sparse matrices)
