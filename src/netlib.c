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
#include <unistd.h>



void *handel_client(void *arg){

	int client_fd = *((int*)arg);
	free(arg);
	char req[4096] = {0};
	ssize_t recv_rq = recv(client_fd, req, sizeof(req) - 1, 0);
 
	if (recv_rq <= 0) {
		printf("recv failed : %s\n", strerror(errno));
		close(client_fd);
		return NULL;
	}
	
	http_request request_data = parse_http_request(req)	;
	
	if(!request_data.valid){
		send_error_response(client_fd,400);
		printf("error while parsing the request %s \n",strerror(errno));
		close(client_fd);
		return NULL;
	}

	
	if (strcmp(request_data.path, "/") == 0) {
		
		if(!send_success_response(client_fd, NULL,NULL,1)){
				close(client_fd);
				return NULL;
		}

	} else if (strncmp(request_data.path, "/echo/", 6) == 0) {
		

		char *echo_str = remove_first_n_copy(request_data.path, 6);

		if (!echo_str) {
			printf("malloc failed\n");
			close(client_fd);
			return NULL;
		}

		if(!send_success_response(client_fd,echo_str,"text/plain",
							strlen(echo_str))){
				close(client_fd);
				return NULL;
		}		
		free(echo_str); /* Free dynamically allocated string */
	

	}else if (strncmp(request_data.path, "/user-agent",11) == 0) {

			if (!send_success_response(client_fd,request_data.user_agent,"text/plain",
								strlen(request_data.user_agent))){
					close(client_fd);
					return NULL;
			}

	} else {
		send_error_response(client_fd,404);	
	}
	
	close(client_fd);

	return NULL;
}


int send_error_response(int client_fd, int code){

	char *reason;
   switch (code) {
        case 400: reason = "Bad Request"; break;
        case 401: reason = "Unauthorized"; break;
        case 403: reason = "Forbidden"; break;
        case 404: reason = "Not Found"; break;
        case 408: reason = "Request Timeout"; break;
        default:  reason = "Internal Server Error"; code = 500; break;
    }
	
	char hdr[256];
	snprintf(hdr,sizeof(hdr), "HTTP/1.1 %d %s\r\n Content-Length: 0\r\n Connection:	close\r\n\r\n", code , reason);
	
	if (send(client_fd,hdr,strlen(hdr),0) < 0){
				printf("error in sending : %s \n",strerror(errno));
				return 0;
	}
	return 1;
}


int send_success_response(int client_fd, char *body, char *content_type, size_t content_length) {

  char hdr[512];
    
    if (body == NULL) {
        snprintf(hdr, sizeof(hdr),  "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        if (send(client_fd, hdr, strlen(hdr), 0) < 0) {
            printf("error in sending: %s\n", strerror(errno));
						return 0;
        }
				return 1;

    }else{
				if (content_type == NULL)
					content_type = "application/octet-stream";
	
				snprintf(hdr, sizeof(hdr), 
  	          "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n",
              content_type, content_length);

								 
		    if (send(client_fd, hdr, strlen(hdr), 0) < 0 || 
 	      		send(client_fd, body,content_length, 0) < 0) {
  	        printf("error in sending: %s\n", strerror(errno));
					return 0;
   		 }
		}	

	return 1;
}


int get_header_value(char req[],const char *header_name,char *out_value, size_t out_len){

	if (!req || !header_name || !out_value) 
		return 0;

	char search_arr[128] = {0}; //helps in getting the length of the first part of the header
	snprintf(search_arr, sizeof(search_arr),"%s: ", header_name);

	// find the start of the string
	char *start_value = strstr(req, search_arr);  // return a pointer to the start of the needed data of the header

	if (!start_value) return 0;
	
	start_value += strlen(search_arr);  //skip the unwanted format exp "User-Agent:"

	// find the end of the string 
	char *end_value = strstr(start_value, "\r\n");

	if (!end_value) return 0;

	// calc the length and copy it
		ssize_t in_len = end_value - start_value;	
		
	if (in_len >= out_len) in_len = out_len -1;	
		
	strncpy(out_value,start_value,in_len);
	out_value[in_len] = '\0';

	return 1;
}


http_request parse_http_request(char req[]){

	http_request parsed = {0};
	parsed.valid = 1;
	
	if (req == NULL){
		parsed.valid = 0;
		return parsed;
	}

	if (sscanf(req, "%15s %255s %15s",parsed.method,parsed.path,parsed.version) != 3) {
		printf("parsing request failed : %s\n",strerror(errno));
		parsed.valid = 0;
		return parsed;
	}
	
	get_header_value(req,"User-Agent",parsed.user_agent,sizeof(parsed.user_agent));
	get_header_value(req,"Host",parsed.host,sizeof(parsed.host));
	get_header_value(req,"Content-Type",parsed.content_type,sizeof(parsed.content_type));


	char content_length[32] = {0};
	if (get_header_value(req,"Content-Length",content_length,sizeof(content_length)))
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