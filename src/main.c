/**
 * HTTP/1.1 Server
 * 
 * A HTTP server that handles GET requests on port 4221.
 *
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



int main(int ac,char **av) {
	// Disable output buffering for immediate console output
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	  char *directory = NULL;
    char *single_file = NULL;
    int port = 4221;
    int opt;
    
    // getopt automatically handles -d, -f, -p
    while ((opt = getopt(ac, av, "d:f:p:")) != -1) {
        switch (opt) {
            case 'd':
                directory = optarg;
                break;
            case 'f':
                single_file = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-d directory] [-f file] [-p port]\n", av[0]);
                return 1;
        }
    }
	
		printf("%s \n %d \n",directory,port);
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

	set_server_adds(server_fd,port);
	
	/**
	 * Start listening with backlog of 5 connections
	 * Up to 5 pending connections can queue while server handles current request
	 */
	int connection_backlog = 7;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		close(server_fd);
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	/**
	 * Accept incoming connection (blocks until client connects)
	 * Returns new socket descriptor for this specific client
	 */

	while (1) {
	
	client_fd = accept(server_fd, (struct sockaddr *) &client_addr, 
	                   (socklen_t *) &client_addr_len);
	
	if (client_fd == -1) {
		printf("Accept failed: %s\n", strerror(errno));
		close(server_fd);
		return 1;
	}

	pthread_t t;
	int *client_fd_ptr = malloc(sizeof(int));
	if (client_fd_ptr == NULL) {
		printf("alocation failed %s \n",strerror(errno));
		close(client_fd);
		continue;
	}
	
	*client_fd_ptr = client_fd;
	
	if(pthread_create(&t,NULL,handel_client,client_fd_ptr) != 0)
	{
		perror("failed to create a theread");
		free(client_fd_ptr);
		close(client_fd);
		continue;
	}

	pthread_detach(t);
	/* Log client connection info */
	printf("Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
	printf("Client Port: %d\n", ntohs(client_addr.sin_port));	
	
	}	
return 0;
}
