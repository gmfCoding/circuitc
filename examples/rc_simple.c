// Simple RC Circuit Example
#include "circuit.h"
#include <stdio.h>

int main(void) {
    // Create circuit
    Circuit *circuit = circuit_create();
    set_time_step(circuit, 1e-4);  // 100 microseconds
    
    // Build RC circuit: 5V -> 1kΩ resistor -> capacitor (10 µF) -> ground
    //   Node 0: Ground
    //   Node 1: Voltage source (+)
    //   Node 2: Between resistor and capacitor
    
    circuit_add_element(circuit, element_create_voltage_source(1, 0, 5.0));   // 5V source
    circuit_add_element(circuit, element_create_resistor(1, 2, 1000.0));       // 1kΩ resistor
    circuit_add_element(circuit, element_create_capacitor(2, 0, 10e-6));       // 10µF cap
    
    // Analyze circuit (build and factor matrix)
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Circuit analysis failed!\n");
        return 1;
    }
    
    // Simulate for 50ms (5 time constants, τ = RC = 10ms)
    printf("Time (ms), Capacitor Voltage (V), Current (mA)\n");
    
    for (int i = 0; i < 500; i++) {
        circuit_step(circuit);
        
        if (i % 10 == 0) {  // Print every 10th step
            double time_ms = circuit->time * 1000.0;
            double v_cap = get_node_voltage(circuit, 2);
            double i_cap = circuit->elements[2]->current * 1000.0;  // mA
            
            printf("%.2f, %.6f, %.6f\n", time_ms, v_cap, i_cap);
        }
    }
    
    // Cleanup
    circuit_destroy(circuit);
    return 0;
}
