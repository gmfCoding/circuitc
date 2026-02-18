/*
 * CircuitC - Circuit File Loader
 * Parses CircuitJS format .txt files and creates Circuit objects
 */

#define _POSIX_C_SOURCE 200809L
#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_TOKENS 32

/* Node mapping structure to convert (x,y) coordinates to node numbers */
typedef struct {
    int x, y;
    int nodeNumber;
} NodePoint;

typedef struct {
    NodePoint *points;
    int count;
    int capacity;
} NodeMap;

/* Create a new node map */
static NodeMap* nodemap_create(void) {
    NodeMap *map = (NodeMap*)malloc(sizeof(NodeMap));
    if (!map) return NULL;
    
    map->capacity = 100;
    map->count = 0;
    map->points = (NodePoint*)malloc(map->capacity * sizeof(NodePoint));
    if (!map->points) {
        free(map);
        return NULL;
    }
    return map;
}

/* Free node map */
static void nodemap_destroy(NodeMap *map) {
    if (map) {
        free(map->points);
        free(map);
    }
}

/* Get or create a node number for a given (x,y) coordinate */
static int nodemap_get_node(NodeMap *map, int x, int y) {
    /* Check if this coordinate already has a node number */
    for (int i = 0; i < map->count; i++) {
        if (map->points[i].x == x && map->points[i].y == y) {
            return map->points[i].nodeNumber;
        }
    }
    
    /* Create a new node number (starting from 1, 0 is reserved for ground) */
    if (map->count >= map->capacity) {
        map->capacity *= 2;
        NodePoint *new_points = (NodePoint*)realloc(map->points, map->capacity * sizeof(NodePoint));
        if (!new_points) return -1;
        map->points = new_points;
    }
    
    int nodeNum = map->count + 1;  /* Start from 1 */
    map->points[map->count].x = x;
    map->points[map->count].y = y;
    map->points[map->count].nodeNumber = nodeNum;
    map->count++;
    
    return nodeNum;
}

/* Tokenize a line into tokens */
static int tokenize_line(char *line, char **tokens, int max_tokens) {
    int count = 0;
    char *token = strtok(line, " \t\r\n");
    
    while (token != NULL && count < max_tokens) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    
    return count;
}

/* Parse simulation parameters from first line */
static bool parse_sim_params(const char *line, double *timeStep) {
    double version, maxTime, flags;
    int steps, flags2;
    
    /* Skip the '$' if present */
    const char *parse_line = line;
    if (*parse_line == '$') {
        parse_line++;
        while (*parse_line && isspace(*parse_line)) parse_line++;
    }
    
    /* Format: version timeStep maxTime flags steps flags2 */
    int result = sscanf(parse_line, "%lf %lf %lf %lf %d %d", 
                       &version, timeStep, &maxTime, &flags, &steps, &flags2);
    
    if (result >= 2) {
        /* Default time step if not specified */
        if (*timeStep <= 0) *timeStep = 5e-6;
        return true;
    }
    
    return false;
}

/* Validate element coordinates */
static bool validate_coordinates(int x1, int y1, int x2, int y2, const char *elem_type) {
    /* Check if coordinates are multiples of 8 */
    if (x1 % 8 != 0 || y1 % 8 != 0 || x2 % 8 != 0 || y2 % 8 != 0) {
        fprintf(stderr, "Warning: %s coordinates not aligned to 8-pixel grid: (%d,%d) to (%d,%d)\n", 
                elem_type, x1, y1, x2, y2);
        return false;
    }
    
    /* Calculate distance between endpoints */
    int dx = x2 - x1;
    int dy = y2 - y1;
    double distance = sqrt(dx * dx + dy * dy);
    
    /* Minimum component size is 16 pixels */
    if (distance < 16.0) {
        fprintf(stderr, "Warning: %s size too small (%.1f pixels): (%d,%d) to (%d,%d), minimum is 16\n",
                elem_type, distance, x1, y1, x2, y2);
        return false;
    }
    
    return true;
}

/* Parse a resistor element: r x1 y1 x2 y2 flags resistance */
static bool parse_resistor(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 7) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    double resistance = atof(tokens[6]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Resistor")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    if (resistance <= 0) resistance = 1.0;  /* Default resistance */
    
    Element *elem = element_create_resistor(n1, n2, resistance);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a capacitor element: c x1 y1 x2 y2 flags capacitance */
static bool parse_capacitor(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 7) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    double capacitance = atof(tokens[6]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Capacitor")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    if (capacitance <= 0) capacitance = 1e-9;  /* Default 1nF */
    
    Element *elem = element_create_capacitor(n1, n2, capacitance);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse an inductor element: l x1 y1 x2 y2 flags inductance */
static bool parse_inductor(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 7) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    double inductance = atof(tokens[6]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Inductor")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    if (inductance <= 0) inductance = 1e-6;  /* Default 1uH */
    
    Element *elem = element_create_inductor(n1, n2, inductance);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a voltage source: v x1 y1 x2 y2 flags waveform frequency voltage ... */
static bool parse_voltage_source(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 9) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    /* tokens[5] is flags, tokens[6] is waveform type, tokens[7] is frequency */
    double voltage = atof(tokens[8]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Voltage source")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    Element *elem = element_create_voltage_source(n1, n2, voltage);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a current source: i x1 y1 x2 y2 flags current */
static bool parse_current_source(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 7) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    double current = atof(tokens[6]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Current source")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    Element *elem = element_create_current_source(n1, n2, current);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a wire: w x1 y1 x2 y2 flags */
static bool parse_wire(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 5) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Wire")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    /* Wire is a resistor with very small resistance */
    Element *elem = element_create_resistor(n1, n2, 1e-6);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a ground element: g x y flags */
static bool parse_ground(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 3) return false;
    
    int x = atoi(tokens[1]);
    int y = atoi(tokens[2]);
    
    /* Get the node at this location */
    int node = nodemap_get_node(map, x, y);
    
    /* Connect this node to ground (node 0) with a very small resistance */
    Element *elem = element_create_resistor(node, 0, 1e-9);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a rail (voltage rail): R x y targetX targetY flags waveform voltage ... */
static bool parse_rail(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 8) return false;
    
    int x = atoi(tokens[1]);
    int y = atoi(tokens[2]);
    /* tokens[3] and [4] are target coordinates for display */
    /* tokens[5] is flags, tokens[6] is waveform type */
    double voltage = atof(tokens[7]);
    
    int node = nodemap_get_node(map, x, y);
    
    /* Rails are voltage sources from ground */
    Element *elem = element_create_voltage_source(0, node, voltage);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a diode element: d x1 y1 x2 y2 flags model */
static bool parse_diode(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 5) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    /* tokens[5] is flags, tokens[6] is model (optional) */
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Diode")) {
        return false;
    }
    
    int n1 = nodemap_get_node(map, x1, y1);  /* Anode */
    int n2 = nodemap_get_node(map, x2, y2);  /* Cathode */
    
    Element *elem = element_create_diode(n1, n2);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a transistor element: t x1 y1 x2 y2 flags [pnp] */
static bool parse_transistor(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 5) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    int flags = (token_count > 5) ? atoi(tokens[5]) : 0;
    
    /* In CircuitJS, transistors have collector, base, emitter nodes 
     * The coordinates given are typically base and emitter
     * We'll use coordinate system to map:
     * - First coord (x1,y1) = collector
     * - Second coord (x2,y2) = base
     * - We need to infer emitter from geometry or use a third coordinate
     * For simplicity, we'll create nodes based on the given coordinates
     * and assume standard layout
     */
    
    int nc = nodemap_get_node(map, x1, y1);      /* Collector */
    int nb = nodemap_get_node(map, (x1+x2)/2, (y1+y2)/2);  /* Base (middle) */
    int ne = nodemap_get_node(map, x2, y2);      /* Emitter */
    
    /* Check if PNP (flags & 1) or orientation flags */
    bool isPNP = (flags & 1);
    
    Element *elem;
    if (isPNP) {
        elem = element_create_transistor_pnp(nc, nb, ne, 100.0);
    } else {
        elem = element_create_transistor_npn(nc, nb, ne, 100.0);
    }
    
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a switch element: s x1 y1 x2 y2 flags [state] */
static bool parse_switch(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 5) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    
    /* Validate coordinates and size */
    if (!validate_coordinates(x1, y1, x2, y2, "Switch")) {
        return false;
    }
    
    /* Check if there's a state parameter (0=closed, 1=open) */
    bool isOpen = false;
    if (token_count > 6) {
        int state = atoi(tokens[6]);
        isOpen = (state != 0);
    }
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    Element *elem = element_create_switch(n1, n2, isOpen);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Parse a switch element (capital S): S x1 y1 x2 y2 flags state ... */
static bool parse_switch_S(Circuit *circuit, NodeMap *map, char **tokens, int token_count) {
    if (token_count < 7) return false;
    
    int x1 = atoi(tokens[1]);
    int y1 = atoi(tokens[2]);
    int x2 = atoi(tokens[3]);
    int y2 = atoi(tokens[4]);
    
    /* CircuitJS format: S has state in different position */
    /* Format appears to be: S x1 y1 x2 y2 flags state ... */
    int state = 0;
    if (token_count > 6) {
        /* tokens[6] might be "false" or "true" or "0"/"1" */
        if (strcmp(tokens[6], "true") == 0 || strcmp(tokens[6], "1") == 0) {
            state = 1;
        }
    }
    
    bool isOpen = (state == 0);  /* In CircuitJS, 0 = open, 1 = closed */
    
    int n1 = nodemap_get_node(map, x1, y1);
    int n2 = nodemap_get_node(map, x2, y2);
    
    Element *elem = element_create_switch(n1, n2, isOpen);
    if (elem) {
        circuit_add_element(circuit, elem);
        return true;
    }
    
    return false;
}

/* Load circuit from string content */
Circuit* circuit_load_from_string(const char *content) {
    if (!content) return NULL;
    
    Circuit *circuit = circuit_create();
    if (!circuit) return NULL;
    
    NodeMap *nodemap = nodemap_create();
    if (!nodemap) {
        circuit_destroy(circuit);
        return NULL;
    }
    
    /* Make a mutable copy of content */
    char *content_copy = strdup(content);
    if (!content_copy) {
        nodemap_destroy(nodemap);
        circuit_destroy(circuit);
        return NULL;
    }
    
    /* Process line by line */
    char *line_start = content_copy;
    bool first_line = true;
    int line_num = 0;
    
    while (*line_start) {
        line_num++;
        
        /* Find end of line */
        char *line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        
        /* Null terminate this line */
        char saved_char = *line_end;
        if (*line_end) {
            *line_end = '\0';
        }
        
        /* Skip leading whitespace */
        char *line = line_start;
        while (*line && isspace(*line)) line++;
        
        /* Process non-empty, non-comment lines */
        if (*line && *line != '#') {
            /* First non-comment line contains simulation parameters */
            if (first_line) {
                double timeStep = 5e-6;
                if (parse_sim_params(line, &timeStep)) {
                    set_time_step(circuit, timeStep);
                }
                first_line = false;
            } else {
                /* Parse element line */
                char line_copy[MAX_LINE_LENGTH];
                strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
                line_copy[MAX_LINE_LENGTH - 1] = '\0';
                
                char *tokens[MAX_TOKENS];
                int token_count = tokenize_line(line_copy, tokens, MAX_TOKENS);
                
                if (token_count > 0) {
                    char type = tokens[0][0];
                    
                    switch (type) {
                        case 'r':
                            parse_resistor(circuit, nodemap, tokens, token_count);
                            break;
                        case 'c':
                            parse_capacitor(circuit, nodemap, tokens, token_count);
                            break;
                        case 'l':
                            parse_inductor(circuit, nodemap, tokens, token_count);
                            break;
                        case 'v':
                            parse_voltage_source(circuit, nodemap, tokens, token_count);
                            break;
                        case 'i':
                            parse_current_source(circuit, nodemap, tokens, token_count);
                            break;
                        case 'w':
                            parse_wire(circuit, nodemap, tokens, token_count);
                            break;
                        case 'g':
                            parse_ground(circuit, nodemap, tokens, token_count);
                            break;
                        case 'R':
                            parse_rail(circuit, nodemap, tokens, token_count);
                            break;
                        case 'd':
                            parse_diode(circuit, nodemap, tokens, token_count);
                            break;
                        case 't':
                            parse_transistor(circuit, nodemap, tokens, token_count);
                            break;
                        case 's':
                            parse_switch(circuit, nodemap, tokens, token_count);
                            break;
                        case 'S':
                            parse_switch_S(circuit, nodemap, tokens, token_count);
                            break;
                        case 'O':
                            /* Output/probe - ignore for simulation */
                            break;
                        case 'o':
                            /* Options/scope - ignore */
                            break;
                        case 'L':
                            /* Label - ignore for simulation */
                            break;
                        case 'M':
                            /* MOSFET - not yet implemented */
                            fprintf(stderr, "Warning: MOSFET elements not yet supported (line %d)\n", line_num);
                            break;
                        case 'j':
                            /* JFET - not yet implemented */
                            fprintf(stderr, "Warning: JFET elements not yet supported (line %d)\n", line_num);
                            break;
                        default:
                            if (isdigit(type)) {
                                /* Numbered components (custom, subcircuits) - not yet implemented */
                                fprintf(stderr, "Warning: Custom component %s not yet supported (line %d)\n", 
                                       tokens[0], line_num);
                            } else {
                                fprintf(stderr, "Warning: Unknown element type '%c' (line %d)\n", type, line_num);
                            }
                            break;
                    }
                }
            }
        }
        
        /* Move to next line */
        if (saved_char) {
            line_start = line_end + 1;
            /* Skip \r\n sequences */
            if (saved_char == '\r' && *line_start == '\n') {
                line_start++;
            }
        } else {
            break;
        }
    }
    
    free(content_copy);
    
    /* Update the node count based on the nodemap */
    if (nodemap->count > circuit->nodeCount) {
        circuit->nodeCount = nodemap->count;
    }
    
    nodemap_destroy(nodemap);
    
    printf("Loaded circuit with %d elements and %d nodes\n", 
           circuit->elementCount, circuit->nodeCount);
    
    return circuit;
}

/* Load circuit from file */
Circuit* circuit_load_from_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fprintf(stderr, "Error: Empty or invalid file %s\n", filename);
        fclose(file);
        return NULL;
    }
    
    /* Read entire file into memory */
    char *content = (char*)malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Error: Cannot allocate memory for file %s\n", filename);
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);
    
    /* Parse the content */
    Circuit *circuit = circuit_load_from_string(content);
    free(content);
    
    return circuit;
}
