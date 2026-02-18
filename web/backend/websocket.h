/*
 * WebSocket Server for Circuit Simulator
 * Raw WebSocket implementation with binary protocol
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include "../../src/circuit.h"

/* WebSocket opcodes */
#define WS_OPCODE_CONTINUE 0x0
#define WS_OPCODE_TEXT     0x1
#define WS_OPCODE_BINARY   0x2
#define WS_OPCODE_CLOSE    0x8
#define WS_OPCODE_PING     0x9
#define WS_OPCODE_PONG     0xA

/* Binary message types */
#define MSG_CIRCUIT_DATA   0x01
#define MSG_SIM_UPDATE     0x02
#define MSG_CONTROL        0x03
#define MSG_LOAD_FILE      0x04
#define MSG_ERROR          0xFF

/* Control commands */
#define CMD_START          0x01
#define CMD_STOP           0x02
#define CMD_STEP           0x03
#define CMD_RESET          0x04
#define CMD_SET_SPEED      0x05
#define CMD_SET_CURRENT_VIS 0x06

/* WebSocket frame structure */
typedef struct {
    uint8_t fin;
    uint8_t opcode;
    uint8_t mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    uint8_t *payload;
} ws_frame_t;

/* Client connection */
typedef struct {
    int socket;
    bool handshake_done;
    Circuit *circuit;
    bool running;
    double speed_multiplier;
    double current_vis_factor;
    int step_count;
    char circuit_file[256];
} client_t;

/* WebSocket functions */
int ws_server_start(int port);
bool ws_handshake(int client_socket);
bool ws_read_frame(int socket, ws_frame_t *frame);
bool ws_send_frame(int socket, uint8_t opcode, const uint8_t *payload, uint64_t length);
bool ws_send_binary(int socket, const uint8_t *data, uint64_t length);
void ws_frame_free(ws_frame_t *frame);

/* Request type detection */
bool is_websocket_request(const char *buffer, size_t length);
bool handle_http_or_websocket(int client_socket);

/* Client handling */
void client_init(client_t *client, int socket);
void client_destroy(client_t *client);
void handle_client_message(client_t *client, const uint8_t *data, uint64_t length);
void send_circuit_data(client_t *client);
void send_sim_update(client_t *client);

/* Binary protocol helpers */
void pack_uint16(uint8_t *buffer, uint16_t value);
void pack_uint32(uint8_t *buffer, uint32_t value);
void pack_float(uint8_t *buffer, float value);
void pack_double(uint8_t *buffer, double value);
uint16_t unpack_uint16(const uint8_t *buffer);
uint32_t unpack_uint32(const uint8_t *buffer);
float unpack_float(const uint8_t *buffer);
double unpack_double(const uint8_t *buffer);

#endif /* WEBSOCKET_H */
