/**
 * Simple TCP Server
 * 
 * This program creates a basic TCP server that listens on port 4221
 * and accepts a single client connection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
	// Disable output buffering to ensure immediate console output
	// Useful for debugging and real-time log monitoring
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	// File descriptors for server socket and client connection
	int server_fd, client_fd;
	int client_addr_len;
	struct sockaddr_in client_addr;
	
	/**
	 * Create a TCP socket using IPv4
	 * AF_INET: IPv4 protocol family
	 * SOCK_STREAM: TCP (connection-oriented, reliable)
	 * 0: Default protocol for the socket type
	 */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	// Check if socket creation failed
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	/**
	 * Set SO_REUSEADDR socket option
	 * This allows the server to bind to a port that was recently used,
	 * preventing "Address already in use" errors during rapid restarts.
	 * Essential for development and testing environments.
	 */
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	/**
	 * Configure server address structure
	 * sin_family: Address family (IPv4)
	 * sin_port: Port number in network byte order (big-endian)
	 * sin_addr: IP address (INADDR_ANY = 0.0.0.0, accepts connections on any interface)
	 */
	struct sockaddr_in serv_addr = { 
		.sin_family = AF_INET,
		.sin_port = htons(4221),  // htons: host to network short (byte order conversion)
		.sin_addr = { htonl(INADDR_ANY) },  // htonl: host to network long
	};
	
	/**
	 * Bind the socket to the specified address and port
	 * This associates the socket with a specific network interface and port number
	 */
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	/**
	 * Start listening for incoming connections
	 * connection_backlog: Maximum number of pending connections in the queue
	 * Connections beyond this limit will be refused until the queue has space
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
	 * Accept an incoming client connection
	 * This blocks until a client connects
	 * Returns a new socket file descriptor for communicating with the client
	 */
	client_fd = accept(server_fd, (struct sockaddr *) &client_addr, 
	                   (socklen_t *) &client_addr_len);
	
	// Check if accept failed
	if (client_fd == -1) {
		printf("Accept failed: %s\n", strerror(errno));
		close(server_fd);
		return 1;
	}
	
	printf("Client connected\n");
	
	// TODO: Add code here to communicate with the client
	// Example: send/recv data using client_fd
	
	// Clean up: close both client and server sockets
	close(client_fd);
	close(server_fd);
	
	return 0;
}