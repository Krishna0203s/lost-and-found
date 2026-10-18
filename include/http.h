#ifndef HTTP_H
#define HTTP_H

/* Configuration for the HTTP server */
typedef struct {
    int port;
    const char* web_root;
    const char* db_file;
} HttpServerConfig;

/* Start listening on configured port and dispatch worker threads */
int http_server_start(const HttpServerConfig* config);

/* Gracefully signal the HTTP server to stop */
void http_server_stop(void);

#endif /* HTTP_H */
