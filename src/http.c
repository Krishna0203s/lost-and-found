#define _GNU_SOURCE
#include "http.h"
#include "handlers.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <io.h>
  #define close_socket(s) closesocket(s)
  #define MSG_NOSIGNAL 0
  #define SHUT_RDWR SD_BOTH
  typedef int socklen_t;
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <signal.h>
  #define close_socket(s) close(s)
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

static char* portable_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return (char*)haystack;
    for (; *haystack; haystack++) {
#ifdef _WIN32
        if (_strnicmp(haystack, needle, nlen) == 0) return (char*)haystack;
#else
        if (strncasecmp(haystack, needle, nlen) == 0) return (char*)haystack;
#endif
    }
    return NULL;
}
#define strcasestr portable_strcasestr

#define BUFFER_SIZE 8192
#define MAX_HEADER_SIZE 16384

static volatile int g_running = 1;
static int g_server_fd = -1;
static char g_web_root[512] = "web";

typedef struct {
    int client_fd;
} ClientContext;

static const char* get_mime_type(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(dot, ".css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcasecmp(dot, ".json") == 0) return "application/json; charset=utf-8";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".gif") == 0) return "image/gif";
    if (strcasecmp(dot, ".svg") == 0) return "image/svg+xml";
    if (strcasecmp(dot, ".ico") == 0) return "image/x-icon";
    if (strcasecmp(dot, ".webp") == 0) return "image/webp";
    if (strcasecmp(dot, ".woff2") == 0) return "font/woff2";
    if (strcasecmp(dot, ".woff") == 0) return "font/woff";
    if (strcasecmp(dot, ".txt") == 0) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static void send_http_response(int fd, int status_code, const char* status_text,
                               const char* content_type, const char* body, size_t body_len) {
    char header_buf[1024];
    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PATCH, PUT, DELETE, OPTIONS, HEAD\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    send(fd, header_buf, header_len, MSG_NOSIGNAL);
    if (body && body_len > 0) {
        send(fd, body, body_len, MSG_NOSIGNAL);
    }
}

static void serve_static_file(int fd, const char* path) {
    /* Prevent directory traversal */
    if (strstr(path, "..")) {
        send_http_response(fd, 403, "Forbidden", "text/plain", "Forbidden", 9);
        return;
    }

    char file_path[1024];
    const char* rel_path = path;
    if (rel_path[0] == '/') rel_path++;
    if (strlen(rel_path) == 0) rel_path = "login.html";

    snprintf(file_path, sizeof(file_path), "%s/%s", g_web_root, rel_path);

    struct stat st;
    if (stat(file_path, &st) != 0 || S_ISDIR(st.st_mode)) {
        /* If directory, try login.html or index.html inside */
        if (S_ISDIR(st.st_mode)) {
            snprintf(file_path, sizeof(file_path), "%s/%s/login.html", g_web_root, rel_path);
            if (stat(file_path, &st) != 0) {
                snprintf(file_path, sizeof(file_path), "%s/%s/index.html", g_web_root, rel_path);
                if (stat(file_path, &st) != 0) {
                    send_http_response(fd, 404, "Not Found", "text/plain", "Not Found", 9);
                    return;
                }
            }
        } else {
            send_http_response(fd, 404, "Not Found", "text/plain", "Not Found", 9);
            return;
        }
    }

    FILE* f = fopen(file_path, "rb");
    if (!f) {
        send_http_response(fd, 500, "Internal Server Error", "text/plain", "Error opening file", 18);
        return;
    }

    size_t file_size = (size_t)st.st_size;
    const char* mime = get_mime_type(file_path);

    char header_buf[1024];
    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime, file_size);

    if (send(fd, header_buf, header_len, MSG_NOSIGNAL) > 0) {
        char chunk[8192];
        size_t bytes_read;
        while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
            if (send(fd, chunk, bytes_read, MSG_NOSIGNAL) <= 0) {
                break;
            }
        }
    }

    fclose(f);
}

static void* handle_client(void* arg) {
    ClientContext* ctx = (ClientContext*)arg;
    int fd = ctx->client_fd;
    free(ctx);

    char buffer[MAX_HEADER_SIZE];
    ssize_t total_received = 0;
    ssize_t n;

    /* Read HTTP headers */
    while (total_received < (ssize_t)sizeof(buffer) - 1) {
        n = recv(fd, buffer + total_received, sizeof(buffer) - 1 - total_received, 0);
        if (n <= 0) break;
        total_received += n;
        buffer[total_received] = '\0';
        if (strstr(buffer, "\r\n\r\n")) break;
    }

    if (total_received <= 0) {
        close_socket(fd);
        return NULL;
    }

    char* header_end = strstr(buffer, "\r\n\r\n");
    if (!header_end) {
        close_socket(fd);
        return NULL;
    }

    size_t header_len = (size_t)(header_end - buffer) + 4;
    size_t body_in_buffer = (size_t)total_received - header_len;

    /* Parse request line */
    char method[16] = {0};
    char full_path[512] = {0};
    char version[16] = {0};
    sscanf(buffer, "%15s %511s %15s", method, full_path, version);

    /* Separate path and query */
    char path[256] = {0};
    char query[512] = {0};
    char* qmark = strchr(full_path, '?');
    if (qmark) {
        size_t plen = (size_t)(qmark - full_path);
        if (plen >= sizeof(path)) plen = sizeof(path) - 1;
        memcpy(path, full_path, plen);
        path[plen] = '\0';
        snprintf(query, sizeof(query), "%s", qmark + 1);
    } else {
        snprintf(path, sizeof(path), "%s", full_path);
    }

    /* Handle CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(fd, 204, "No Content", "text/plain", NULL, 0);
        close_socket(fd);
        return NULL;
    }

    /* Check Content-Length */
    size_t content_length = 0;
    char* cl_header = strcasestr(buffer, "Content-Length:");
    if (cl_header) {
        content_length = (size_t)strtoul(cl_header + 15, NULL, 10);
    }

    if (content_length > 5 * 1024 * 1024) {
        send_http_response(fd, 413, "Payload Too Large", "text/plain", "Payload exceeds 5MB limit", 25);
        close_socket(fd);
        return NULL;
    }

    /* Read remaining body if any */
    char* body = NULL;
    if (content_length > 0) {
        body = malloc(content_length + 1);
        if (body) {
            size_t copied = 0;
            if (body_in_buffer > 0) {
                copied = (body_in_buffer < content_length) ? body_in_buffer : content_length;
                memcpy(body, header_end + 4, copied);
            }
            while (copied < content_length) {
                ssize_t r = recv(fd, body + copied, content_length - copied, 0);
                if (r <= 0) break;
                copied += r;
            }
            body[copied] = '\0';
        }
    }

    /* Check Authorization */
    char auth_header[256] = {0};
    char* auth_line = strcasestr(buffer, "Authorization:");
    if (auth_line) {
        const char* p = auth_line + 14;
        while (*p == ' ' || *p == '\t') p++;
        const char* end = strstr(p, "\r\n");
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len >= sizeof(auth_header)) len = sizeof(auth_header) - 1;
        strncpy(auth_header, p, len);
        auth_header[len] = '\0';
    }

    /* Route request */
    if (strncmp(path, "/api/", 5) == 0) {
        HttpRequest req;
        memset(&req, 0, sizeof(HttpRequest));
        snprintf(req.method, sizeof(req.method), "%s", method);
        snprintf(req.path, sizeof(req.path), "%s", path);
        snprintf(req.query, sizeof(req.query), "%s", query);
        snprintf(req.auth_header, sizeof(req.auth_header), "%s", auth_header);
        req.body = body;
        req.body_len = content_length;

        HttpResponse res;
        memset(&res, 0, sizeof(HttpResponse));

        db_lock();
        handle_api_request(&req, &res);
        db_unlock();

        const char* status_text = "OK";
        if (res.status_code == 201) status_text = "Created";
        else if (res.status_code == 204) status_text = "No Content";
        else if (res.status_code == 400) status_text = "Bad Request";
        else if (res.status_code == 404) status_text = "Not Found";
        else if (res.status_code == 500) status_text = "Internal Server Error";

        send_http_response(fd, res.status_code, status_text, res.content_type, res.body, res.body_len);
        http_response_free(&res);
    } else {
        serve_static_file(fd, path);
    }

    if (body) free(body);
    close_socket(fd);
    return NULL;
}

int http_server_start(const HttpServerConfig* config) {
    if (config->web_root) {
        strncpy(g_web_root, config->web_root, sizeof(g_web_root) - 1);
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 0;
    }
#endif

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("Socket creation failed");
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config->port);

    if (bind(g_server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close_socket(g_server_fd);
        g_server_fd = -1;
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    if (listen(g_server_fd, 64) < 0) {
        perror("Listen failed");
        close_socket(g_server_fd);
        g_server_fd = -1;
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    printf("\n=======================================================\n");
    printf("   CAMPUS LOST & FOUND CORE ENGINE ONLINE\n");
    printf("   Server Port: %d\n", config->port);
    printf("   Web Root:    %s\n", g_web_root);
    printf("   Web App URL: http://localhost:%d\n", config->port);
    printf("   API URL:     http://localhost:%d/api/statistics\n", config->port);
    printf("=======================================================\n\n");

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!g_running) break;
            continue;
        }

#ifdef _WIN32
        DWORD sock_timeout = 10000;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&sock_timeout, sizeof(sock_timeout));
#else
        struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
#endif

        ClientContext* ctx = malloc(sizeof(ClientContext));
        if (!ctx) {
            close_socket(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ctx) == 0) {
            pthread_detach(tid);
        } else {
            free(ctx);
            close_socket(client_fd);
        }
    }

    if (g_server_fd >= 0) {
        close_socket(g_server_fd);
        g_server_fd = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 1;
}

void http_server_stop(void) {
    g_running = 0;
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
        close_socket(g_server_fd);
        g_server_fd = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
