#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#include "lib/util.h"
#include "nfs.h"
#include "rpc.h"
#include "xdr.h"

#define MOUNT_TCP 0

int mount_fd;

int
mnt_init(struct sockaddr_in *name){
	struct sockaddr_in s;
	mapping_t map;
	uint16_t port;

	dbprintf("mnt_init(): called\n");
	
	/* make RPC to get mountd info */
        map.prog = MNT_NUMBER;
        map.vers = MNT_VERSION;
	map.prot =
#if MOUNT_TCP
	IPPROTO_TCP;
#else
        IPPROTO_UDP;
#endif
	if (map_getport(&map) == 0) {
		dbprintf("mountd port number is %d\n", map.port);
		port = map.port;
		if (port == 0) {
			dbprintf("mnt_init(): mount port invalid\n");
			return 1;
		} 
	} else {
		dbprintf("mnt_init(): error getting mountd port number\n");
		return 1;
	}

	/* set up the mount socket */
	memcpy(&s, name, sizeof(struct sockaddr_in));

	if((mount_fd = socket(PF_INET,
#if MOUNT_TCP
	                      SOCK_STREAM,
#else
	                      SOCK_DGRAM,
#endif
	                      0)) < 0){
		perror("mnt_init(): error creating socket: ");
		return -1;
	}

	dbprintf("mnt_init(): connecting to port %d\n", port);

	s.sin_port = htons(port);

	if(connect(mount_fd, (struct sockaddr*)&s,
	           sizeof(struct sockaddr_in)) < 0){
		perror("mnt_init(): error connecting socket");
		close(mount_fd);
		return -1;
        }  

        /* make mount_fd non-blocking */
        assert(fcntl(mount_fd, F_SETFL, O_NONBLOCK)!=-1);

	dbprintf("mnt_init(): done");

	return 0;
}

unsigned int   
mnt_get_export_list(void)
{
	struct pbuf pbuf, ret;
	char str[100];
	int opt;
	
	initbuf(&pbuf, MNT_NUMBER, MNT_VERSION, MNTPROC_EXPORT);
	
	assert(rpc_call(&pbuf, &ret, mount_fd)==0);
	
	while (getfrombuf(&ret, (char *)&opt, sizeof(opt), 1), opt) {
		dbprintf("NFS Export...\n");
		getstring(&ret, str, 100);
		dbprintf("* Export name is %s\n", (char *)&str);
		
		/* now to extract more stuff... */ 
		while (getfrombuf(&ret, (char *)&opt, sizeof(opt), 1), opt) {
			getstring(&ret, str, 100);
			dbprintf("* Group %s\n", (char *)str);
		} 
	} 
	return 0;
} 

unsigned int   
mnt_mount(char *dir, struct cookie * pfh)
{
	struct pbuf pbuf, ret;
	int status;

	dbprintf("mnt_mount(): mounting \"%s\"\n", dir);

	initbuf(&pbuf, MNT_NUMBER, MNT_VERSION, MNTPROC_MNT);
	
	addstring(&pbuf, dir);
	
	assert(rpc_call(&pbuf, &ret, mount_fd)==0);
	
	/* now we do some stuff :) */ 
	getfrombuf(&ret, (char *)&status, sizeof(status), 1);

	if (status != 0) {
		dbprintf("mnt_mount(): could not mount \"%s\": %d\n", dir, status);
		return status;	
	}

	getfrombuf(&ret, (char *)pfh, sizeof(struct cookie), 1);

	dbprintf("mnt_mount(): \"%s\" has been mounted\n", dir);

	return 0;
}

void
mnt_cleanup(void)
{
	close(mount_fd);
}
