/**
 * HTTP/1.1 Server
 * 
 * A simple HTTP server that handles GET requests on port 4221.
 * Supports routes: "/" (root), "/echo/*" (echo endpoint), and 404 for others.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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

int main() {
	// Disable output buffering for immediate console output
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd, client_fd;
	int client_addr_len;
	struct sockaddr_in client_addr;
	
	/**
	 * Create TCP socket (IPv4)
	 * AF_INET: IPv4, SOCK_STREAM: TCP, 0: default protocol
	 */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
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
	 * Configure server address: 0.0.0.0:4221
	 * INADDR_ANY (0.0.0.0) accepts connections on all network interfaces
	 */
	struct sockaddr_in serv_addr = { 
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	
	/* Bind socket to address and port */
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	/**
	 * Start listening with backlog of 5 connections
	 * Up to 5 pending connections can queue while server handles current request
	 */
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	/**
	 * Accept incoming connection (blocks until client connects)
	 * Returns new socket descriptor for this specific client
	 */
	client_fd = accept(server_fd, (struct sockaddr *) &client_addr, 
	                   (socklen_t *) &client_addr_len);
	
	if (client_fd == -1) {
		printf("Accept failed: %s\n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	/* Log client connection info */
	printf("Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
	printf("Client Port: %d\n", ntohs(client_addr.sin_port));	
	
	/**
	 * Receive HTTP request from client
	 * Buffer size 4096 is sufficient for most HTTP headers
	 */
	char req[4096] = {0};
	ssize_t recv_rq = recv(client_fd, req, sizeof(req) - 1, 0);
 
	if (recv_rq <= 0) {
		printf("recv failed : %s\n", strerror(errno));
		close(client_fd);
		close(server_fd);
		return 1;
	}
	
	/**
	 * Parse HTTP request line: "METHOD PATH HTTP/VERSION"
	 * Extract method (e.g., GET) and path (e.g., /echo/hello)
	 */
	char method[16] = {0};
	char path[256] = {0};

	if (sscanf(req, "%15s %255s", method, path) != 2) {
		printf("parsing request failed : %s\n", strerror(errno));
		close(client_fd);
		close(server_fd);
		return 1;
	}

	/**
	 * Route handler: Match request path and send appropriate response
	 */
	
	/* Route: "/" - Root endpoint */
	if (strcmp(path, "/") == 0) {
		const char *hdr = "HTTP/1.1 200 OK\r\n\r\n";
		const char *body = path;
		
		if (send(client_fd, hdr, strlen(hdr), 0) < 0 || 
		    send(client_fd, body, strlen(body), 0) < 0) {
			printf("send failed : %s\n", strerror(errno));
		}
	
	/* Route: "/echo/*" - Echo back the text after /echo/ */
	} else if (strncmp(path, "/echo/", 6) == 0) {
		const char *hdr = "HTTP/1.1 200 OK\r\n\r\n";
		
		/* Extract text after "/echo/" (skip first 6 characters) */
		char *echo_str = remove_first_n_copy(path, 6);
		if (!echo_str) {
			printf("malloc failed\n");
			close(client_fd);
			close(server_fd);
			return 1;
		}
		
		const char *body = echo_str;
		if (send(client_fd, hdr, strlen(hdr), 0) < 0 || 
		    send(client_fd, body, strlen(body), 0) < 0) {
			printf("send failed : %s\n", strerror(errno));
		}
		
		free(echo_str); /* Free dynamically allocated string */
	
	/* Route: Default - 404 Not Found */
	} else {
		const char *hdr = "HTTP/1.1 404 Not Found\r\n\r\n";
		
		if (send(client_fd, hdr, strlen(hdr), 0) < 0) {
			printf("send failed : %s\n", strerror(errno));
		}
	}

	/* Clean up: close sockets */
	close(client_fd);
	close(server_fd);

	return 0;
}