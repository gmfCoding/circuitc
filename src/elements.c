/*
 * CircuitC - Circuit Element Implementations
 * Implements resistors, capacitors, inductors, and sources
 * Based on CircuitJS by Paul Falstad
 */

#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== RESISTOR ========== */

static void resistor_stamp(Element *elem, Circuit *circuit) {
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], elem->value);
}

static void resistor_do_step(Element *elem, Circuit *circuit) {
    /* Calculate current through resistor */
    elem->current = (elem->volts[0] - elem->volts[1]) / elem->value;
}

Element* element_create_resistor(int n1, int n2, double resistance) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_RESISTOR;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->value = resistance;
    elem->stamp = resistor_stamp;
    elem->doStep = resistor_do_step;
    
    return elem;
}

/* ========== CAPACITOR ========== */

static void capacitor_stamp(Element *elem, Circuit *circuit) {
    /* Capacitor is represented as current source in parallel with resistor */
    /* The companion resistance depends on timestep and capacitance */
    double dt = circuit->timeStep;
    double c = elem->value;
    
    if (circuit->useTrapezoidal) {
        /* Trapezoidal integration */
        elem->compResistance = dt / (2.0 * c);
    } else {
        /* Backward Euler integration */
        elem->compResistance = dt / c;
    }
    
    /* Only stamp the resistor part during initial stamp */
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], elem->compResistance);
}

static void capacitor_start_iteration(Element *elem, Circuit *circuit) {
    /* Calculate the equivalent current source value based on previous state */
    double voltdiff = elem->params.cap.voltdiff;
    double current = elem->current;
    
    if (circuit->useTrapezoidal) {
        /* Trapezoidal: curSource = -V/R - I */
        elem->curSourceValue = -voltdiff / elem->compResistance - current;
    } else {
        /* Backward Euler: curSource = -V/R */
        elem->curSourceValue = -voltdiff / elem->compResistance;
    }
}

static void capacitor_do_step(Element *elem, Circuit *circuit) {
    /* Stamp the current source with the pre-calculated value */
    stamp_current_source(circuit, elem->nodes[0], elem->nodes[1], 
                        elem->curSourceValue);
}

void capacitor_calculate_current(Element *elem) {
    /* Update current based on new voltages */
    double voltdiff = elem->volts[0] - elem->volts[1];
    if (elem->compResistance > 0) {
        elem->current = voltdiff / elem->compResistance + elem->curSourceValue;
    }
    elem->params.cap.voltdiff = voltdiff;
}

static void capacitor_reset(Element *elem) {
    elem->current = 0.0;
    elem->curSourceValue = 0.0;
    elem->params.cap.voltdiff = elem->params.cap.initialVoltage;
}

Element* element_create_capacitor(int n1, int n2, double capacitance) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_CAPACITOR;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->value = capacitance;
    elem->params.cap.initialVoltage = 0.001;  /* Small initial voltage */
    elem->params.cap.voltdiff = elem->params.cap.initialVoltage;
    elem->stamp = capacitor_stamp;
    elem->startIteration = capacitor_start_iteration;
    elem->doStep = capacitor_do_step;
    elem->reset = capacitor_reset;
    
    return elem;
}

/* ========== INDUCTOR ========== */

static void inductor_stamp(Element *elem, Circuit *circuit) {
    /* Inductor is represented as current source in parallel with resistor */
    /* The companion resistance depends on timestep and inductance */
    double dt = circuit->timeStep;
    double l = elem->value;
    
    if (circuit->useTrapezoidal) {
        /* Trapezoidal integration */
        elem->compResistance = 2.0 * l / dt;
    } else {
        /* Backward Euler integration */
        elem->compResistance = l / dt;
    }
    
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], elem->compResistance);
}

static void inductor_start_iteration(Element *elem, Circuit *circuit) {
    /* Calculate the equivalent current source value based on previous state */
    double voltdiff = elem->volts[0] - elem->volts[1];
    double current = elem->current;
    
    if (circuit->useTrapezoidal) {
        /* Trapezoidal: curSource = V/R + I */
        elem->curSourceValue = voltdiff / elem->compResistance + current;
    } else {
        /* Backward Euler: curSource = I */
        elem->curSourceValue = current;
    }
}

static void inductor_do_step(Element *elem, Circuit *circuit) {
    /* Stamp the current source with the pre-calculated value */
    stamp_current_source(circuit, elem->nodes[0], elem->nodes[1], 
                        elem->curSourceValue);
}

void inductor_calculate_current(Element *elem) {
    /* Update current based on new voltages */
   double voltdiff = elem->volts[0] - elem->volts[1];
    if (elem->compResistance > 0) {
        elem->current = voltdiff / elem->compResistance + elem->curSourceValue;
    }
    elem->params.ind.inductorCurrent = elem->current;
}

static void inductor_reset(Element *elem) {
    elem->current = 0.0;
    elem->curSourceValue = 0.0;
    elem->params.ind.inductorCurrent = 0.0;
}

Element* element_create_inductor(int n1, int n2, double inductance) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_INDUCTOR;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->value = inductance;
    elem->params.ind.inductorCurrent = 0.0;
    elem->stamp = inductor_stamp;
    elem->startIteration = inductor_start_iteration;
    elem->doStep = inductor_do_step;
    elem->reset = inductor_reset;
    
    return elem;
}

/* ========== VOLTAGE SOURCE ========== */

static void voltage_source_stamp(Element *elem, Circuit *circuit) {
    stamp_voltage_source(circuit, elem->nodes[0], elem->nodes[1], 
                        elem->voltSource, elem->value);
}

static void voltage_source_do_step(Element *elem, Circuit *circuit) {
    /* Calculate current through voltage source */
    /* Current is stored in the solution vector after node voltages */
    int currentIndex = circuit->nodeCount + elem->voltSource;
    elem->current = circuit->circuitRightSide[currentIndex];
}

Element* element_create_voltage_source(int n1, int n2, double voltage) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_VOLTAGE_SOURCE;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->value = voltage;
    elem->stamp = voltage_source_stamp;
    elem->doStep = voltage_source_do_step;
    
    return elem;
}

/* ========== CURRENT SOURCE ========== */

static void current_source_stamp(Element *elem, Circuit *circuit) {
    stamp_current_source(circuit, elem->nodes[0], elem->nodes[1], elem->value);
}

static void current_source_do_step(Element *elem, Circuit *circuit) {
    /* Current is fixed at the source value */
    elem->current = elem->value;
}

Element* element_create_current_source(int n1, int n2, double current) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_CURRENT_SOURCE;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->value = current;
    elem->stamp = current_source_stamp;
    elem->doStep = current_source_do_step;
    
    return elem;
}

/* ========== GROUND ========== */

Element* element_create_ground(int n) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_GROUND;
    elem->nodes[0] = n;
    elem->nodes[1] = 0;  /* Ground is always node 0 */
    
    return elem;
}

/* ========== COMMON ========== */

void element_destroy(Element *elem) {
    free(elem);
}
