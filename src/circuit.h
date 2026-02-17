/*
 * CircuitC - Electronic Circuit Simulator in C
 * Based on CircuitJS by Paul Falstad and Iain Sharp
 * 
 * This file is the main header defining the core simulation structures
 */

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <stdbool.h>
#include <stddef.h>

/* Maximum sizes for circuit components */
#define MAX_NODES 1000
#define MAX_ELEMENTS 500
#define MAX_VOLTAGE_SOURCES 100
#define MAX_ITERATIONS 100
#define CONVERGENCE_THRESHOLD 1e-6

/* Element types */
typedef enum {
    ELEM_RESISTOR,
    ELEM_CAPACITOR,
    ELEM_INDUCTOR,
    ELEM_VOLTAGE_SOURCE,
    ELEM_CURRENT_SOURCE,
    ELEM_DIODE,
    ELEM_WIRE,
    ELEM_GROUND
} ElementType;

/* Forward declarations */
typedef struct Circuit Circuit;
typedef struct Element Element;

/* Element structure - base for all circuit elements */
struct Element {
    ElementType type;
    int nodes[2];           /* Node connections (0 = ground) */
    double value;           /* Resistance, capacitance, voltage, etc. */
    double current;         /* Current through element */
    double volts[2];        /* Voltages at each node */
    int voltSource;         /* Index of voltage source (if applicable) */
    
    /* For time-varying elements (capacitors, inductors) */
    double curSourceValue;  /* Equivalent current source value */
    double compResistance;  /* Companion resistance */
    
    /* Element-specific parameters */
    union {
        struct {            /* For capacitors */
            double voltdiff;
            double initialVoltage;
        } cap;
        struct {            /* For inductors */
            double inductorCurrent;
        } ind;
        struct {            /* For diodes */
            double leakage;
            double vdrop;
            double vCritical;
        } diode;
    } params;
    
    /* Methods (function pointers) */
    void (*stamp)(Element *elem, Circuit *circuit);
    void (*startIteration)(Element *elem, Circuit *circuit);
    void (*doStep)(Element *elem, Circuit *circuit);
    void (*reset)(Element *elem);
};

/* Circuit structure containing the entire simulation state */
struct Circuit {
    /* Element and node lists */
    Element *elements[MAX_ELEMENTS];
    int elementCount;
    int nodeCount;              /* Number of nodes (excluding ground) */
    int voltageSourceCount;
    
    /* Circuit matrix and solution vectors */
    double **circuitMatrix;     /* Main system matrix [n x n] */
    double *circuitRightSide;   /* Right-hand side vector [n] */
    double *nodeVoltages;       /* Solution voltages [nodeCount] */
    double *lastNodeVoltages;   /* Previous iteration voltages */
    int *circuitPermute;        /* Pivot indices for LU */
    int matrixSize;             /* Actual size of matrix */
    
    /* Original matrix (for nonlinear circuits) */
    double **origMatrix;
    double *origRightSide;
    
    /* Simulation parameters */
    double timeStep;            /* Time step in seconds */
    double time;                /* Current simulation time */
    bool circuitNonLinear;      /* True if circuit has nonlinear elements */
    bool converged;             /* True if iteration converged */
    int subIterations;          /* Iteration counter */
    
    /* Integration method */
    bool useTrapezoidal;        /* True for trapezoidal, false for backward Euler */
};

/* Core simulation functions */
Circuit* circuit_create(void);
void circuit_destroy(Circuit *circuit);
void circuit_add_element(Circuit *circuit, Element *elem);
void circuit_reset(Circuit *circuit);
bool circuit_analyze(Circuit *circuit);
bool circuit_step(Circuit *circuit);

/* Matrix stamping functions */
void stamp_resistor(Circuit *circuit, int n1, int n2, double r);
void stamp_voltage_source(Circuit *circuit, int n1, int n2, int vs, double v);
void stamp_current_source(Circuit *circuit, int n1, int n2, double i);
void stamp_matrix(Circuit *circuit, int i, int j, double x);
void stamp_right_side(Circuit *circuit, int i, double x);

/* Matrix solver functions */
bool lu_factor(double **a, int n, int *ipvt);
void lu_solve(double **a, int n, int *ipvt, double *b);

/* Element creation functions */
Element* element_create_resistor(int n1, int n2, double resistance);
Element* element_create_capacitor(int n1, int n2, double capacitance);
Element* element_create_inductor(int n1, int n2, double inductance);
Element* element_create_voltage_source(int n1, int n2, double voltage);
Element* element_create_current_source(int n1, int n2, double current);
Element* element_create_ground(int n);
void element_destroy(Element *elem);

/* Element-specific functions */
void capacitor_calculate_current(Element *elem);
void inductor_calculate_current(Element *elem);

/* Utility functions */
double get_node_voltage(Circuit *circuit, int node);
void set_time_step(Circuit *circuit, double dt);

#endif /* CIRCUIT_H */
