#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "lib/util.h"
#include "nfs.h"
#include "rpc.h"
#include "xdr.h"


#define PORTMAP_TCP 0

int map_fd;

int
map_init(struct sockaddr_in *name){
	struct sockaddr_in s;

	dbprintf("map_init(): called\n");

	memcpy(&s, name, sizeof(struct sockaddr_in));

	if((map_fd = socket(PF_INET,
#if PORTMAP_TCP
	                    SOCK_STREAM,
#else
	                    SOCK_DGRAM,
#endif
	                    0)) < 0){
		perror("map_init(): error creating socket");
		return -1;
	}

	s.sin_port = htons(PMAP_PORT);

	if(connect(map_fd, (struct sockaddr*)&s,
	           sizeof(struct sockaddr_in)) < 0){
		perror("map_init(): error connecting socket");
		close(map_fd);
		return -1;
	}

        /* make nfs_fd non-blocking */
        assert(fcntl(map_fd, F_SETFL, O_NONBLOCK)!=-1);

	dbprintf("map_init(): done\n");

	return 0;
}

unsigned int   
map_getport(mapping_t *pmap)
{
	int port;
	struct pbuf pbuf, ret;
	
	dbprintf("map_getport(): called\n");
	
	pmap->port = 0;
	
	/* add the xid */ 
	initbuf(&pbuf, PMAP_NUMBER, PMAP_VERSION, PMAPPROC_GETPORT);
	
	/* pack up the map struct */ 
	addtobuf(&pbuf, (char *)pmap, sizeof(mapping_t), 1);

	dbprintf("map_getport(): doing rpc_call()\n");
	
	/* make the call */ 
	assert(rpc_call(&pbuf, &ret, map_fd)==0);

	dbprintf("map_getport(): rpc_call() returned\n");
	
	/* now we can extract the port */ 
	getfrombuf(&ret, (char *)&port, sizeof(port), 1);
	
	pmap->port = port;
	
	dbprintf("map_getport(): got port %d\n", port);
	
	return 0;
}

void
map_cleanup(void)
{
	close(map_fd);
}
