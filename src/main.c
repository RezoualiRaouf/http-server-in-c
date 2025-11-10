/**
 * HTTP/1.1 Server with Command-Line Arguments
 * 
 * Usage:
 *   ./server -f <file>           Serve single file to all requests
 *   ./server -d <directory>      Serve files from directory
 *   ./server -p <port>           Custom port (default: 4221)
 */

#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "netlib.h"
#include <pthread.h>

// Global configuration
char *g_directory = NULL;
char *g_single_file = NULL;
static char *g_log_file = NULL;


int main(int ac, char **av) {
    // Disable output buffering for immediate console output
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int port = 4221;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(ac, av, "d:f:p:l:h")) != -1) {
        switch (opt) {
            case 'd':
                g_directory = optarg;
                break;
            case 'f':
                g_single_file = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: Invalid port number\n");
                    return 1;
                }
                break;
            case 'l':
                g_log_file = optarg;
                break;
            case 'h':
            default:
                fprintf(stderr, "Usage: %s [-d directory] [-f file] [-p port] [-l logfile]\n", av[0]);
                fprintf(stderr, "  -d <directory>  Serve files from directory\n");
                fprintf(stderr, "  -f <file>       Serve single file to all requests\n");
                fprintf(stderr, "  -p <port>       Port number (default: 4221)\n");
                fprintf(stderr, "  -l <logfile>    Log file path (default: stdout only)\n");
                return (opt == 'h') ? 0 : 1;
        }
    } 

    // Validate arguments

    if (g_directory && g_single_file) {
        fprintf(stderr, "Error: Cannot use both -d and -f\n");
        return 1;
    }

    if (g_single_file) {
        FILE *fp = fopen(g_single_file, "rb");
        if (!fp) {
            log_message(LOG_ERROR, "Cannot open file '%s': %s", g_single_file, strerror(errno));
            return 1;
        }
        fclose(fp);
        log_message(LOG_INFO, "Server mode: Single file (%s)", g_single_file);
    } else if (g_directory) {
        log_message(LOG_INFO, "Server mode: Directory (%s)", g_directory);
    } else {
        g_directory = ".";
        log_message(LOG_INFO, "Server mode: Directory (current directory)");
    }

    int server_fd, client_fd;
    int client_addr_len;
    struct sockaddr_in client_addr;
    
    init_logging(g_log_file);
    log_message(LOG_INFO, "Server starting...");
    
    /**
     * Create TCP socket (IPv4)
     * AF_INET: IPv4, SOCK_STREAM: TCP, 0: default protocol
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
    log_message(LOG_ERROR, "Socket creation failed: %s", strerror(errno));
    close_logging();
    return 1;
    }

    /**
     * Enable SO_REUSEADDR to prevent "Address already in use" errors
     * Allows immediate port reuse after server restart
     */
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        close(server_fd);
        return 1;
    }

    /**
     * Configure server address and bind to port
     */
    set_server_adds(server_fd, port);

    /**
     * Start listening with backlog of 7 connections
     */
    int connection_backlog = 7;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d...\n", port);
    client_addr_len = sizeof(client_addr);

    log_message(LOG_INFO, "Server listening on port %d", port);
    
    /**
     * Accept incoming connections (blocks until client connects)
     * Returns new socket descriptor for this specific client
     */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                          (socklen_t *)&client_addr_len);

        if (client_fd == -1) {
            printf("Accept failed: %s\n", strerror(errno));
            close(server_fd);
            return 1;
        }

        pthread_t t;
        int *client_fd_ptr = malloc(sizeof(int));
        if (client_fd_ptr == NULL) {
            printf("allocation failed %s \n", strerror(errno));
            close(client_fd);
            continue;
        }

        *client_fd_ptr = client_fd;

        if (pthread_create(&t, NULL, handel_client, client_fd_ptr) != 0) {
            perror("failed to create a thread");
            free(client_fd_ptr);
            close(client_fd);
            continue;
        }

        pthread_detach(t);
        /* Log client connection info */
        printf("Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
        printf("Client Port: %d\n", ntohs(client_addr.sin_port));
    }

    close(server_fd);

    log_message(LOG_INFO, "Server shutting down");
    close_logging();
    return 0;
}
