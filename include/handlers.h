#ifndef HANDLERS_H
#define HANDLERS_H

#include <stddef.h>

/* Simplified representation of an incoming HTTP request */
typedef struct {
    char method[16];
    char path[256];
    char query[512];
    char auth_header[256];
    char* body;
    size_t body_len;
} HttpRequest;

/* Simplified representation of an outgoing HTTP response */
typedef struct {
    int status_code;
    char content_type[64];
    char* body;
    size_t body_len;
} HttpResponse;

/* Free any dynamically allocated memory inside HttpResponse */
void http_response_free(HttpResponse* res);

/* Main API router delegating incoming request to appropriate controller */
void handle_api_request(const HttpRequest* req, HttpResponse* res);

#endif /* HANDLERS_H */
