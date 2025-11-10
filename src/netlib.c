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

// External global variables from main.c
extern char *g_directory;
extern char *g_single_file;

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
        send_error_response(client_fd, 404);
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
        send_error_response(client_fd, 500);
        return;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, size, fp);
    fclose(fp);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Failed to read file completely\n");
        free(buffer);
        send_error_response(client_fd, 500);
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


void *handel_client(void *arg) {
    int client_fd = *((int*)arg);
    free(arg);
    
    char req[4096] = {0};
    ssize_t recv_rq = recv(client_fd, req, sizeof(req) - 1, 0);
 
    if (recv_rq <= 0) {
        printf("recv failed : %s\n", strerror(errno));
        close(client_fd);
        return NULL;
    }
    
    http_request request_data = parse_http_request(req);
    
    if (!request_data.valid) {
        send_error_response(client_fd, 400);
        printf("error while parsing the request %s \n", strerror(errno));
        close(client_fd);
        return NULL;
    }

    printf("Request: %s %s\n", request_data.method, request_data.path);

    // Only handle GET requests
    if (strcmp(request_data.method, "GET") != 0) {
        send_error_response(client_fd, 405);  // Method Not Allowed
        close(client_fd);
        return NULL;
    }

    // Single file mode - check if requested path matches the filename
    if (g_single_file) {
        // Extract just the filename from g_single_file path
        char *expected_filename = strrchr(g_single_file, '/');
        if (expected_filename) {
            expected_filename++; // Skip the '/'
        } else {
            expected_filename = g_single_file; // No directory in path
        }
        
        // Build expected URL path: /filename.ext
        char expected_path[512];
        snprintf(expected_path, sizeof(expected_path), "/%s", expected_filename);
        
        // Check if requested path matches
        if (strcmp(request_data.path, expected_path) == 0) {
            serve_file(client_fd, g_single_file);
        } else {
            // Wrong path requested
            printf("Single-file mode: Expected %s, got %s\n", expected_path, request_data.path);
            send_error_response(client_fd, 404);
        }
        close(client_fd);
        return NULL;
    }

    // Directory mode
    if (g_directory) {
        // Handle root path
        if (strcmp(request_data.path, "/") == 0) {
            // Try to serve index.html
            char index_path[512];
            snprintf(index_path, sizeof(index_path), "%s/index.html", g_directory);
            serve_file(client_fd, index_path);
            close(client_fd);
            return NULL;
        } 
        else {
            // Remove leading slash from path
            char *requested_path = request_data.path + 1; // Skip the '/'
            
            // Security check: prevent path traversal
            if (strstr(requested_path, "..") != NULL) {
                send_error_response(client_fd, 403);  // Forbidden
                close(client_fd);
                return NULL;
            }

            // Build full path
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", g_directory, requested_path);
            
            serve_file(client_fd, full_path);
            close(client_fd);
            return NULL;
        }
    }

    // No mode configured (shouldn't happen)
    send_error_response(client_fd, 500);
    close(client_fd);
    return NULL;
}

int send_error_response(int client_fd, int code) {
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
    snprintf(hdr, sizeof(hdr), "HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", code, reason);
    
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