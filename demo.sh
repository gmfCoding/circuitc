#!/bin/bash
# Circuit Loader Demo Script

echo "========================================="
echo "CircuitC Loader Demo"
echo "========================================="
echo ""

echo "1. Testing Simple Voltage Divider"
echo "-----------------------------------"
./build/load_circuit examples/test_simple.txt 50
echo ""
echo "Press Enter to continue..."
read

echo ""
echo "2. Testing RC Charging Circuit"
echo "-----------------------------------"
./build/load_circuit examples/test_rc.txt 100
echo ""
echo "Press Enter to continue..."
read

echo ""
echo "3. Testing CircuitJS Op-Amp Circuit"
echo "-----------------------------------"
if [ -f /workspaces/circuitjs1_original/tests/cir-amp-741.txt ]; then
    ./build/load_circuit /workspaces/circuitjs1_original/tests/cir-amp-741.txt 100
else
    echo "CircuitJS test files not found. Skipping."
fi
echo ""
echo "Press Enter to continue..."
read

echo ""
echo "4. Testing VCVS Circuit"
echo "-----------------------------------"
if [ -f /workspaces/circuitjs1_original/tests/vcvs-opamp.txt ]; then
    ./build/load_circuit /workspaces/circuitjs1_original/tests/vcvs-opamp.txt 50
else
    echo "CircuitJS test files not found. Skipping."
fi

echo ""
echo "========================================="
echo "Demo Complete!"
echo "========================================="
