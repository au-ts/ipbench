/* 
   dbench version 2
   Copyright (C) Andrew Tridgell 1999
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "dbench.h"

static void server(int fd)
{
	char buf[70000];
	unsigned *ibuf = (unsigned *)buf;
	int n;

	signal(SIGPIPE, SIG_IGN);
	
	vbprintf("[Client started]\n");

	while (1) {
		if (read_sock(fd, buf, 4) != 4) break;
		n = ntohl(ibuf[0]);
		if (n+4 >= sizeof(buf)) {
			vbprintf("overflow in server!\n");
			exit(1);
		}
		if (read_sock(fd, buf+4, n) != n) break;
		n = ntohl(ibuf[1]);
		ibuf[0] = htonl(n);
		if (write_sock(fd, buf, n+4) != n+4) break;
	}

	exit(0);
}

static void listener(void)
{
	int sock;

	sock = open_socket_in(SOCK_STREAM, TCP_PORT);

	if (listen(sock, 20) == -1)
		errprintf(1, "listen failed %s\n", strerror(errno));

	vbprintf("Waiting for connections\n");

	signal(SIGCHLD, SIG_IGN);

	while (1) {
		struct sockaddr addr;
		int in_addrlen = sizeof(addr);
		int fd;

		while (waitpid((pid_t)-1,(int *)NULL, WNOHANG) > 0) ;

		fd = accept(sock,&addr,&in_addrlen);

		if (fd != -1) {
			if (fork() == 0) server(fd);
			close(fd);
		}
	}
}

static pid_t pid;
static int running = 1;

/* setup server child */
int tbench_target_setup(char* arg) { 
	pid = fork();
	if ( pid == -1 )
		errprintf(1, "Failed to fork child : %s\n", strerror(errno));

	if ( pid == 0 ) 
	{
		dbprintf("Forked listner child (%d)\n", getpid());
		listener();
	}
	else  
	{
		/* just give a few seconds to settle */
		sleep(2);  
		dbprintf("Setup Complete");
	}

	return 0;
};


/* setup and run until tbench_target_stop() is called */
int tbench_target_start(struct timeval *start) { 
	gettimeofday(start, NULL); 
	/* a bit of a hack, this should sleep properly */
	while (running)
		sleep(2);

	dbprintf("Test complete!\n");
	return 0;
}

/* signal we have stopped and kill the test child */
int tbench_target_stop(struct timeval *stop) { 
	running = 0;
	kill(pid, SIGKILL);
	gettimeofday(stop, NULL); 
	dbprintf("Stopping test\n");
	return 0;  
}

/* really just dummies below, we don't return anything */
int tbench_target_marshall(void **data, size_t *size, double running_time) 
{ 
	*data = NULL; 
	*size = 0; 
	return 0; 
}

void tbench_target_marshall_cleanup(void **data) { 
	return; 
}

int tbench_target_unmarshall(void *input, size_t input_len, void **data, size_t *data_len) { 
	*data = NULL; 
	*data_len = 0; 
	return 0; 
}

void tbench_target_unmarshall_cleanup(void **data) { 
	return;  
}
