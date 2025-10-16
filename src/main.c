/**
 * HTTP/1.1 server
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
	 * SOCK_STREAM: TCP 
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
	 
	int connection_backlog = 5; // max connect to listen to
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
	
	
		printf("Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
		printf("Client Port: %d\n", ntohs(client_addr.sin_port));	
	
	/**
	 * Receive and read the request from the 
	 * clients using  client_fd
	 */	
		
  char req[4096]; // to store the request
	ssize_t recv_rq = recv(client_fd, req, sizeof(req) - 1, 0);
 
	// check for errors while receiving  	

	if (recv_rq < 0 || recv_rq == 0) {
  printf("recv failed : %s\n",strerror(errno));
  close(client_fd);
	return 0;
  }

	/**
	* send to the clinet a connection based on the request using parsing
	*/
	
	char method[16] = {0};           // Sets all to 0		
	char path[256] = {0};

	if (sscanf(req, "%15s %244s",method,path) < 0) {	
	  printf("parsing request failed : %s\n",strerror(errno));
	  close(client_fd);
		}

	if (strcmp(path, "/") == 0) {
		  const char *hdr =  "HTTP/1.1 200 OK\r\n"
			"\r\n";
		if (send(client_fd, hdr, strlen(hdr), 0)) {
	  printf("parsing request failed : %s\n",strerror(errno));
	  close(client_fd);
	  }	
  }else {
		  const char *hdr =  "HTTP/1.1 404  not found\r\n"
			"\r\n";
		
 	if (send(client_fd, hdr, strlen(hdr), 0)) {
 		  printf("parsing request failed : %s\n",strerror(errno));
 		  close(client_fd);
 		  }	
 		}

	// Clean up: close both client and server sockets
	close(client_fd);
	close(server_fd);

	return 0;
}