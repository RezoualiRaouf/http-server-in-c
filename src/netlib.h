#ifndef NETLIB_H
#define NETLIB_H
#include "stddef.h"

typedef struct http_request{
	char method[16];
	char path[256];
	char version[16];
	char user_agent[256];
	char host[256];
	char *content_type;
	int content_length;
	int valid;
}http_request;

int ends_with(char *input, char *extension);
void *handel_client(void *arg);
int send_error_response(int client_fd, int code);
int send_success_response(int client_fd, char *body, char *content_type, size_t content_length);
int get_header_value(char req[],const char *header_name,char *out_value, size_t out_len);
http_request parse_http_request(char req[]);
char *remove_first_n_copy(const char *s, size_t n);

#endif