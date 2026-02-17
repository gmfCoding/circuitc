/*
 * CircuitC - Matrix Solver Implementation
 * LU decomposition with partial pivoting
 * Based on CircuitJS by Paul Falstad
 */

#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Allocate a 2D matrix */
static double** matrix_alloc(int n) {
    double **matrix = (double**)malloc(n * sizeof(double*));
    if (!matrix) return NULL;
    
    for (int i = 0; i < n; i++) {
        matrix[i] = (double*)calloc(n, sizeof(double));
        if (!matrix[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++)
                free(matrix[j]);
            free(matrix);
            return NULL;
        }
    }
    return matrix;
}

/* Free a 2D matrix */
static void matrix_free(double **matrix, int n) {
    if (!matrix) return;
    for (int i = 0; i < n; i++)
        free(matrix[i]);
    free(matrix);
}

/* 
 * LU factorization using Crout's method with partial pivoting
 * Factors matrix a into upper and lower triangular matrices
 * Returns true on success, false if matrix is singular
 */
bool lu_factor(double **a, int n, int *ipvt) {
    int i, j, k;
    
    /* Check for singular matrix (all-zero rows) */
    for (i = 0; i < n; i++) {
        bool row_all_zeros = true;
        for (j = 0; j < n; j++) {
            if (a[i][j] != 0.0) {
                row_all_zeros = false;
                break;
            }
        }
        if (row_all_zeros) {
            fprintf(stderr, "Singular matrix: row %d is all zeros\n", i);
            return false;
        }
    }
    
    /* Use Crout's method - loop through columns */
    for (j = 0; j < n; j++) {
        /* Calculate upper triangular elements for this column */
        for (i = 0; i < j; i++) {
            double q = a[i][j];
            for (k = 0; k < i; k++)
                q -= a[i][k] * a[k][j];
            a[i][j] = q;
        }
        
        /* Calculate lower triangular elements and find pivot */
        double largest = 0.0;
        int largestRow = -1;
        for (i = j; i < n; i++) {
            double q = a[i][j];
            for (k = 0; k < j; k++)
                q -= a[i][k] * a[k][j];
            a[i][j] = q;
            double x = fabs(q);
            if (x >= largest) {
                largest = x;
                largestRow = i;
            }
        }
        
        /* Pivot if necessary */
        if (j != largestRow) {
            for (k = 0; k < n; k++) {
                double temp = a[largestRow][k];
                a[largestRow][k] = a[j][k];
                a[j][k] = temp;
            }
        }
        
        /* Keep track of row interchanges */
        ipvt[j] = largestRow;
        
        /* Avoid division by zero */
        if (a[j][j] == 0.0) {
            fprintf(stderr, "Warning: avoided zero at [%d][%d]\n", j, j);
            a[j][j] = 1e-18;
        }
        
        /* Scale column j */
        if (j != n - 1) {
            double mult = 1.0 / a[j][j];
            for (i = j + 1; i < n; i++)
                a[i][j] *= mult;
        }
    }
    
    return true;
}

/*
 * Solve linear system using LU factorization
 * Solves A*x = b where A has been factored by lu_factor
 * On input, b contains the right-hand side
 * On output, b contains the solution x
 */
void lu_solve(double **a, int n, int *ipvt, double *b) {
    int i;
    
    /* Find first nonzero b element (forward elimination with pivoting) */
    for (i = 0; i < n; i++) {
        int row = ipvt[i];
        double swap = b[row];
        b[row] = b[i];
        b[i] = swap;
        if (swap != 0.0)
            break;
    }
    
    int bi = i++;
    
    /* Forward substitution using lower triangular matrix */
    for (; i < n; i++) {
        int row = ipvt[i];
        double tot = b[row];
        b[row] = b[i];
        
        for (int j = bi; j < i; j++)
            tot -= a[i][j] * b[j];
        b[i] = tot;
    }
    
    /* Back substitution using upper triangular matrix */
    for (i = n - 1; i >= 0; i--) {
        double tot = b[i];
        for (int j = i + 1; j < n; j++)
            tot -= a[i][j] * b[j];
        b[i] = tot / a[i][i];
    }
}

/*
 * Allocate and initialize circuit structure
 */
Circuit* circuit_create(void) {
    Circuit *circuit = (Circuit*)calloc(1, sizeof(Circuit));
    if (!circuit) return NULL;
    
    circuit->elementCount = 0;
    circuit->nodeCount = 0;
    circuit->voltageSourceCount = 0;
    circuit->timeStep = 5e-6;  /* Default 5 microseconds */
    circuit->time = 0.0;
    circuit->circuitNonLinear = false;
    circuit->useTrapezoidal = true;
    
    return circuit;
}

/*
 * Free circuit and all associated memory
 */
void circuit_destroy(Circuit *circuit) {
    if (!circuit) return;
    
    /* Free elements */
    for (int i = 0; i < circuit->elementCount; i++) {
        element_destroy(circuit->elements[i]);
    }
    
    /* Free matrices and vectors */
    if (circuit->circuitMatrix)
        matrix_free(circuit->circuitMatrix, circuit->matrixSize);
    if (circuit->origMatrix)
        matrix_free(circuit->origMatrix, circuit->matrixSize);
    
    free(circuit->circuitRightSide);
    free(circuit->origRightSide);
    free(circuit->nodeVoltages);
    free(circuit->lastNodeVoltages);
    free(circuit->circuitPermute);
    
    free(circuit);
}

/*
 * Add element to circuit
 */
void circuit_add_element(Circuit *circuit, Element *elem) {
    if (circuit->elementCount >= MAX_ELEMENTS) {
        fprintf(stderr, "Error: Maximum number of elements exceeded\n");
        return;
    }
    
    circuit->elements[circuit->elementCount++] = elem;
    
    /* Update node count */
    for (int i = 0; i < 2; i++) {
        if (elem->nodes[i] > circuit->nodeCount)
            circuit->nodeCount = elem->nodes[i];
    }
    
    /* Track voltage sources */
    if (elem->type == ELEM_VOLTAGE_SOURCE) {
        elem->voltSource = circuit->voltageSourceCount++;
    }
    
    /* Mark nonlinear if needed */
    if (elem->type == ELEM_DIODE)
        circuit->circuitNonLinear = true;
}

/*
 * Reset circuit to initial state
 */
void circuit_reset(Circuit *circuit) {
    circuit->time = 0.0;
    
    for (int i = 0; i < circuit->elementCount; i++) {
        Element *elem = circuit->elements[i];
        if (elem->reset)
            elem->reset(elem);
    }
    
    /* Clear voltages */
    if (circuit->nodeVoltages) {
        memset(circuit->nodeVoltages, 0, circuit->nodeCount * sizeof(double));
    }
    if (circuit->lastNodeVoltages) {
        memset(circuit->lastNodeVoltages, 0, circuit->nodeCount * sizeof(double));
    }
}

/*
 * Set timestep for simulation
 */
void set_time_step(Circuit *circuit, double dt) {
    circuit->timeStep = dt;
}

/*
 * Get voltage at a node
 */
double get_node_voltage(Circuit *circuit, int node) {
    if (node == 0 || node > circuit->nodeCount)
        return 0.0;
    return circuit->nodeVoltages[node - 1];
}
