/*
 * CircuitC - Core Simulation Engine
 * Implements the Modified Nodal Analysis matrix stamping and simulation loop
 * Based on CircuitJS by Paul Falstad
 */

#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations for element-specific calculate functions */
void capacitor_calculate_current(Element *elem);
void inductor_calculate_current(Element *elem);

/* Helper function to allocate 2D matrix */
static double** alloc_matrix(int n) {
    double **matrix = (double**)malloc(n * sizeof(double*));
    if (!matrix) return NULL;
    
    for (int i = 0; i < n; i++) {
        matrix[i] = (double*)calloc(n, sizeof(double));
        if (!matrix[i]) {
            for (int j = 0; j < i; j++)
                free(matrix[j]);
            free(matrix);
            return NULL;
        }
    }
    return matrix;
}

/*
 * Stamp a resistor into the circuit matrix
 * Adds conductance (1/R) to matrix at appropriate positions
 */
void stamp_resistor(Circuit *circuit, int n1, int n2, double r) {
    if (r == 0.0) {
        fprintf(stderr, "Warning: zero resistance detected\n");
        r = 1e-9;  /* Small resistance to avoid singularity */
    }
    
    double g = 1.0 / r;  /* Conductance */
    
    if (isinf(g) || isnan(g)) {
        fprintf(stderr, "Warning: bad resistance %g (conductance %g)\n", r, g);
        return;
    }
    
    stamp_matrix(circuit, n1, n1, g);
    stamp_matrix(circuit, n2, n2, g);
    stamp_matrix(circuit, n1, n2, -g);
    stamp_matrix(circuit, n2, n1, -g);
}

/*
 * Stamp an independent voltage source into the circuit matrix
 * Voltage source from n1 (+) to n2 (-) with voltage v
 * Requires an additional row and column for the current
 */
void stamp_voltage_source(Circuit *circuit, int n1, int n2, int vs, double v) {
    /* Row/col for this voltage source (1-indexed like nodes) */
    int vn = circuit->nodeCount + vs + 1;
    
    /* Voltage constraint equations: V(n1) - V(n2) = v */
    stamp_matrix(circuit, vn, n1, 1.0);
    stamp_matrix(circuit, vn, n2, -1.0);
    stamp_right_side(circuit, vn, v);
    
    /* Current flow equations */
    stamp_matrix(circuit, n1, vn, 1.0);
    stamp_matrix(circuit, n2, vn, -1.0);
}

/*
 * Stamp a current source into the circuit matrix
 * Current flows from n1 to n2 with magnitude i
 * Only affects the right-hand side vector
 */
void stamp_current_source(Circuit *circuit, int n1, int n2, double i) {
    stamp_right_side(circuit, n1, -i);
    stamp_right_side(circuit, n2, i);
}

/*
 * Add value x to matrix element [i][j]
 * Node 0 is ground and is not included in matrix
 */
void stamp_matrix(Circuit *circuit, int i, int j, double x) {
    if (isinf(x)) {
        fprintf(stderr, "Warning: infinite value in stamp_matrix\n");
        return;
    }
    
    /* Convert to 0-based indices (node 0 is ground) */
    if (i > 0 && j > 0) {
        circuit->circuitMatrix[i - 1][j - 1] += x;
    }
}

/*
 * Add value x to right-hand side vector at position i
 */
void stamp_right_side(Circuit *circuit, int i, double x) {
    if (i > 0) {
        circuit->circuitRightSide[i - 1] += x;
    }
}

/*
 * Analyze circuit and build matrices
 * Calls stamp() method on each element
 */
bool circuit_analyze(Circuit *circuit) {
    int matrixSize = circuit->nodeCount + circuit->voltageSourceCount;
    
    if (matrixSize == 0) {
        fprintf(stderr, "Error: Empty circuit\n");
        return false;
    }
    
    circuit->matrixSize = matrixSize;
    
    /* Allocate matrices and vectors */
    circuit->circuitMatrix = alloc_matrix(matrixSize);
    circuit->origMatrix = alloc_matrix(matrixSize);
    circuit->circuitRightSide = (double*)calloc(matrixSize, sizeof(double));
    circuit->origRightSide = (double*)calloc(matrixSize, sizeof(double));
    circuit->nodeVoltages = (double*)calloc(circuit->nodeCount, sizeof(double));
    circuit->lastNodeVoltages = (double*)calloc(circuit->nodeCount, sizeof(double));
    circuit->circuitPermute = (int*)malloc(matrixSize * sizeof(int));
    
    if (!circuit->circuitMatrix || !circuit->origMatrix || 
        !circuit->circuitRightSide || !circuit->origRightSide ||
        !circuit->nodeVoltages || !circuit->lastNodeVoltages ||
        !circuit->circuitPermute) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return false;
    }
    
    /* Stamp all linear elements */
    for (int i = 0; i < circuit->elementCount; i++) {
        Element *elem = circuit->elements[i];
        if (elem->stamp) {
            elem->stamp(elem, circuit);
        }
    }
    
    /* Save original matrix for nonlinear circuits */
    for (int i = 0; i < matrixSize; i++) {
        memcpy(circuit->origMatrix[i], circuit->circuitMatrix[i], 
               matrixSize * sizeof(double));
        circuit->origRightSide[i] = circuit->circuitRightSide[i];
    }
    
    /* For linear circuits, factor once */
    if (!circuit->circuitNonLinear) {
        if (!lu_factor(circuit->circuitMatrix, matrixSize, circuit->circuitPermute)) {
            fprintf(stderr, "Error: Singular matrix (linear circuit)\n");
            return false;
        }
    }
    
    return true;
}

/*
 * Perform one simulation timestep
 * Returns true on success, false on error
 */
bool circuit_step(Circuit *circuit) {
    if (!circuit->circuitMatrix) {
        fprintf(stderr, "Error: Circuit not analyzed\n");
        return false;
    }
    
    int matrixSize = circuit->matrixSize;
    int maxSubIterations = circuit->circuitNonLinear ? MAX_ITERATIONS : 1;
    
    /* Save node voltages from last step */
    memcpy(circuit->lastNodeVoltages, circuit->nodeVoltages, 
           circuit->nodeCount * sizeof(double));
    
    /* Start iteration - called once per timestep for all elements */
    for (int i = 0; i < circuit->elementCount; i++) {
        Element *elem = circuit->elements[i];
        if (elem->startIteration) {
            elem->startIteration(elem, circuit);
        }
    }
    
    /* Subiterations for nonlinear convergence */
    for (int subiter = 0; subiter < maxSubIterations; subiter++) {
        circuit->converged = true;
        circuit->subIterations = subiter;
        
        /* Reset right side to original */
        memcpy(circuit->circuitRightSide, circuit->origRightSide,
               matrixSize * sizeof(double));
        
        /* For nonlinear circuits, reset matrix */
        if (circuit->circuitNonLinear) {
            for (int i = 0; i < matrixSize; i++) {
                memcpy(circuit->circuitMatrix[i], circuit->origMatrix[i],
                       matrixSize * sizeof(double));
            }
        }
        
        /* Update all elements (doStep for time-varying elements) */
        for (int i = 0; i < circuit->elementCount; i++) {
            Element *elem = circuit->elements[i];
            if (elem->doStep) {
                elem->doStep(elem, circuit);
            }
        }
        
        /* Factor matrix if nonlinear */
        if (circuit->circuitNonLinear) {
            /* Check for convergence */
            if (circuit->converged && subiter > 0) {
                break;
            }
            
            if (!lu_factor(circuit->circuitMatrix, matrixSize, circuit->circuitPermute)) {
                fprintf(stderr, "Error: Singular matrix at t=%g (iteration %d)\n", 
                        circuit->time, subiter);
                return false;
            }
        }
        
        /* Solve the system */
        lu_solve(circuit->circuitMatrix, matrixSize, circuit->circuitPermute,
                 circuit->circuitRightSide);
        
        /* Extract node voltages from solution */
        for (int i = 0; i < circuit->nodeCount; i++) {
            circuit->nodeVoltages[i] = circuit->circuitRightSide[i];
        }
        
        /* Update element voltages */
        for (int i = 0; i < circuit->elementCount; i++) {
            Element *elem = circuit->elements[i];
            elem->volts[0] = (elem->nodes[0] == 0) ? 0.0 : 
                             circuit->nodeVoltages[elem->nodes[0] - 1];
            elem->volts[1] = (elem->nodes[1] == 0) ? 0.0 : 
                             circuit->nodeVoltages[elem->nodes[1] - 1];
            elem->volts[2] = (elem->nodes[2] == 0) ? 0.0 : 
                             circuit->nodeVoltages[elem->nodes[2] - 1];
        }
        
        /* Calculate currents for all elements */
        for (int i = 0; i < circuit->elementCount; i++) {
            Element *elem = circuit->elements[i];
            /* Calculate current based on element type */
            if (elem->type == ELEM_RESISTOR) {
                elem->current = (elem->volts[0] - elem->volts[1]) / elem->value;
            } else if (elem->type == ELEM_CAPACITOR) {
                capacitor_calculate_current(elem);
            } else if (elem->type == ELEM_INDUCTOR) {
                inductor_calculate_current(elem);
            } else if (elem->type == ELEM_VOLTAGE_SOURCE) {
                int currentIndex = circuit->nodeCount + elem->voltSource;
                elem->current = circuit->circuitRightSide[currentIndex];
            } else if (elem->type == ELEM_CURRENT_SOURCE) {
                elem->current = elem->value;
            }
        }
        
        /* If linear, no need to iterate */
        if (!circuit->circuitNonLinear)
            break;
    }
    
    /* Advance time */
    circuit->time += circuit->timeStep;
    
    return true;
}
