#include "http.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
  #include <io.h>
  #ifndef F_OK
  #define F_OK 0
  #endif
  #define access _access
#else
  #include <unistd.h>
#endif

static void handle_signal(int sig) {
    (void)sig;
    printf("\nGracefully shutting down Campus Lost & Found Server...\n");
    http_server_stop();
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -p <port>      Server port (default: 8080)\n");
    printf("  -d <db_path>   Database file path (default: lostfound.db)\n");
    printf("  -w <web_root>  Web root directory (default: web)\n");
    printf("  --seed         Populate database with sample campus records\n");
    printf("  -h, --help     Show this help message\n");
}

int main(int argc, char* argv[]) {
    int port = 8080;
    const char* db_file = "lostfound.db";
    const char* web_root = "web";
    int force_seed = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            db_file = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            web_root = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0) {
            force_seed = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Check if web_root exists in current or subfolder */
    if (access(web_root, F_OK) != 0) {
        if (access("college_lost_found_project/web", F_OK) == 0) {
            web_root = "college_lost_found_project/web";
        }
    }

    /* Check if db_file directory or schema is accessible */
    printf("[Init] Initializing database at '%s'...\n", db_file);
    if (!db_init(db_file)) {
        fprintf(stderr, "Fatal: Could not initialize database.\n");
        return 1;
    }

    /* Seed database only if explicitly requested via --seed */
    SystemStatistics stats;
    db_get_statistics(&stats);
    if (force_seed) {
        printf("[Init] Seeding database with realistic campus lost and found records...\n");
        db_seed();
        db_get_statistics(&stats);
    }
    printf("[Init] Loaded %d lost items, %d found items, %d matches.\n",
           stats.total_lost, stats.total_found, stats.possible_matches);

    /* Setup signal handlers for clean exit and socket safety */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    HttpServerConfig config;
    config.port = port;
    config.web_root = web_root;
    config.db_file = db_file;

    int res = http_server_start(&config);

    db_close();
    printf("[Shutdown] Database closed. Server terminated.\n");
    return res ? 0 : 1;
}
