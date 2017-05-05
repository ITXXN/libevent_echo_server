#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <event.h>
#include <string.h>

#define handle_error(msg) \
	do {perror(msg); exit(EXIT_FAILURE);} while(0);
#define IPADDR "127.0.0.1"
#define PORT 9999
#define BUFSIZE 1024
#define LISTENSIZE 5

struct event_base *base;
struct socket_event
{
	struct event *read_ev;
	struct event *wirte_ev;
	char buffer[BUFSIZE];
};

int server_socket_init(void);
void evevt_base_init(void);
int start_handle(int sock);
void handle_accept(int sock, short event, void *arg);
void handle_read(int sock, short event, void *arg);
void handle_write(int sock, short event, void *arg);
void free_socket_event(struct socket_event *ev);

int main()
{
	int sock;

	sock = server_socket_init();
	evevt_base_init();
	start_handle(sock);

	return 0;
}

int server_socket_init(void)
{
	int sock;
	int ret = 0;
	int on = 1;
	struct sockaddr_in server_addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
		handle_error("create server socket error.\n");
	}
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
	if (-1 == ret) {
		handle_error("set socket addr reuse error.\n");
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, IPADDR, &server_addr.sin_addr);
	server_addr.sin_port = htons(PORT);

	ret = bind(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
	if (-1 == ret) {
		handle_error("bind error.\n");
	}

	ret = listen(sock, LISTENSIZE);
	if (-1 == ret) {
		handle_error("listen error.\n");
	}

	return sock;
}

void evevt_base_init(void)
{
	base = event_base_new();
}

int start_handle(int sock)
{
	struct event listen_ev;
	event_set(&listen_ev, sock, EV_READ|EV_PERSIST, handle_accept, NULL);
	event_base_set(base, &listen_ev);
	event_add(&listen_ev, NULL);
	event_base_dispatch(base);

	return 0;
}

void handle_accept(int sock, short event, void *arg)
{
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len;
	int cli_fd;
	struct socket_event *ev;

	ev = (struct socket_event*)malloc(sizeof(struct socket_event));
	ev->read_ev = (struct event*)malloc(sizeof(struct event));
	ev->wirte_ev = (struct event*)malloc(sizeof(struct event));

	cli_addr_len = sizeof(struct sockaddr_in);

	cli_fd = accept(sock, (struct sockaddr*)&cli_addr, &cli_addr_len);
	if (-1 == cli_fd) {
		printf("accept error.\n");
		return ;
	}
	printf("accept ip:%s, port:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

	event_set(ev->read_ev, cli_fd, EV_READ|EV_PERSIST, handle_read, ev);
	event_base_set(base, ev->read_ev);
	event_add(ev->read_ev, NULL);
}

void handle_read(int sock, short event, void *arg)
{
	struct socket_event *ev;
	int size;

	ev = (struct socket_event*)arg;
	bzero(ev->buffer, sizeof(ev->buffer));

	//size = recv(sock, ev->buffer, sizeof(ev->buffer), 0);
	size = read(sock, ev->buffer, sizeof(ev->buffer));
	if (0 == size) {
		printf("client %d closed.\n", sock);
		free_socket_event(ev);
		close(sock);
		return;
	}
	printf("receive data: %s, size: %d\n", ev->buffer, size);

	event_set(ev->wirte_ev, sock, EV_WRITE, handle_write, ev->buffer);
	event_base_set(base, ev->wirte_ev);
	event_add(ev->wirte_ev, NULL);
}

void free_socket_event(struct socket_event *ev)
{
	event_del(ev->read_ev);
	free(ev->read_ev);
	free(ev->wirte_ev);
	free(ev);
}

void handle_write(int sock, short event, void *arg)
{
	char *buffer;

	buffer = (char *)arg;
	send(sock, buffer, strlen(buffer), 0);
}
