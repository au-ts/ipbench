#include "latency.h"

/* the socket */
static int s;

static int
create_addr(char *hostname, uint16_t port, struct sockaddr_in *addr)
{
	struct hostent *he;
	extern int h_errno;

	if ((he = gethostbyname(hostname)) == NULL) {
		dbprintf("gethostbyname error %s\n", hstrerror(h_errno));
		return -1;
	}

	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = PF_INET;
	addr->sin_addr = *((struct in_addr *) he->h_addr);
	assert(addr->sin_addr.s_addr != INADDR_NONE);
	addr->sin_port = htons(port);
	assert(addr->sin_port != 0);

	return 0;
}


int tcp_setup_socket(char *hostname, int port, char *args)
{
	int c;
	struct sockaddr_in addr;
	if (create_addr(hostname, port, &addr) == -1) {
		dbprintf("Can not contact target!\n");
		return -1;
	}

	dbprintf("Socket type is TCP.\n");

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		dbprintf("Can not create socket (%s).\n", strerror(errno));
		return -1;
	}

	c = connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (c != 0) {
		dbprintf("Can not connect (%s).\n", strerror(errno));
		return -1;
	}
	return s;
}

int udp_setup_socket(char *hostname, int port, char *args)
{
	int c;
	struct sockaddr_in addr;
	if (create_addr(hostname, port, &addr) == -1) {
		dbprintf("Can not contact target!\n");
		return -1;
	}

	dbprintf("Socket type is UDP.\n");

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		dbprintf("Can not create socket (%s).\n", strerror(errno));
		return -1;
	}

	c = connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (c != 0) {
		dbprintf("Can not connect (%s).\n", strerror(errno));
		return -1;
	}
	return s;
}


/* ip based packet sending functions */
int ip_setup_packet(char **buf, int size)
{
	*buf = malloc(size);
	assert (buf != NULL);
	return 0;
}

int ip_fill_packet(void *buf, uint64_t seq)
{
	*((uint64_t*)buf) = seq;
	return 0;
}

int ip_read_packet(void *buf, uint64_t *seq)
{
	*seq = *((uint64_t*)buf);
	return 0;
}

int ip_send_packet(void *buf, int size, int flags)
{
	return send(s, buf, size, flags);
}

int ip_recv_packet(void *buf, int size, int flags)
{
	return recv(s, buf, size, flags);
}
