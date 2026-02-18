/*
 * Circuit Server - WebSocket backend for circuit simulator
 */

#define _POSIX_C_SOURCE 200809L
#include "websocket.h"
#include "../../src/circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* Send circuit topology and component data */
void send_circuit_data(client_t *client) {
    if (!client->circuit) return;
    
    Circuit *c = client->circuit;
    
    /* Calculate message size */
    /* Header: 1 byte type + 2 bytes element count + 2 bytes node count */
    size_t msg_size = 1 + 2 + 2;
    
    /* Each element: 1 type + 3 nodes (2 bytes each) + 8 value + 8 current + 3*8 volts */
    msg_size += c->elementCount * (1 + 3*2 + 8 + 8 + 3*8);
    
    uint8_t *buffer = malloc(msg_size);
    if (!buffer) return;
    
    size_t offset = 0;
    
    /* Message type */
    buffer[offset++] = MSG_CIRCUIT_DATA;
    
    /* Element and node counts */
    pack_uint16(buffer + offset, c->elementCount);
    offset += 2;
    pack_uint16(buffer + offset, c->nodeCount);
    offset += 2;
    
    /* Element data */
    for (int i = 0; i < c->elementCount; i++) {
        Element *elem = c->elements[i];
        
        buffer[offset++] = (uint8_t)elem->type;
        
        /* Nodes */
        pack_uint16(buffer + offset, elem->nodes[0]);
        offset += 2;
        pack_uint16(buffer + offset, elem->nodes[1]);
        offset += 2;
        pack_uint16(buffer + offset, elem->nodes[2]);
        offset += 2;
        
        /* Value */
        pack_double(buffer + offset, elem->value);
        offset += 8;
        
        /* Current */
        pack_double(buffer + offset, elem->current);
        offset += 8;
        
        /* Voltages */
        pack_double(buffer + offset, elem->volts[0]);
        offset += 8;
        pack_double(buffer + offset, elem->volts[1]);
        offset += 8;
        pack_double(buffer + offset, elem->volts[2]);
        offset += 8;
    }
    
    ws_send_binary(client->socket, buffer, offset);
    free(buffer);
}

/* Send simulation update */
void send_sim_update(client_t *client) {
    if (!client->circuit) return;
    
    Circuit *c = client->circuit;
    
    /* Header: 1 type + 8 time + 4 step + element count updates */
    size_t msg_size = 1 + 8 + 4 + c->elementCount * (8 + 3*8);
    
    uint8_t *buffer = malloc(msg_size);
    if (!buffer) return;
    
    size_t offset = 0;
    
    buffer[offset++] = MSG_SIM_UPDATE;
    
    /* Simulation time */
    pack_double(buffer + offset, c->time);
    offset += 8;
    
    /* Step count */
    pack_uint32(buffer + offset, client->step_count);
    offset += 4;
    
    /* Element updates (current and voltages) */
    for (int i = 0; i < c->elementCount; i++) {
        Element *elem = c->elements[i];
        
        pack_double(buffer + offset, elem->current);
        offset += 8;
        
        pack_double(buffer + offset, elem->volts[0]);
        offset += 8;
        pack_double(buffer + offset, elem->volts[1]);
        offset += 8;
        pack_double(buffer + offset, elem->volts[2]);
        offset += 8;
    }
    
    ws_send_binary(client->socket, buffer, offset);
    free(buffer);
}

/* Handle client messages */
void handle_client_message(client_t *client, const uint8_t *data, uint64_t length) {
    if (length < 1) return;
    
    uint8_t msg_type = data[0];
    
    switch (msg_type) {
        case MSG_LOAD_FILE: {
            /* Format: 1 byte type + filename string */
            if (length < 2) break;
            
            size_t filename_len = length - 1;
            if (filename_len >= sizeof(client->circuit_file)) {
                filename_len = sizeof(client->circuit_file) - 1;
            }
            
            memcpy(client->circuit_file, data + 1, filename_len);
            client->circuit_file[filename_len] = '\0';
            
            /* Load circuit */
            if (client->circuit) {
                circuit_destroy(client->circuit);
            }
            
            printf("Loading circuit: %s\n", client->circuit_file);
            client->circuit = circuit_load_from_file(client->circuit_file);
            
            if (client->circuit) {
                circuit_analyze(client->circuit);
                send_circuit_data(client);
                printf("Circuit loaded: %d elements, %d nodes\n", 
                       client->circuit->elementCount, client->circuit->nodeCount);
            } else {
                /* Send error */
                uint8_t error[] = {MSG_ERROR, 0x01};  /* 0x01 = load error */
                ws_send_binary(client->socket, error, sizeof(error));
            }
            break;
        }
        
        case MSG_CONTROL: {
            /* Format: 1 byte type + 1 byte command + optional data */
            if (length < 2) break;
            
            uint8_t cmd = data[1];
            
            switch (cmd) {
                case CMD_START:
                    client->running = true;
                    printf("Simulation started\n");
                    break;
                    
                case CMD_STOP:
                    client->running = false;
                    printf("Simulation stopped\n");
                    break;
                    
                case CMD_STEP:
                    if (client->circuit) {
                        circuit_step(client->circuit);
                        client->step_count++;
                        send_sim_update(client);
                    }
                    break;
                    
                case CMD_RESET:
                    if (client->circuit) {
                        circuit_reset(client->circuit);
                        client->step_count = 0;
                        send_circuit_data(client);
                    }
                    printf("Simulation reset\n");
                    break;
                    
                case CMD_SET_SPEED:
                    if (length >= 6) {
                        client->speed_multiplier = unpack_float(data + 2);
                        printf("Speed set to %.2fx\n", client->speed_multiplier);
                    }
                    break;
                    
                case CMD_SET_CURRENT_VIS:
                    if (length >= 6) {
                        client->current_vis_factor = unpack_float(data + 2);
                        printf("Current visualization factor: %.2f\n", client->current_vis_factor);
                    }
                    break;
            }
            break;
        }
    }
}

/* Simulation thread */
void* simulation_thread(void *arg) {
    client_t *client = (client_t*)arg;
    
    struct timeval last_update;
    gettimeofday(&last_update, NULL);
    
    while (client->socket >= 0) {
        if (client->running && client->circuit) {
            /* Run simulation step */
            if (circuit_step(client->circuit)) {
                client->step_count++;
                
                /* Send update at display rate (adjustable by speed) */
                struct timeval now;
                gettimeofday(&now, NULL);
                
                double elapsed = (now.tv_sec - last_update.tv_sec) + 
                               (now.tv_usec - last_update.tv_usec) / 1000000.0;
                
                /* Update rate: base 60 Hz / speed_multiplier */
                double update_interval = 1.0 / (60.0 * client->speed_multiplier);
                
                if (elapsed >= update_interval) {
                    send_sim_update(client);
                    last_update = now;
                }
            } else {
                /* Simulation error */
                client->running = false;
                uint8_t error[] = {MSG_ERROR, 0x02};  /* 0x02 = sim error */
                ws_send_binary(client->socket, error, sizeof(error));
            }
            
            /* Small delay to prevent CPU spinning */
            usleep(100);
        } else {
            /* Idle */
            usleep(10000);  /* 10ms */
        }
    }
    
    return NULL;
}

/* Main server */
int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    int server_socket = ws_server_start(port);
    if (server_socket < 0) {
        return 1;
    }
    
    printf("Waiting for WebSocket connections...\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* WebSocket handshake */
        if (!ws_handshake(client_socket)) {
            fprintf(stderr, "WebSocket handshake failed\n");
            close(client_socket);
            continue;
        }
        
        printf("WebSocket handshake successful\n");
        
        /* Initialize client */
        client_t client;
        client_init(&client, client_socket);
        client.handshake_done = true;
        
        /* Start simulation thread */
        pthread_t sim_thread;
        pthread_create(&sim_thread, NULL, simulation_thread, &client);
        
        /* Handle incoming messages */
        fd_set readfds;
        struct timeval timeout;
        time_t last_ping = time(NULL);
        
        while (1) {
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);
            
            /* Timeout after 1 second to check for ping */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int activity = select(client_socket + 1, &readfds, NULL, NULL, &timeout);
            
            if (activity < 0) {
                perror("select error");
                break;
            }
            
            /* Send ping every 30 seconds */
            time_t now = time(NULL);
            if (now - last_ping >= 30) {
                printf("Sending keepalive ping\n");
                uint8_t ping_data = 0;
                if (!ws_send_frame(client_socket, WS_OPCODE_PING, &ping_data, 1)) {
                    printf("Failed to send ping, disconnecting\n");
                    break;
                }
                last_ping = now;
            }
            
            /* Check if data available to read */
            if (activity > 0 && FD_ISSET(client_socket, &readfds)) {
                ws_frame_t frame;
                if (!ws_read_frame(client_socket, &frame)) {
                    printf("Client disconnected\n");
                    break;
                }
                
                if (frame.opcode == WS_OPCODE_BINARY) {
                    handle_client_message(&client, frame.payload, frame.payload_length);
                } else if (frame.opcode == WS_OPCODE_CLOSE) {
                    printf("Close frame received\n");
                    ws_frame_free(&frame);
                    break;
                } else if (frame.opcode == WS_OPCODE_PING) {
                    ws_send_frame(client_socket, WS_OPCODE_PONG, frame.payload, frame.payload_length);
                } else if (frame.opcode == WS_OPCODE_PONG) {
                    printf("Pong received\n");
                }
                
                ws_frame_free(&frame);
            }
        }
        
        /* Cleanup */
        client_destroy(&client);
        pthread_cancel(sim_thread);
        pthread_join(sim_thread, NULL);
    }
    
    close(server_socket);
    return 0;
}
