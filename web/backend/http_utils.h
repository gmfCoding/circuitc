/*
 * HTTP Utilities - Static file serving
 */

#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* Serve a static file over HTTP */
bool http_serve_file(int client_socket, const char *request_path);

/* Send HTTP response */
void http_send_response(int client_socket, int status_code, const char *content_type, 
                       const char *body, size_t body_length);

/* Get MIME type from file extension */
const char* http_get_mime_type(const char *path);

#endif /* HTTP_UTILS_H */
