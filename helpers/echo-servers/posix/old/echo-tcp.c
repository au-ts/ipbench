#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define CONNECTION_BACKLOG 32

void
do_echo(int sock, uint32_t message_size){
	int r, s;
	char *buffer;

	buffer = malloc(message_size);
	assert(buffer!=NULL);

	for(;;){
		r = recv(sock, buffer, message_size, MSG_WAITALL);
		if(r!=message_size){
			return;
		}

		s = send(sock, buffer, message_size, 0);
		if(s!=message_size){
			return;
		}
	}
}

int
main(int argc, char *argv[]){
	int s, b, l, a;
	struct sockaddr_in anyaddr;
	struct sockaddr addr;
	socklen_t addrlen;
	pid_t f;
	uint16_t port;
	uint32_t message_size;

	/* parse the options */
	if(argc!=3){
		printf("usage: echo <port> <message size in bytes>\n");
		return -1;
	}

	port = atoi(argv[1]);
	message_size = atoi(argv[2]);

	assert(port>0);
	assert(message_size>0);

	printf("using port %d, message size %d\n", port, message_size);

	/* set up wildcard address with desired port */
	memset(&anyaddr, 0, sizeof(struct sockaddr_in));
	anyaddr.sin_family = PF_INET;
	anyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	anyaddr.sin_port = htons(port);

	s = socket(PF_INET, SOCK_STREAM, 0);
	assert(s>=0);

	b = bind(s, (struct sockaddr*)&anyaddr, sizeof(struct sockaddr_in));
	assert(b==0);

	l = listen(s, CONNECTION_BACKLOG);
	assert(l==0);

	for(;;){
		a = accept(s, &addr, &addrlen);
		assert(a>=0);

		f = fork();
		assert(f>=0);

		if(f==0){
			/* child */
			close(s);
			do_echo(a, message_size);
			close(a);
			exit(0);
		}else{
			close(a);
		}
	}
}
