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

/* ========== DIODE ========== */

/* Diode using companion model for nonlinear analysis */
static void diode_stamp(Element *elem, Circuit *circuit) {
    /* Initial stamp with forward resistance */
    double r = 1e3;  /* Initial forward resistance estimate */
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], r);
}

static void diode_start_iteration(Element *elem, Circuit *circuit) {
    (void)circuit;  /* Unused */
    /* Initialize diode voltage drop to typical forward voltage */
    if (elem->params.diode.vdrop == 0.0) {
        elem->params.diode.vdrop = 0.7;  /* Typical silicon diode */
    }
}

static void diode_do_step(Element *elem, Circuit *circuit) {
    /* Diode equation: I = Is * (exp(V/Vt) - 1)
     * Using companion model: linearize around operating point
     */
    double vd = elem->volts[0] - elem->volts[1];  /* Voltage across diode */
    double vt = elem->params.diode.thermalVoltage;
    double is = elem->params.diode.saturationCurrent;
    
    /* Limit voltage change for stability */
    double vdiff = vd - elem->params.diode.vdrop;
    if (vdiff > vt * 10) vdiff = vt * 10;
    if (vdiff < -vt * 10) vdiff = -vt * 10;
    vd = elem->params.diode.vdrop + vdiff;
    
    /* Calculate current using Shockley equation */
    double id, gd;
    if (vd >= 0) {
        /* Forward bias */
        double exp_term = exp(vd / vt);
        if (exp_term > 1e50) exp_term = 1e50;  /* Prevent overflow */
        id = is * (exp_term - 1.0);
        gd = is * exp_term / vt;  /* Conductance = dI/dV */
    } else {
        /* Reverse bias */
        id = -is;
        gd = is / vt;  /* Small conductance in reverse */
    }
    
    /* Prevent divide by zero */
    if (gd < 1e-12) gd = 1e-12;
    double rd = 1.0 / gd;  /* Dynamic resistance */
    
    /* Companion model: current source in parallel with resistor */
    double geq = gd;
    double ieq = id - gd * vd;
    
    /* Stamp companion model */
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], rd);
    stamp_current_source(circuit, elem->nodes[0], elem->nodes[1], ieq);
    
    /* Store values for next iteration */
    elem->params.diode.vdrop = vd;
    elem->current = id;
    
    /* Check convergence */
    if (fabs(vdiff) > elem->params.diode.thermalVoltage * 0.1) {
        circuit->converged = false;
    }
}

Element* element_create_diode(int n1, int n2) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_DIODE;
    elem->nodes[0] = n1;  /* Anode */
    elem->nodes[1] = n2;  /* Cathode */
    elem->nodes[2] = 0;
    
    /* Default diode parameters (silicon diode) */
    elem->params.diode.saturationCurrent = 1e-14;  /* 10 fA */
    elem->params.diode.thermalVoltage = 0.026;      /* 26 mV at room temp */
    elem->params.diode.vdrop = 0.0;
    elem->params.diode.vCritical = 0.7;
    
    elem->stamp = diode_stamp;
    elem->startIteration = diode_start_iteration;
    elem->doStep = diode_do_step;
    
    return elem;
}

/* ========== TRANSISTOR (Bipolar NPN/PNP) ========== */

/* Simple Ebers-Moll transistor model */
static void transistor_stamp(Element *elem, Circuit *circuit) {
    /* Initial stamp - transistors are highly nonlinear */
    /* Stamp base-emitter as diode, collector-emitter as resistor */
    stamp_resistor(circuit, elem->nodes[1], elem->nodes[2], 1e6);  /* Base-emitter */
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[2], 1e6);  /* Collector-emitter */
}

static void transistor_start_iteration(Element *elem, Circuit *circuit) {
    (void)circuit;  /* Unused */
    /* Initialize voltages */
    if (elem->params.trans.vbe == 0.0) {
        elem->params.trans.vbe = 0.0;
    }
}

static void transistor_do_step(Element *elem, Circuit *circuit) {
    /* Simple Ebers-Moll model for bipolar transistor
     * Nodes: 0=Collector, 1=Base, 2=Emitter
     */
    double vc = elem->volts[0];
    double vb = elem->volts[1];
    double ve = elem->volts[2];
    
    double vbe = vb - ve;
    double vce = vc - ve;
    double beta = elem->params.trans.beta;
    
    /* Adjust for PNP transistor */
    bool isPNP = (elem->type == ELEM_TRANSISTOR_PNP);
    if (isPNP) {
        vbe = -vbe;
        vce = -vce;
    }
    
    double ib, ic, ie;
    
    /* Check operating region */
    if (vbe < 0.5) {
        /* Cutoff region */
        ib = 0;
        ic = 0;
        ie = 0;
    } else if (vce < 0.2) {
        /* Saturation region */
        ic = beta * 1e-3 * (vbe - 0.7);  /* Simplified */
        if (ic < 0) ic = 0;
        ib = ic / 10.0;  /* Forced beta in saturation */
        ie = ib + ic;
    } else {
        /* Active region: Ic = beta * Ib */
        /* Base current from B-E diode */
        double vt = 0.026;
        double is = 1e-14;
        ib = is * (exp(vbe / vt) - 1.0);
        if (ib < 0) ib = 0;
        if (ib > 0.1) ib = 0.1;  /* Limit current */
        
        ic = beta * ib;
        ie = ib + ic;
    }
    
    /* For PNP, reverse currents */
    if (isPNP) {
        ib = -ib;
        ic = -ic;
        ie = -ie;
    }
    
    /* Stamp currents as current sources */
    stamp_current_source(circuit, elem->nodes[1], elem->nodes[2], ib);  /* Base */
    stamp_current_source(circuit, elem->nodes[0], elem->nodes[2], ic);  /* Collector */
    
    /* Store currents */
    elem->params.trans.ib = ib;
    elem->params.trans.ic = ic;
    elem->params.trans.ie = ie;
    elem->params.trans.vbe = vbe;
    elem->current = ic;  /* Main current is collector current */
    
    /* Check convergence */
    double vbe_change = fabs(vbe - elem->params.trans.vbe);
    if (vbe_change > 0.01) {
        circuit->converged = false;
    }
}

Element* element_create_transistor_npn(int nc, int nb, int ne, double beta) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_TRANSISTOR_NPN;
    elem->nodes[0] = nc;  /* Collector */
    elem->nodes[1] = nb;  /* Base */
    elem->nodes[2] = ne;  /* Emitter */
    
    elem->params.trans.beta = (beta > 0) ? beta : 100.0;  /* Default beta = 100 */
    elem->params.trans.vbe = 0.0;
    elem->params.trans.ic = 0.0;
    elem->params.trans.ib = 0.0;
    
    elem->stamp = transistor_stamp;
    elem->startIteration = transistor_start_iteration;
    elem->doStep = transistor_do_step;
    
    return elem;
}

Element* element_create_transistor_pnp(int nc, int nb, int ne, double beta) {
    Element *elem = element_create_transistor_npn(nc, nb, ne, beta);
    if (elem) {
        elem->type = ELEM_TRANSISTOR_PNP;
    }
    return elem;
}

/* ========== SWITCH ========== */

static void switch_stamp(Element *elem, Circuit *circuit) {
    /* Switch is either very low resistance (closed) or very high (open) */
    double r = elem->params.sw.isOpen ? 1e12 : 1e-6;
    stamp_resistor(circuit, elem->nodes[0], elem->nodes[1], r);
}

static void switch_do_step(Element *elem, Circuit *circuit) {
    (void)circuit;  /* Unused */
    /* Calculate current based on state */
    double r = elem->params.sw.isOpen ? 1e12 : 1e-6;
    elem->current = (elem->volts[0] - elem->volts[1]) / r;
}

Element* element_create_switch(int n1, int n2, bool isOpen) {
    Element *elem = (Element*)calloc(1, sizeof(Element));
    if (!elem) return NULL;
    
    elem->type = ELEM_SWITCH;
    elem->nodes[0] = n1;
    elem->nodes[1] = n2;
    elem->nodes[2] = 0;
    
    elem->params.sw.isOpen = isOpen;
    
    elem->stamp = switch_stamp;
    elem->doStep = switch_do_step;
    
    return elem;
}

/* ========== COMMON ========== */

void element_destroy(Element *elem) {
    free(elem);
}
