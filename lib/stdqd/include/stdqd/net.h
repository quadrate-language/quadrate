#ifndef STDQD_NET_H
#define STDQD_NET_H

#include <quadrate/runtime/context.h>
#include <quadrate/runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_stdqd_listen(qd_context* ctx);
qd_exec_result qd_stdqd_accept(qd_context* ctx);
qd_exec_result qd_stdqd_connect(qd_context* ctx);
qd_exec_result qd_stdqd_send(qd_context* ctx);
qd_exec_result qd_stdqd_receive(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif

/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(void) {
	int server = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(8080);  // fixed port 8080

	bind(server, (struct sockaddr*)&addr, sizeof(addr));
	listen(server, 1);

	printf("Serving on http://localhost:8080/\n");

	while (1) {
		int client = accept(server, NULL, NULL);
		char buffer[1024];
		read(client, buffer, sizeof(buffer)); // ignore the request

		const char *response =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 6\r\n"
			"Connection: close\r\n"
			"\r\n"
			"Hello!";

		write(client, response, strlen(response));
		close(client);
	}

	close(server);
	return 0;
}
*/
