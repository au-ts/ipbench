#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#include "plugin.h"
#include "nfs.h"
#include "rpc.h"
#include "xdr.h"

int nfs_fd;

static int check_errors(struct pbuf * pbuf);

/* this should be called once at beginning to setup everything */
/*
 * XXX this code should be abstracted out, as it is almost identical to any
 * other portmap connection (see mnt_init)
 */
int
nfs_init(struct sockaddr_in * name)
{
	struct sockaddr_in s;
	mapping_t map;
	uint16_t port = 0;

	/* make RPC to get nfs info */
	map.prog = NFS_NUMBER;
	map.vers = NFS_VERSION;
	map.prot = IPPROTO_UDP;

	if (map_getport(&map) == 0) {
		port = map.port;
		dbprintf("nfs port number is %d\n", port);
	} else {
		if (port == 0) {
			printf("Invalid NFS port\n");
			return 1;
		}
		dbprintf("Error getting NFS port number\n");
		return 1;
	}

	/* set up the mount socket */
	memcpy(&s, name, sizeof(struct sockaddr_in));

	if ((nfs_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
	        dbprintf("socket failed: %s\n", strerror(errno));
		return -1;
	}
	s.sin_port = htons(port);

	if (connect(nfs_fd, (struct sockaddr *) & s,
		    sizeof(struct sockaddr_in)) < 0) {
	        dbprintf("connect failed: %s\n", strerror(errno));

		close(nfs_fd);
		return -1;
	}

	/* make nfs_fd non-blocking */
	assert(fcntl(nfs_fd, F_SETFL, O_NONBLOCK)!=-1);

	return 0;
}

/******************************************
 * Async functions
 ******************************************/

void
nfs_getattr_cb(struct callback *c, struct pbuf *pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1;
	fattr_t pattrs;

	void (*cb) (uintptr_t, int, fattr_t *)= callback;

	assert(callback != NULL);

	err = check_errors(pbuf);

	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);

		if (status == NFS_OK) {
			/* it worked, so take out the return stuff! */
			getfrombuf(pbuf, (void *)&pattrs,
				   sizeof(fattr_t), 1);
		}
	}

	cb(token, status, &pattrs);

	return;

}

int
nfs_getattr(struct cookie * fh,
	    void (*func) (uintptr_t, int, fattr_t *),
	    uintptr_t token)
{
	struct pbuf pbuf;
	struct callback c;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_GETATTR);

	/* put in the fhandle */
	addtobuf(&pbuf, (char *)fh, sizeof(struct cookie), 1);

	/* send it! */
	c = create_nfs_callback(nfs_getattr_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

void
nfs_lookup_cb(struct callback *c, struct pbuf * pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1;
	struct cookie new_fh;
	fattr_t pattrs;
	void (*cb) (uintptr_t, int, struct cookie *, fattr_t *)= callback;

	assert(callback != NULL);

	err = check_errors(pbuf);
	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);

		if (status == NFS_OK) {
			/* it worked, so take out the return stuff! */
			getfrombuf(pbuf, (void *)&new_fh,
				   sizeof(struct cookie), 1);

			getfrombuf(pbuf, (void *)&pattrs,
				   sizeof(fattr_t), 1);
		}
	}

	cb(token, status, &new_fh, &pattrs);

	return;
}

/* request a file handle */
int
nfs_lookup(struct cookie * cwd, char *name,
	   void (*func) (uintptr_t, int, struct cookie *, fattr_t *),
	   uintptr_t token)
{
	struct pbuf pbuf;
	struct callback c;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_LOOKUP);

	/* put in the fhandle */
	addtobuf(&pbuf, (char *)cwd, sizeof(struct cookie), 1);

	/* put in the name */
	addstring(&pbuf, name);

	/* send it! */
	c = create_nfs_callback(nfs_lookup_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

void
nfs_read_cb(struct callback *c, struct pbuf *pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1;
	fattr_t pattrs;
	char *data = NULL;
	int size = 0;
	void (*cb) (uintptr_t, int, fattr_t *, int, char *)= callback;

	err = check_errors(pbuf);

	assert(callback != NULL);

	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);


		if (status == NFS_OK) {

			/* it worked, so take out the return stuff! */
			getfrombuf(pbuf, (void *)&pattrs,
				   sizeof(fattr_t), 1);

			getfrombuf(pbuf, (void *)&size,
				   sizeof(int), 1);

			data = getpointfrombuf(pbuf, pattrs.size);
		}
	}

	cb(token, status, &pattrs, size, data);

	return;
}

int
nfs_read(struct cookie * fh, int pos, int count,
	 void (*func) (uintptr_t, int, fattr_t *, int, char *),
	 uintptr_t token)
{

	struct pbuf pbuf;
	struct callback c;
	readargs_t args;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_READ);


	/* copy in the fhandle */
	memcpy(&args.file, (char *)fh, sizeof(struct cookie));

	args.offset = pos;
	args.count = count;
	args.totalcount = 0;	/* unused as per RFC */

	/* add them to the buffer */
	addtobuf(&pbuf, (char *)&args, sizeof(args), 1);

	c = create_nfs_callback(nfs_read_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

void
nfs_write_cb(struct callback *c, struct pbuf *pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1;
	fattr_t pattrs;
	void (*cb) (uintptr_t, int, fattr_t *)= callback;

	err = check_errors(pbuf);

	assert(callback != NULL);

	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);

		if (status == NFS_OK) {
			/* it worked, so take out the return stuff! */
			getfrombuf(pbuf, (void *)&pattrs,
				   sizeof(fattr_t), 1);
		}
	}

	cb(token, status, &pattrs);

	return;
}

int
nfs_write(struct cookie * fh, int offset, int count, void *data,
	  void (*func) (uintptr_t, int, fattr_t *),
	  uintptr_t token)
{
	struct pbuf pbuf;
	struct callback c;
	writeargs_t args;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_WRITE);

	/* copy in the fhandle */
	memcpy(&args.file, (char *)fh, sizeof(struct cookie));

	args.offset = offset;
	args.beginoffset = 0;	/* unused as per RFC */
	args.totalcount = 0;	/* unused as per RFC */

	/* add them to the buffer */
	addtobuf(&pbuf, (char *)&args, sizeof(args), 1);

	/* put the data in */
	adddata(&pbuf, data, count, 0);

	c = create_nfs_callback(nfs_write_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

void
nfs_create_cb(struct callback *c, struct pbuf *pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1;
	struct cookie new_fh;
	fattr_t pattrs;
	void (*cb) (uintptr_t, int, struct cookie *, fattr_t *)= callback;

	assert(callback != NULL);

	err = check_errors(pbuf);

	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);

		if (status == NFS_OK) {

			/* it worked, so take out the return stuff! */
			getfrombuf(pbuf, (void *)&new_fh,
				   sizeof(struct cookie), 1);

			getfrombuf(pbuf, (void *)&pattrs,
				   sizeof(fattr_t), 1);

		}
	}
	dbprintf("NFS CREATE CALLBACK\n");

	cb(token, status, &new_fh, &pattrs);

	return;
}

int
nfs_create(struct cookie * fh, char *name, sattr_t * sat,
	   void (*func) (uintptr_t, int, struct cookie *, fattr_t *),
	   uintptr_t token)
{
	struct pbuf pbuf;
	struct callback c;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_CREATE);

	/* put in the fhandle */
	addtobuf(&pbuf, (char *)fh, sizeof(struct cookie), 1);

	/* put in the name */
	addstring(&pbuf, name);
	addtobuf(&pbuf, (char *)sat, sizeof(sattr_t), 1);

	c = create_nfs_callback(nfs_create_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

int
getentries_readdir(struct pbuf * pbuf, int *cookie)
{
	int old_pos = pbuf->pos;
	int tmp = 1;
	int count = 0;
	int fileid;

	getfrombuf(pbuf, (char *)&tmp, sizeof(tmp), 1);

	dbprintf("Got entry: %d\n", tmp);

	while (tmp) {
		getfrombuf(pbuf, (char *)&fileid, sizeof(fileid), 1);
		skipstring(pbuf);
		dbprintf("Skipped string\n");
		getfrombuf(pbuf, (char *)cookie, sizeof(int), 1);
		dbprintf("Skipped string %d\n", *cookie);
		count++;
		getfrombuf(pbuf, (char *)&tmp, sizeof(tmp), 1);
	}

	getfrombuf(pbuf, (char *)&tmp, sizeof(tmp), 1);

	if (tmp == 0)
		cookie = 0;

	pbuf->pos = old_pos;

	dbprintf("Returning: %d\n", count);

	return count;
}

void
nfs_readdir_cb(struct callback *c, struct pbuf *pbuf)
{
	void *callback = c->param.nfs.cb;
	uintptr_t token = c->param.nfs.arg;
	int err = 0, status = -1, num_entries = 0, next_cookie = 0;
	struct nfs_filename *entries = NULL;
	void (*cb) (uintptr_t, int, int, struct nfs_filename *, int)= callback;
	int count = 0;

	dbprintf("NFS READDIR CALLBACK\n");
	assert(callback != NULL);

	err = check_errors(pbuf);

	if (err == 0) {
		/* get the status out */
		getfrombuf(pbuf, (char *)&status, sizeof(status), 1);

		if (status == NFS_OK) {
			int tmp, fileid, cookie;
			dbprintf("Getting entries\n");

			num_entries = getentries_readdir(pbuf, &next_cookie);
			entries = malloc(sizeof(struct nfs_filename) *
					 num_entries);

			getfrombuf(pbuf, (char *)&tmp, sizeof(tmp), 1);
			dbprintf("Got entry: %d\n", tmp);

			while (tmp) {
				int size;
				getfrombuf(pbuf, (char *)&fileid,
					   sizeof(fileid), 1);

				dbprintf("Got filed: %d\n", fileid);

				getfrombuf(pbuf, (char *)&entries[count].size,
					   sizeof(int), 1);

				dbprintf("Got size: %d\n", entries[count].size);

				entries[count].file = &pbuf->buf[pbuf->pos];

				size = entries[count].size;
				if (size % 4)
					size += 4 - (size % 4);

				pbuf->pos += size;
				dbprintf("Got size: %d\n", pbuf->pos);

				getfrombuf(pbuf, (char *)&cookie,
				           sizeof(int), 1);
				count++;
				getfrombuf(pbuf, (char *)&tmp,
				           sizeof(tmp), 1);
			}
		}
	}
	cb(token, status, num_entries, entries, next_cookie);
	return;
}

/* send a request for a directory item */
int
nfs_readdir(struct cookie * pfh, int cookie, int size,
	    void (*func) (uintptr_t, int, int, struct nfs_filename *, int),
	    uintptr_t token)
{
	struct pbuf pbuf;
	struct callback c;
	readdirargs_t args;

	/* now the user data struct is setup, do some call stuff! */
	initbuf(&pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_READDIR);

	/* copy the buffer */
	memcpy(&args.dir, pfh, sizeof(struct cookie));

	/* set the cookie */
	args.cookie = cookie;
	args.count = size;

	/* copy the arguments into the packet */
	addtobuf(&pbuf, (char *)&args, sizeof(args), 1);

	/* make the call! */
	c = create_nfs_callback(nfs_readdir_cb, func, token);
	return rpc_send(&pbuf, nfs_fd, &c);
}

/********************************************
 * Data extraction functions
 ********************************************/

static int
check_errors(struct pbuf * pbuf)
{
	int txid, ctype;
	opaque_auth_t auth;
	int r;

	/* extract the xid */
	getfrombuf(pbuf, (char *)&txid, sizeof(txid), 1);

	/* and the call type */
	getfrombuf(pbuf, (char *)&ctype, sizeof(ctype), 1);

	if (ctype != MSG_REPLY) {
		dbprintf("Got a reply to something else!!\n");
		dbprintf("Looking for msgtype %d\n", MSG_REPLY);
		dbprintf("Got msgtype %d\n", ctype);
		return ERR_BAD_MSG;
	}

	/* check if it was an accepted reply */
	getfrombuf(pbuf, (char *)&r, sizeof(r), 1);
	if (r != MSG_ACCEPTED) {
		dbprintf("Message NOT accepted (%d)\n", r);

		/* extract error code */
		getfrombuf(pbuf, (char *)&r, sizeof(r), 1);
		dbprintf("Error code %d\n", r);

		if (r == 1) {
			/* get the auth problem */
			getfrombuf(pbuf, (char *)&r, sizeof(r), 1);
			dbprintf("auth_stat %d\n", r);
		}
		return ERR_NOT_ACCEPTED;
	}

	/* and the auth data! */
	getfrombuf(pbuf, (char *)&auth, sizeof(auth), 1);

	if (auth.flavour != AUTH_NULL)
		assert("gave back other auth type!\n");

	/* check its accept stat */
	getfrombuf(pbuf, (char *)&r, sizeof(r), 1);

	if (r == SUCCESS) {
		return 0;
	}
	dbprintf("reply stat was %d\n", r);
	return ERR_FAILURE;
}

void
nfs_cleanup(void)
{
	close(nfs_fd);
}
