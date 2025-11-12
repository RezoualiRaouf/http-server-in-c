#include "netlib.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

// External global variables from main.c
extern char *g_directory;
extern char *g_single_file;
static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;


int set_server_adds(int server_fd, int port) {
    struct sockaddr_in serv_addr = { 
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(INADDR_ANY) },
    };
    
    /* Bind socket to address and port */
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        close(server_fd);
        return 0;
    }   
    return 1;   
}

int ends_with(char *input, char *extension) {
    if (input == NULL || extension == NULL)
        return 0;

    // get strings length
    size_t input_len, ext_len;
    input_len = strlen(input);
    ext_len = strlen(extension);

    if (ext_len < 1 || ext_len > input_len)
        return 0;
    
    size_t start_index = input_len - ext_len;

    while (*extension != '\0') {
        if (*extension != input[start_index])   
            return 0;
        extension++;
        start_index++;
    }

    return 1;
}

void serve_file(int client_fd, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "File not found: %s\n", filepath);
        send_error_response(client_fd, 404,0);
        return;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Check for empty file
    if (size == 0) {
        fclose(fp);
        send_success_response(client_fd, NULL, get_content_type(filepath), 0);
        return;
    }

    // Allocate buffer
    char *buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "malloc failed for file buffer\n");
        fclose(fp);
        send_error_response(client_fd, 500,0);
        return;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, size, fp);
    fclose(fp);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Failed to read file completely\n");
        free(buffer);
        send_error_response(client_fd, 500,0);
        return;
    }

    // Send response
    char *content_type = get_content_type(filepath);
    send_success_response(client_fd, buffer, content_type, size);

    free(buffer);
}

char *get_content_type(const char *filename) {
    if (ends_with((char *)filename, ".html") || ends_with((char *)filename, ".htm")) {
        return "text/html";
    } else if (ends_with((char *)filename, ".css")) {
        return "text/css";
    } else if (ends_with((char *)filename, ".js")) {
        return "text/javascript";
    } else if (ends_with((char *)filename, ".json")) {
        return "application/json";
    } else if (ends_with((char *)filename, ".png")) {
        return "image/png";
    } else if (ends_with((char *)filename, ".jpg") || ends_with((char *)filename, ".jpeg")) {
        return "image/jpeg";
    } else if (ends_with((char *)filename, ".gif")) {
        return "image/gif";
    } else if (ends_with((char *)filename, ".svg")) {
        return "image/svg+xml";
    } else if (ends_with((char *)filename, ".txt")) {
        return "text/plain";
    } else {
        return "application/octet-stream";
    }
}


void serve_file_keepalive(int client_fd, const char *filepath, int keep_alive) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "File not found: %s\n", filepath);
        send_error_response(client_fd, 404, keep_alive);
        return;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Check for empty file
    if (size == 0) {
        fclose(fp);
        send_success_response_keepalive(client_fd, NULL, get_content_type(filepath), 0, keep_alive);
        return;
    }

    // Allocate buffer
    char *buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "malloc failed for file buffer\n");
        fclose(fp);
        send_error_response(client_fd, 500, 0);
        return;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, size, fp);
    fclose(fp);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Failed to read file completely\n");
        free(buffer);
        send_error_response(client_fd, 500, 0);
        return;
    }

    // Send response
    char *content_type = get_content_type(filepath);
    send_success_response_keepalive(client_fd, buffer, content_type, size, keep_alive);

    free(buffer);
}

void *handel_client(void *arg) {
    int client_fd = *((int*)arg);
    free(arg);
    
    // Get client IP address
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    char client_ip[INET_ADDRSTRLEN] = "unknown";
    
    if (getpeername(client_fd, (struct sockaddr *)&addr, &addr_size) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    }
    
    // Set socket timeout (5 seconds)
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int request_count = 0;
    int keep_alive = 1;
    
    // Keep connection alive for multiple requests
    while (keep_alive) {
        char req[4096] = {0};
        ssize_t recv_rq = recv(client_fd, req, sizeof(req) - 1, 0);
     
        if (recv_rq <= 0) {
            if (recv_rq == 0) {
                log_message(LOG_INFO, "Client %s closed connection after %d requests", 
                           client_ip, request_count);
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                log_message(LOG_INFO, "Client %s timeout after %d requests", 
                           client_ip, request_count);
            } else {
                log_message(LOG_ERROR, "recv failed from %s: %s", client_ip, strerror(errno));
            }
            break;
        }
        
        request_count++;
        
        http_request request_data = parse_http_request(req);
        
        if (!request_data.valid) {
            send_error_response(client_fd, 400, 0);  // Don't keep alive on error
            log_request(client_ip, "INVALID", "-", 400, 0);
            break;
        }

        // Check if client wants to close connection
        char connection_header[64] = {0};
        if (get_header_value(req, "Connection", connection_header, sizeof(connection_header))) {
            if (strcasecmp(connection_header, "close") == 0) {
                keep_alive = 0;  // Client wants to close
            }
        }

        // Limit requests per connection (prevent abuse)
        if (request_count >= 100) {
            keep_alive = 0;  // Force close after 100 requests
        }

        // Only handle GET requests
        if (strcmp(request_data.method, "GET") != 0) {
            send_error_response(client_fd, 405, 0);
            log_request(client_ip, request_data.method, request_data.path, 405, 0);
            break;
        }

        // Single file mode
        if (g_single_file) {
            char *expected_filename = strrchr(g_single_file, '/');
            if (expected_filename) {
                expected_filename++;
            } else {
                expected_filename = g_single_file;
            }
            
            char expected_path[512];
            snprintf(expected_path, sizeof(expected_path), "/%s", expected_filename);
            
            if (strcmp(request_data.path, expected_path) == 0) {
                FILE *fp = fopen(g_single_file, "rb");
                long size = 0;
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    size = ftell(fp);
                    fclose(fp);
                }
                serve_file_keepalive(client_fd, g_single_file, keep_alive);
                log_request(client_ip, request_data.method, request_data.path, 200, size);
            } else {
                send_error_response(client_fd, 404, keep_alive);
                log_request(client_ip, request_data.method, request_data.path, 404, 0);
            }
            continue;
        }

        // Directory mode
        if (g_directory) {
            int status_code = 200;
            size_t bytes_sent = 0;
            
            if (strcmp(request_data.path, "/") == 0) {
                char index_path[512];
                snprintf(index_path, sizeof(index_path), "%s/index.html", g_directory);
                
                FILE *fp = fopen(index_path, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    bytes_sent = ftell(fp);
                    fclose(fp);
                    serve_file_keepalive(client_fd, index_path, keep_alive);
                } else {
                    status_code = 404;
                    send_error_response(client_fd, 404, keep_alive);
                }
                log_request(client_ip, request_data.method, request_data.path, status_code, bytes_sent);
            } else {
                char *requested_path = request_data.path + 1;
                
                if (strstr(requested_path, "..") != NULL) {
                    send_error_response(client_fd, 403, 0);
                    log_request(client_ip, request_data.method, request_data.path, 403, 0);
                    break;
                }

                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", g_directory, requested_path);
                
                FILE *fp = fopen(full_path, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    bytes_sent = ftell(fp);
                    fclose(fp);
                    serve_file_keepalive(client_fd, full_path, keep_alive);
                    status_code = 200;
                } else {
                    send_error_response(client_fd, 404, keep_alive);
                    status_code = 404;
                }
                log_request(client_ip, request_data.method, request_data.path, status_code, bytes_sent);
            }
            continue;
        }

        send_error_response(client_fd, 500, 0);
        log_request(client_ip, request_data.method, request_data.path, 500, 0);
        break;
    }
    
    close(client_fd);
    return NULL;
}

int send_error_response(int client_fd, int code, int keep_alive) {
    char *reason;
    switch (code) {
        case 400: reason = "Bad Request"; break;
        case 401: reason = "Unauthorized"; break;
        case 403: reason = "Forbidden"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 408: reason = "Request Timeout"; break;
        default:  reason = "Internal Server Error"; code = 500; break;
    }
    
    char hdr[256];
    const char *connection = keep_alive ? "keep-alive" : "close";
    
    snprintf(hdr, sizeof(hdr), 
             "HTTP/1.1 %d %s\r\n"
             "Content-Length: 0\r\n"
             "Connection: %s\r\n"
             "\r\n", 
             code, reason, connection);
    
    if (send(client_fd, hdr, strlen(hdr), 0) < 0) {
        printf("error in sending : %s \n", strerror(errno));
        return 0;
    }
    return 1;
}

int send_success_response(int client_fd, char *body, char *content_type, size_t content_length) {
    char hdr[512];
    
    if (body == NULL) {
        snprintf(hdr, sizeof(hdr), "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        if (send(client_fd, hdr, strlen(hdr), 0) < 0) {
            printf("error in sending: %s\n", strerror(errno));
            return 0;
        }
        return 1;
    } else {
        if (content_type == NULL)
            content_type = "application/octet-stream";
    
        snprintf(hdr, sizeof(hdr), 
                 "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n",
                 content_type, content_length);
                 
        if (send(client_fd, hdr, strlen(hdr), 0) < 0 || 
            send(client_fd, body, content_length, 0) < 0) {
            printf("error in sending: %s\n", strerror(errno));
            return 0;
        }
    }   

    return 1;
}

int send_success_response_keepalive(int client_fd, char *body, char *content_type, 
                                    size_t content_length, int keep_alive) {
    char hdr[512];
    const char *connection = keep_alive ? "keep-alive" : "close";
    
    if (body == NULL) {
        snprintf(hdr, sizeof(hdr), 
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: %s\r\n"
                 "\r\n", 
                 connection);
        if (send(client_fd, hdr, strlen(hdr), 0) < 0) {
            printf("error in sending: %s\n", strerror(errno));
            return 0;
        }
        return 1;
    } else {
        if (content_type == NULL)
            content_type = "application/octet-stream";
    
        snprintf(hdr, sizeof(hdr), 
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: %s\r\n"
                 "Keep-Alive: timeout=5, max=100\r\n"
                 "\r\n",
                 content_type, content_length, connection);
                 
        if (send(client_fd, hdr, strlen(hdr), 0) < 0 || 
            send(client_fd, body, content_length, 0) < 0) {
            printf("error in sending: %s\n", strerror(errno));
            return 0;
        }
    }   

    return 1;
}

int get_header_value(char req[], const char *header_name, char *out_value, size_t out_len) {
    if (!req || !header_name || !out_value) 
        return 0;

    char search_arr[128] = {0};
    snprintf(search_arr, sizeof(search_arr), "%s: ", header_name);

    // find the start of the string
    char *start_value = strstr(req, search_arr);

    if (!start_value) return 0;
    
    start_value += strlen(search_arr);

    // find the end of the string 
    char *end_value = strstr(start_value, "\r\n");

    if (!end_value) return 0;

    // calc the length and copy it
    ssize_t in_len = end_value - start_value;   
        
    if (in_len >= (ssize_t)out_len) in_len = out_len - 1; 
        
    strncpy(out_value, start_value, in_len);
    out_value[in_len] = '\0';

    return 1;
}

http_request parse_http_request(char req[]) {
    http_request parsed = {0};
    parsed.content_type = NULL;
    parsed.valid = 1;
    
    if (req == NULL) {
        parsed.valid = 0;
        return parsed;
    }

    if (sscanf(req, "%15s %255s %15s", parsed.method, parsed.path, parsed.version) != 3) {
        printf("parsing request failed : %s\n", strerror(errno));
        parsed.valid = 0;
        return parsed;
    }
    
    get_header_value(req, "User-Agent", parsed.user_agent, sizeof(parsed.user_agent));
    get_header_value(req, "Host", parsed.host, sizeof(parsed.host));

    char content_length[32] = {0};
    if (get_header_value(req, "Content-Length", content_length, sizeof(content_length)))
        parsed.content_length = atoi(content_length);

    return parsed;
}

/**
 * Remove first n characters from a string and return a new malloc'd copy.
 * 
 * @param s - Source string to copy from
 * @param n - Number of characters to skip from the beginning
 * @return New dynamically allocated string, or NULL on error
 * 
 * Note: Caller must free() the returned string.
 * If n >= strlen(s), returns an empty string that must still be freed.
 */
char *remove_first_n_copy(const char *s, size_t n) {
    if (!s) return NULL;
    
    size_t len = strlen(s);
    size_t newlen = (n >= len) ? 0 : len - n;

    char *out = malloc(newlen + 1); /* +1 for null terminator */
    if (!out) return NULL; /* malloc failed */

    if (newlen > 0) {
        memcpy(out, s + n, newlen);
    }
    out[newlen] = '\0';
    return out;
}

/**
 * Initialize logging system
 * @param log_file - Path to log file, or NULL for stdout only
 */

void init_logging(const char *log_file) {
    if (log_file) {
        g_log_file = fopen(log_file, "a");  // Append mode
        if (!g_log_file) {
            fprintf(stderr, "Warning: Could not open log file '%s': %s\n", 
                    log_file, strerror(errno));
            fprintf(stderr, "Logging to stdout only.\n");
        } else {
            printf("Logging to file: %s\n", log_file);
        }
    }
}

/**
 * Close logging system
 */
void close_logging(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/**
 * Get current timestamp string
 */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * Get log level string
 */
static const char* log_level_string(log_level level) {
    switch (level) {
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR:   return "ERROR";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKNOWN";
    }
}

/**
 * Log a formatted message
 * Thread-safe with mutex protection
 */
void log_message(log_level level, const char *format, ...) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    pthread_mutex_lock(&g_log_mutex);
    
    // Log to stdout
    printf("[%s] [%s] %s\n", timestamp, log_level_string(level), message);
    
    // Log to file if available
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] %s\n", timestamp, log_level_string(level), message);
        fflush(g_log_file);  // Ensure it's written immediately
    }
    
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * Log HTTP request in Apache Common Log Format
 * Format: client_ip - - [timestamp] "METHOD path HTTP/1.1" status_code bytes_sent
 */
void log_request(const char *client_ip, const char *method, const char *path, 
                 int status_code, size_t bytes_sent) {
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    pthread_mutex_lock(&g_log_mutex);
    
    // Apache Common Log Format
    const char *log_line = "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n";
    
    // Log to stdout
    printf(log_line, client_ip, timestamp, method, path, status_code, bytes_sent);
    
    // Log to file if available
    if (g_log_file) {
        fprintf(g_log_file, log_line, client_ip, timestamp, method, path, status_code, bytes_sent);
        fflush(g_log_file);
    }
    
    pthread_mutex_unlock(&g_log_mutex);
}