/*
 * CircuitC - Example Programs
 * Demonstrates circuit simulation with various test circuits
 */

#define _USE_MATH_DEFINES
#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Print circuit state */
static void print_state(Circuit *circuit, bool verbose) {
    if (verbose) {
        printf("Time: %.6e s\n", circuit->time);
        printf("Node Voltages:\n");
        for (int i = 0; i < circuit->nodeCount; i++) {
            printf("  Node %d: %.6f V\n", i + 1, circuit->nodeVoltages[i]);
        }
        printf("Elements:\n");
        for (int i = 0; i < circuit->elementCount; i++) {
            Element *elem = circuit->elements[i];
            const char *type_name[] = {
                "Resistor", "Capacitor", "Inductor", "Voltage Source",
                "Current Source", "Diode", "Wire", "Ground"
            };
            printf("  %s: V0=%.6f V, V1=%.6f V, I=%.6e A\n",
                   type_name[elem->type],
                   elem->volts[0], elem->volts[1], elem->current);
        }
        printf("\n");
    } else {
        printf("%.6e", circuit->time);
        for (int i = 0; i < circuit->nodeCount; i++) {
            printf(",%.6f", circuit->nodeVoltages[i]);
        }
        for (int i = 0; i < circuit->elementCount; i++) {
            printf(",%.6e", circuit->elements[i]->current);
        }
        printf("\n");
    }
}

/* Example 1: Simple voltage divider */
void example_voltage_divider(void) {
    printf("=== Example 1: Voltage Divider ===\n");
    printf("Circuit: 10V source -> 1kΩ -> Node1 -> 1kΩ -> Ground\n\n");
    
    Circuit *circuit = circuit_create();
    
    /* Create elements:
     * V1: 10V source between node 1 and ground
     * R1: 1kΩ resistor between node 1 and node 2
     * R2: 1kΩ resistor between node 2 and ground
     */
    circuit_add_element(circuit, element_create_voltage_source(1, 0, 10.0));
    circuit_add_element(circuit, element_create_resistor(1, 2, 1000.0));
    circuit_add_element(circuit, element_create_resistor(2, 0, 1000.0));
    
    /* Analyze and solve */
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Failed to analyze circuit\n");
        circuit_destroy(circuit);
        return;
    }
    
    /* Run one step */
    circuit_step(circuit);
    
    printf("Results:\n");
    printf("  Node 1 (source): %.3f V\n", get_node_voltage(circuit, 1));
    printf("  Node 2 (middle): %.3f V (expected: 5.0 V)\n\n", 
           get_node_voltage(circuit, 2));
    
    circuit_destroy(circuit);
}

/* Example 2: RC Circuit */
void example_rc_circuit(void) {
    printf("=== Example 2: RC Circuit (Charging) ===\n");
    printf("Circuit: 5V source -> 1kΩ -> Node1 -> 10µF capacitor -> Ground\n\n");
    
    Circuit *circuit = circuit_create();
    set_time_step(circuit, 0.0001);  /* 100 µs timestep */
    
    /* Create RC circuit */
    circuit_add_element(circuit, element_create_voltage_source(1, 0, 5.0));
    circuit_add_element(circuit, element_create_resistor(1, 2, 1000.0));
    circuit_add_element(circuit, element_create_capacitor(2, 0, 10e-6));
    
    /* Analyze */
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Failed to analyze circuit\n");
        circuit_destroy(circuit);
        return;
    }
    
    /* Simulate for 0.05 seconds (5 time constants) */
    /* Time constant τ = RC = 1000Ω * 10µF = 0.01s */
    printf("Time (ms), Capacitor Voltage (V), Current (mA)\n");
    
    for (int i = 0; i < 500; i++) {
        circuit_step(circuit);
        
        /* Print every 10th step */
        if (i % 10 == 0) {
            double v_cap = get_node_voltage(circuit, 2);
            double i_cap = circuit->elements[2]->current;
            printf("%.3f, %.6f, %.6f\n", 
                   circuit->time * 1000.0, v_cap, i_cap * 1000.0);
        }
    }
    
    printf("\nFinal voltage: %.3f V (expected: ~5.0 V)\n\n", 
           get_node_voltage(circuit, 2));
    
    circuit_destroy(circuit);
}

/* Example 3: LC Oscillator */
void example_lc_oscillator(void) {
    printf("=== Example 3: LC Oscillator ===\n");
    printf("Circuit: 1mH inductor -> Node1 -> 100µF capacitor -> Ground\n");
    printf("         (with initial charge on capacitor)\n\n");
    
    Circuit *circuit = circuit_create();
    set_time_step(circuit, 1e-5);  /* 10 µs timestep */
    
    /* Create LC circuit */
    Element *inductor = element_create_inductor(1, 0, 1e-3);
    Element *capacitor = element_create_capacitor(1, 0, 100e-6);
    
    /* Set initial voltage on capacitor */
    capacitor->params.cap.initialVoltage = 5.0;
    capacitor->params.cap.voltdiff = 5.0;
    
    circuit_add_element(circuit, inductor);
    circuit_add_element(circuit, capacitor);
    
    /* Analyze */
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Failed to analyze circuit\n");
        circuit_destroy(circuit);
        return;
    }
    
    /* Calculate expected frequency */
    double L = 1e-3;
    double C = 100e-6;
    double f = 1.0 / (2.0 * M_PI * sqrt(L * C));
    printf("Expected oscillation frequency: %.2f Hz\n", f);
    printf("Expected period: %.3f ms\n\n", 1000.0 / f);
    
    printf("Time (ms), Voltage (V), Current (mA)\n");
    
    /* Simulate for 2 periods */
    int steps = (int)(2.0 / f / circuit->timeStep);
    for (int i = 0; i < steps && i < 1000; i++) {
        circuit_step(circuit);
        
        /* Print every 10th step */
        if (i % 10 == 0) {
            double v = get_node_voltage(circuit, 1);
            double i = inductor->current;
            printf("%.3f, %.6f, %.6f\n", 
                   circuit->time * 1000.0, v, i * 1000.0);
        }
    }
    
    printf("\n");
    circuit_destroy(circuit);
}

/* Example 4: Complex circuit */
void example_complex_circuit(void) {
    printf("=== Example 4: Complex Multi-Node Circuit ===\n");
    printf("Circuit: Two voltage sources, multiple resistors\n\n");
    
    Circuit *circuit = circuit_create();
    
    /* Create a more complex circuit:
     *     V1=10V
     *       |
     *      R1=1k
     *       |
     *     Node1 -- R2=2k -- Node2
     *       |                 |
     *      R3=1k            R4=1k
     *       |                 |
     *      GND     V2=5V --- GND
     */
    circuit_add_element(circuit, element_create_voltage_source(1, 0, 10.0));
    circuit_add_element(circuit, element_create_resistor(1, 2, 1000.0));
    circuit_add_element(circuit, element_create_resistor(2, 3, 2000.0));
    circuit_add_element(circuit, element_create_resistor(2, 0, 1000.0));
    circuit_add_element(circuit, element_create_voltage_source(3, 0, 5.0));
    circuit_add_element(circuit, element_create_resistor(3, 0, 1000.0));
    
    /* Analyze and solve */
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Failed to analyze circuit\n");
        circuit_destroy(circuit);
        return;
    }
    
    circuit_step(circuit);
    
    printf("Results:\n");
    printf("  Node 1: %.3f V\n", get_node_voltage(circuit, 1));
    printf("  Node 2: %.3f V\n", get_node_voltage(circuit, 2));
    printf("  Node 3: %.3f V\n\n", get_node_voltage(circuit, 3));
    
    circuit_destroy(circuit);
}

int main(int argc, char *argv[]) {
    printf("CircuitC - Electronic Circuit Simulator\n");
    printf("Based on CircuitJS by Paul Falstad\n");
    printf("========================================\n\n");
    
    /* Run examples */
    example_voltage_divider();
    example_rc_circuit();
    example_lc_oscillator();
    example_complex_circuit();
    
    printf("All examples completed successfully!\n");
    
    return 0;
}
