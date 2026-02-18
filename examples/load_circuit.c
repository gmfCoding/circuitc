/*
 * Circuit Loader Test Program
 * Tests loading CircuitJS .txt files
 */

#include "../src/circuit.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <circuit_file.txt> [num_steps]\n", argv[0]);
        fprintf(stderr, "Example: %s /workspaces/circuitjs1_original/tests/fullrect.txt 100\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    int num_steps = 100;
    
    if (argc >= 3) {
        num_steps = atoi(argv[2]);
    }
    
    printf("Loading circuit from: %s\n", filename);
    
    /* Load the circuit from file */
    Circuit *circuit = circuit_load_from_file(filename);
    if (!circuit) {
        fprintf(stderr, "Failed to load circuit from %s\n", filename);
        return 1;
    }
    
    printf("\n=== Circuit Information ===\n");
    printf("Elements: %d\n", circuit->elementCount);
    printf("Nodes: %d\n", circuit->nodeCount);
    printf("Voltage sources: %d\n", circuit->voltageSourceCount);
    printf("Time step: %g seconds\n", circuit->timeStep);
    
    /* Analyze the circuit */
    printf("\n=== Analyzing Circuit ===\n");
    if (!circuit_analyze(circuit)) {
        fprintf(stderr, "Circuit analysis failed!\n");
        circuit_destroy(circuit);
        return 1;
    }
    printf("Circuit analysis successful!\n");
    
    /* Print initial node voltages */
    printf("\n=== Initial Node Voltages ===\n");
    for (int i = 1; i <= circuit->nodeCount && i <= 10; i++) {
        printf("Node %d: %.6f V\n", i, get_node_voltage(circuit, i));
    }
    if (circuit->nodeCount > 10) {
        printf("... (%d more nodes)\n", circuit->nodeCount - 10);
    }
    
    /* Run simulation */
    printf("\n=== Running Simulation ===\n");
    printf("Simulating %d steps...\n", num_steps);
    
    int steps_per_update = num_steps / 10;
    if (steps_per_update < 1) steps_per_update = 1;
    
    for (int step = 0; step < num_steps; step++) {
        if (!circuit_step(circuit)) {
            fprintf(stderr, "Simulation failed at step %d!\n", step);
            circuit_destroy(circuit);
            return 1;
        }
        
        /* Print progress */
        if ((step + 1) % steps_per_update == 0 || step == 0) {
            double time_ms = circuit->time * 1000.0;
            printf("Step %d: t = %.6f ms", step + 1, time_ms);
            
            /* Show voltage at first few nodes */
            if (circuit->nodeCount > 0) {
                printf(" | V(1) = %.6f", get_node_voltage(circuit, 1));
            }
            if (circuit->nodeCount > 1) {
                printf(", V(2) = %.6f", get_node_voltage(circuit, 2));
            }
            printf("\n");
        }
    }
    
    /* Print final node voltages */
    printf("\n=== Final Node Voltages ===\n");
    for (int i = 1; i <= circuit->nodeCount && i <= 10; i++) {
        printf("Node %d: %.6f V\n", i, get_node_voltage(circuit, i));
    }
    if (circuit->nodeCount > 10) {
        printf("... (%d more nodes)\n", circuit->nodeCount - 10);
    }
    
    /* Print element currents */
    printf("\n=== Element Currents ===\n");
    for (int i = 0; i < circuit->elementCount && i < 10; i++) {
        Element *elem = circuit->elements[i];
        const char *type_str = "Unknown";
        
        switch (elem->type) {
            case ELEM_RESISTOR: type_str = "Resistor"; break;
            case ELEM_CAPACITOR: type_str = "Capacitor"; break;
            case ELEM_INDUCTOR: type_str = "Inductor"; break;
            case ELEM_VOLTAGE_SOURCE: type_str = "VSource"; break;
            case ELEM_CURRENT_SOURCE: type_str = "ISource"; break;
            case ELEM_DIODE: type_str = "Diode"; break;
            case ELEM_TRANSISTOR_NPN: type_str = "NPN"; break;
            case ELEM_TRANSISTOR_PNP: type_str = "PNP"; break;
            case ELEM_SWITCH: type_str = "Switch"; break;
            case ELEM_WIRE: type_str = "Wire"; break;
            case ELEM_GROUND: type_str = "Ground"; break;
            default: break;
        }
        
        /* For transistors, show all three nodes */
        if (elem->type == ELEM_TRANSISTOR_NPN || elem->type == ELEM_TRANSISTOR_PNP) {
            printf("%s %d: Ic=%.6f A (C=%d B=%d E=%d)\n", 
                   type_str, i, elem->current, 
                   elem->nodes[0], elem->nodes[1], elem->nodes[2]);
        } else {
            printf("%s %d: %.6f A (nodes %d-%d)\n", 
                   type_str, i, elem->current, elem->nodes[0], elem->nodes[1]);
        }
    }
    if (circuit->elementCount > 10) {
        printf("... (%d more elements)\n", circuit->elementCount - 10);
    }
    
    printf("\n=== Simulation Complete ===\n");
    printf("Final time: %.6f ms\n", circuit->time * 1000.0);
    
    /* Cleanup */
    circuit_destroy(circuit);
    
    return 0;
}
