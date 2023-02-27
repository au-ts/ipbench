#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "nfs.h"
#include "rpc.h"
#include "callback.h"

int finished = 0;

void print_usage(){
	printf("wrong arguments!\n");
}

int resolve(char *name, struct sockaddr_in *sock){
	struct hostent *hp;

	if((hp = gethostbyname(name))==NULL)
		return -1;

	memset(sock, 0, sizeof(struct sockaddr_in));
	memcpy(&sock->sin_addr, hp->h_addr, hp->h_length);

	assert(hp->h_addrtype == PF_INET);

	sock->sin_family = PF_INET;
	sock->sin_port = htons(0);

	return 0;
}

#define READ_SIZE 1024
#define READ_COUNT 4096


static void
read_cb(uintptr_t token, int status, fattr_t *pattrs, int size, char *data){
	static uintptr_t count = 0;
	static int total = 0;

	assert(status==NFS_OK);

	total += size;

	printf("read %d:%d bytes\n", (int)token, total);

	if(++count==READ_COUNT){
		printf("finished reading %d bytes!\n", total);
		finished=1;
	}
}

static void
open_cb(uintptr_t token, int status, struct cookie *fh, fattr_t *pattrs){
	static int idem = 0;
	int i;
	uint8_t data[READ_SIZE];

	bzero(data, READ_SIZE);

	printf("open_cb() called!\n");

	assert(idem==0);
	idem = 1;

	assert(status==NFS_OK);

	printf("uid=%d, gid=%d, size=%d\n",
	       pattrs->uid, pattrs->uid, pattrs->size);

	i=0;
	while(!finished){
		struct pbuf buf;
		nfs_read(fh, (i%READ_COUNT)*READ_SIZE, READ_SIZE, read_cb, i);

		/* nonblocking recv prevents massive queue buildup */
		rpc_recv(&buf, nfs_fd, 0);

		i++;
	}

	printf("Performed %d requests to get %d blocks\n", i, READ_COUNT);
	printf("lost %d%% of requests\n", 100-((100*READ_COUNT)/i));
}

static void
runtest(){
	struct cookie pfh;
	uintptr_t token = 0;

	mnt_get_export_list();

	mnt_mount("/tmp", &pfh);

	nfs_lookup(&pfh, "bench.file", open_cb, token);
}

int main(int argc, char *argv[]){
	struct sockaddr_in addr;

	if(argc<2){
		print_usage();
		exit(-1);
	}

	if(resolve(argv[1], &addr) < 0){
		printf("error resolving \"%s\"\n", argv[1]);
		exit(-1);
	}

	callback_init(10,100000);

	map_init(&addr);
	mnt_init(&addr);
	nfs_init(&addr);

	runtest();

	while(!finished){
		struct pbuf buf;

		printf("receive loop\n");

		rpc_recv(&buf, nfs_fd, -1);
	}

	return 0;
}
