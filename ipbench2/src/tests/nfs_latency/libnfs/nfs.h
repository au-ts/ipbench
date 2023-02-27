
#ifndef __NFS_H
#define __NFS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "nfsrpc.h"

extern int nfs_fd;
extern int nfs_nb_fd;

/* to initialise the lot */
int map_init(struct sockaddr_in * name);
int mnt_init(struct sockaddr_in * name);
int nfs_init(struct sockaddr_in * name);

/* And to clean up again */
void map_cleanup(void);
void mnt_cleanup(void);
void nfs_cleanup(void);

/* mount functions */
unsigned int mnt_get_export_list(void);

unsigned int mnt_mount(char *dir, struct cookie * pfh);


/* NFS functions */
int
nfs_getattr(struct cookie * fh,
	    void (*func) (uintptr_t, int, fattr_t *),
	    uintptr_t token);


	int nfs_lookup(struct cookie * cwd, char *name,
		  void (*func) (uintptr_t, int, struct cookie *, fattr_t *),
		        uintptr_t token);


	int nfs_create(struct cookie * fh, char *name, sattr_t * sat,
		  void (*func) (uintptr_t, int, struct cookie *, fattr_t *),
		        uintptr_t token);


	int nfs_read(struct cookie * fh, int pos, int count,
		 void (*func) (uintptr_t, int, fattr_t * attr, int, char *),
		      uintptr_t token);


	int nfs_write(struct cookie * fh, int offset, int count, void *data,
		       void (*func) (uintptr_t, int, fattr_t *),
		       uintptr_t token);


	int nfs_readdir(struct cookie * pfh, int cookie, int size,
	     void (*func) (uintptr_t, int, int, struct nfs_filename *, int),
			 uintptr_t token);


/* some error codes from nfsrpc */
#define ERR_OK            0
#define ERR_BAD_MSG      -1
#define ERR_NOT_ACCEPTED -2
#define ERR_FAILURE      -3
#define ERR_NOT_OK       -4
#define ERR_NOT_FOUND    -5
#define ERR_NEXT_AVAIL   -6

#endif				/* __NFS_H */
