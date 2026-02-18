/*
 * WebSocket Server Implementation
 * Raw WebSocket protocol with binary messaging
 */

#define _POSIX_C_SOURCE 200809L
#include "websocket.h"
#include "http_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <ctype.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* Base64 encoding for WebSocket handshake */
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const unsigned char *input, int length) {
    int output_length = 4 * ((length + 2) / 3);
    char *output = malloc(output_length + 1);
    if (!output) return NULL;
    
    int i, j;
    for (i = 0, j = 0; i < length;) {
        uint32_t octet_a = i < length ? input[i++] : 0;
        uint32_t octet_b = i < length ? input[i++] : 0;
        uint32_t octet_c = i < length ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
    
    int padding = (3 - (length % 3)) % 3;
    for (i = 0; i < padding; i++)
        output[output_length - 1 - i] = '=';
    
    output[output_length] = '\0';
    return output;
}

/* Start WebSocket server */
int ws_server_start(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }
    
    printf("WebSocket server started on port %d\n", port);
    return server_socket;
}

/* Check if request is a WebSocket upgrade */
bool is_websocket_request(const char *buffer, size_t length) {
    /* Look for "Upgrade: websocket" header */
    if (length < 20) return false;
    
    /* Convert to lowercase for case-insensitive search */
    char *lower = malloc(length + 1);
    if (!lower) return false;
    
    for (size_t i = 0; i < length; i++) {
        lower[i] = tolower(buffer[i]);
    }
    lower[length] = '\0';
    
    bool is_ws = (strstr(lower, "upgrade: websocket") != NULL);
    free(lower);
    
    return is_ws;
}

/* Parse HTTP request path */
static char* extract_request_path(const char *buffer) {
    /* Format: GET /path/to/file HTTP/1.1 */
    const char *get_start = strstr(buffer, "GET ");
    if (!get_start) return NULL;
    
    get_start += 4; /* Skip "GET " */
    const char *path_end = strchr(get_start, ' ');
    if (!path_end) return NULL;
    
    int path_len = path_end - get_start;
    char *path = malloc(path_len + 1);
    if (!path) return NULL;
    
    strncpy(path, get_start, path_len);
    path[path_len] = '\0';
    
    return path;
}

/* Handle HTTP or WebSocket request */
bool handle_http_or_websocket(int client_socket) {
    char buffer[4096];
    
    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* Peek at the request to determine type */
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, MSG_PEEK);
    if (bytes_read <= 0) {
        return false;
    }
    
    buffer[bytes_read] = '\0';
    
    /* Check if it's a WebSocket upgrade request */
    if (is_websocket_request(buffer, bytes_read)) {
        printf("WebSocket upgrade request detected\n");
        /* Now consume the data for real */
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) return false;
        buffer[bytes_read] = '\0';
        
        /* Process WebSocket handshake */
        char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
        if (!key_start) return false;
        
        key_start += 19;
        char *key_end = strstr(key_start, "\r\n");
        if (!key_end) return false;
        
        int key_len = key_end - key_start;
        char key[256];
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        /* Compute accept key */
        char accept_key_input[512];
        snprintf(accept_key_input, sizeof(accept_key_input), "%s%s", key, WS_GUID);
        
        unsigned char sha1_result[SHA_DIGEST_LENGTH];
        SHA1((unsigned char*)accept_key_input, strlen(accept_key_input), sha1_result);
        
        char *accept_key = base64_encode(sha1_result, SHA_DIGEST_LENGTH);
        if (!accept_key) return false;
        
        /* Send handshake response */
        char response[512];
        int response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n",
            accept_key);
        
        free(accept_key);
        
        ssize_t sent = send(client_socket, response, response_len, 0);
        return (sent > 0);
    } else {
        /* HTTP request - extract path and serve static file */
        printf("HTTP request detected\n");
        /* Consume the data */
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) return false;
        buffer[bytes_read] = '\0';
        
        char *path = extract_request_path(buffer);
        if (!path) {
            const char *error_msg = "400 Bad Request";
            http_send_response(client_socket, 400, "text/plain", error_msg, strlen(error_msg));
            return false;
        }
        
        printf("HTTP GET %s\n", path);
        bool success = http_serve_file(client_socket, path);
        free(path);
        return false; /* Close connection after serving HTTP */
    }
}

/* WebSocket handshake with buffered data */
bool ws_handshake(int client_socket) {
    char buffer[4096];
    
    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        fprintf(stderr, "Failed to read handshake request (bytes_read=%zd)\n", bytes_read);
        return false;
    }
    
    buffer[bytes_read] = '\0';
    
    /* Debug: print request */
    printf("Received %zd bytes for handshake:\n", bytes_read);
    printf("===START===\n%s\n===END===\n", buffer);
    
    /* Debug: print hex dump of first 100 bytes */
    printf("Hex dump: ");
    for (ssize_t i = 0; i < bytes_read && i < 100; i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
    
    /* Extract Sec-WebSocket-Key */
    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        fprintf(stderr, "No Sec-WebSocket-Key found in request\n");
        return false;
    }
    
    key_start += 19;
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        fprintf(stderr, "Malformed Sec-WebSocket-Key\n");
        return false;
    }
    
    int key_len = key_end - key_start;
    char key[256];
    strncpy(key, key_start, key_len);
    key[key_len] = '\0';
    
    printf("WebSocket Key: %s\n", key);
    
    /* Compute accept key */
    char accept_key_input[512];
    snprintf(accept_key_input, sizeof(accept_key_input), "%s%s", key, WS_GUID);
    
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)accept_key_input, strlen(accept_key_input), sha1_result);
    
    char *accept_key = base64_encode(sha1_result, SHA_DIGEST_LENGTH);
    if (!accept_key) {
        fprintf(stderr, "Failed to encode accept key\n");
        return false;
    }
    
    printf("Accept Key: %s\n", accept_key);
    
    /* Send handshake response */
    char response[512];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);
    
    free(accept_key);
    
    ssize_t sent = send(client_socket, response, response_len, 0);
    if (sent <= 0) {
        fprintf(stderr, "Failed to send handshake response\n");
        return false;
    }
    
    return true;
}

/* Read WebSocket frame */
bool ws_read_frame(int socket, ws_frame_t *frame) {
    uint8_t header[2];
    if (recv(socket, header, 2, 0) != 2) return false;
    
    frame->fin = (header[0] >> 7) & 0x1;
    frame->opcode = header[0] & 0x0F;
    frame->mask = (header[1] >> 7) & 0x1;
    frame->payload_length = header[1] & 0x7F;
    
    /* Extended payload length */
    if (frame->payload_length == 126) {
        uint8_t len[2];
        if (recv(socket, len, 2, 0) != 2) return false;
        frame->payload_length = (len[0] << 8) | len[1];
    } else if (frame->payload_length == 127) {
        uint8_t len[8];
        if (recv(socket, len, 8, 0) != 8) return false;
        frame->payload_length = 0;
        for (int i = 0; i < 8; i++)
            frame->payload_length = (frame->payload_length << 8) | len[i];
    }
    
    /* Masking key */
    if (frame->mask) {
        if (recv(socket, frame->masking_key, 4, 0) != 4) return false;
    }
    
    /* Payload */
    if (frame->payload_length > 0) {
        frame->payload = malloc(frame->payload_length);
        if (!frame->payload) return false;
        
        ssize_t total = 0;
        while (total < (ssize_t)frame->payload_length) {
            ssize_t n = recv(socket, frame->payload + total, 
                           frame->payload_length - total, 0);
            if (n <= 0) {
                free(frame->payload);
                return false;
            }
            total += n;
        }
        
        /* Unmask payload */
        if (frame->mask) {
            for (uint64_t i = 0; i < frame->payload_length; i++)
                frame->payload[i] ^= frame->masking_key[i % 4];
        }
    } else {
        frame->payload = NULL;
    }
    
    return true;
}

/* Send WebSocket frame */
bool ws_send_frame(int socket, uint8_t opcode, const uint8_t *payload, uint64_t length) {
    uint8_t header[10];
    int header_len = 2;
    
    header[0] = 0x80 | (opcode & 0x0F);  /* FIN + opcode */
    
    if (length < 126) {
        header[1] = length;
    } else if (length < 65536) {
        header[1] = 126;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++)
            header[2 + i] = (length >> (56 - i * 8)) & 0xFF;
        header_len = 10;
    }
    
    if (send(socket, header, header_len, 0) != header_len) return false;
    if (length > 0 && send(socket, payload, length, 0) != (ssize_t)length) return false;
    
    return true;
}

/* Send binary message */
bool ws_send_binary(int socket, const uint8_t *data, uint64_t length) {
    return ws_send_frame(socket, WS_OPCODE_BINARY, data, length);
}

/* Free frame */
void ws_frame_free(ws_frame_t *frame) {
    if (frame && frame->payload) {
        free(frame->payload);
        frame->payload = NULL;
    }
}

/* Binary packing helpers */
void pack_uint16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (value >> 8) & 0xFF;
    buffer[1] = value & 0xFF;
}

void pack_uint32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
}

void pack_float(uint8_t *buffer, float value) {
    uint32_t temp;
    memcpy(&temp, &value, sizeof(float));
    pack_uint32(buffer, temp);
}

void pack_double(uint8_t *buffer, double value) {
    uint64_t temp;
    memcpy(&temp, &value, sizeof(double));
    for (int i = 0; i < 8; i++)
        buffer[i] = (temp >> (56 - i * 8)) & 0xFF;
}

uint16_t unpack_uint16(const uint8_t *buffer) {
    return (buffer[0] << 8) | buffer[1];
}

uint32_t unpack_uint32(const uint8_t *buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

float unpack_float(const uint8_t *buffer) {
    uint32_t temp = unpack_uint32(buffer);
    float value;
    memcpy(&value, &temp, sizeof(float));
    return value;
}

double unpack_double(const uint8_t *buffer) {
    uint64_t temp = 0;
    for (int i = 0; i < 8; i++)
        temp = (temp << 8) | buffer[i];
    double value;
    memcpy(&value, &temp, sizeof(double));
    return value;
}

/* Initialize client */
void client_init(client_t *client, int socket) {
    client->socket = socket;
    client->handshake_done = false;
    client->circuit = NULL;
    client->running = false;
    client->speed_multiplier = 1.0;
    client->current_vis_factor = 1.0;
    client->step_count = 0;
    client->circuit_file[0] = '\0';
}

/* Destroy client */
void client_destroy(client_t *client) {
    if (client->circuit) {
        circuit_destroy(client->circuit);
        client->circuit = NULL;
    }
    if (client->socket >= 0) {
        close(client->socket);
        client->socket = -1;
    }
}
