/**
 * HTTP/1.1 Server
 * 
 * A HTTP server that handles GET requests on port 4221.
 *
 */

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


int main() {
	// Disable output buffering for immediate console output
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);


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
	int connection_backlog = 7;
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
	

	http_request request_data = parse_http_request(req)	;
	
	if(!request_data.valid){
		send_error_response(client_fd,400);
		printf("error while parsing the request %s \n",strerror(errno));
		close(client_fd);
		close(server_fd);
		return 1;
	}

	
	/* Route: "/" - Root endpoint */
	if (strcmp(request_data.path, "/") == 0) {
		
		if(!send_success_response(client_fd, NULL,NULL,1)){
				close(client_fd);
				close(server_fd);
				return 1;
		}

	/* Route: "/echo/*" - Echo back the text after /echo/ */
	} else if (strncmp(request_data.path, "/echo/", 6) == 0) {
		
		/* Extract text after "/echo/" (skip first 6 characters) */
		char *echo_str = remove_first_n_copy(request_data.path, 6);

		if (!echo_str) {
			printf("malloc failed\n");
			close(client_fd);
			close(server_fd);
			return 1;
		}

		if(!send_success_response(client_fd,echo_str,"text/plain",
							strlen(echo_str))){
				close(client_fd);
				close(server_fd);
				return 1;
		}		
		free(echo_str); /* Free dynamically allocated string */
	
	/* Rout: "/User-Agent*" back the users agent*/ 
	}else if (strncmp(request_data.path, "/user-agent",11) == 0) {

			if (!send_success_response(client_fd,request_data.user_agent,"text/plain",
								strlen(request_data.user_agent))){
					close(client_fd);
					close(server_fd);				
					return 1;
			}
			

	/* Route: Default - 404 Not Found */		
	} else {
		send_error_response(client_fd,404);	
			close(client_fd);
			close(server_fd);
			return 1;
	}
	return 0;
}
