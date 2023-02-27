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

/****************************************************************************
open a socket of the specified type, port and address for incoming data
****************************************************************************/
int open_socket_in(int type, int port)
{
	struct sockaddr_in sock;
	int res;
	int one=1;
	extern char *tcp_options;

	memset((char *)&sock,0, sizeof(sock));
	sock.sin_port = htons(port);
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = 0;
	res = socket(AF_INET, type, 0);
	if (res == -1) { 
		fprintf(stderr, "socket failed\n"); return -1; 
	}

	setsockopt(res,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one));

	/* now we've got a socket - we need to bind it */
	if (bind(res, (struct sockaddr * ) &sock,sizeof(sock)) < 0) { 
		fprintf(stderr, "bind failed %s\n", strerror(errno));
		return(-1); 
	}

	set_socket_options(res, tcp_options);

	return res;
}


/* open a socket to a tcp remote host with the specified port 
   based on code from Warren */
int open_socket_out(char *host, int port)
{
	int type = SOCK_STREAM;
	struct sockaddr_in sock_out;
	int res;
	struct hostent *hp;  
	extern char *tcp_options;

	res = socket(PF_INET, type, 0);
	if (res == -1) {
		return -1;
	}

	hp = gethostbyname(host);
	if (!hp) {
		fprintf(stderr,"unknown host: %s\n", host);
		return -1;
	}

	memcpy(&sock_out.sin_addr, hp->h_addr, hp->h_length);
	sock_out.sin_port = htons(port);
	sock_out.sin_family = PF_INET;

	set_socket_options(res, tcp_options);

	if (connect(res,(struct sockaddr *)&sock_out,sizeof(sock_out))) {
		close(res);
		fprintf(stderr,"failed to connect to %s - %s\n", 
			host, strerror(errno));
		return -1;
	}

	return res;
}



int read_sock(int s, char *buf, int size)
{
	int total=0;

	while (size) {
		int r = recv(s, buf, size, MSG_WAITALL);
		if (r <= 0) {
			if (r == -1) perror("recv");
			break;
		}
		buf += r;
		size -= r;
		total += r;
	}
	return total;
}

int write_sock(int s, char *buf, int size)
{
	int total=0;

	while (size) {
		int r = send(s, buf, size, 0);
		if (r <= 0) {
			if (r == -1) perror("send");
			break;
		}
		buf += r;
		size -= r;
		total += r;
	}
	return total;
}
