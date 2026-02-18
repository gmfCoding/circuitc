/*
 * HTTP Utilities - Static file serving implementation
 */

#define _POSIX_C_SOURCE 200809L
#include "http_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>

/* Get MIME type from file extension */
const char* http_get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    
    return "application/octet-stream";
}

/* Send HTTP response */
void http_send_response(int client_socket, int status_code, const char *content_type,
                       const char *body, size_t body_length) {
    const char *status_text = "OK";
    if (status_code == 404) status_text = "Not Found";
    else if (status_code == 500) status_text = "Internal Server Error";
    else if (status_code == 400) status_text = "Bad Request";
    
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        status_code, status_text, content_type, body_length);
    
    send(client_socket, header, header_len, 0);
    if (body && body_length > 0) {
        send(client_socket, body, body_length, 0);
    }
}

/* Serve a static file over HTTP */
bool http_serve_file(int client_socket, const char *request_path) {
    /* Normalize the path */
    char file_path[512];
    
    /* Default to index.html if root requested */
    if (strcmp(request_path, "/") == 0) {
        request_path = "/index.html";
    }
    
    /* Build full path to frontend directory */
    snprintf(file_path, sizeof(file_path), "../frontend%s", request_path);
    
    /* Security: prevent directory traversal */
    if (strstr(file_path, "..") != NULL && strstr(request_path, "..") != NULL) {
        const char *error_msg = "403 Forbidden";
        http_send_response(client_socket, 403, "text/plain", error_msg, strlen(error_msg));
        return false;
    }
    
    /* Open and read the file */
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        printf("File not found: %s\n", file_path);
        const char *error_msg = "404 Not Found";
        http_send_response(client_socket, 404, "text/plain", error_msg, strlen(error_msg));
        return false;
    }
    
    /* Get file size */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        const char *error_msg = "500 Internal Server Error";
        http_send_response(client_socket, 500, "text/plain", error_msg, strlen(error_msg));
        return false;
    }
    
    size_t file_size = st.st_size;
    
    /* Read file content */
    char *content = malloc(file_size);
    if (!content) {
        close(fd);
        const char *error_msg = "500 Internal Server Error";
        http_send_response(client_socket, 500, "text/plain", error_msg, strlen(error_msg));
        return false;
    }
    
    ssize_t bytes_read = read(fd, content, file_size);
    close(fd);
    
    if (bytes_read != (ssize_t)file_size) {
        free(content);
        const char *error_msg = "500 Internal Server Error";
        http_send_response(client_socket, 500, "text/plain", error_msg, strlen(error_msg));
        return false;
    }
    
    /* Send response */
    const char *mime_type = http_get_mime_type(file_path);
    http_send_response(client_socket, 200, mime_type, content, file_size);
    
    free(content);
    printf("Served: %s (%zu bytes, %s)\n", file_path, file_size, mime_type);
    return true;
}
