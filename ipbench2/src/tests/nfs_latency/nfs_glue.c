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

#include "nfs_latency.h" /* for microuptime stuff */

#include "libnfs/nfs.h"
#include "libnfs/rpc.h"
#include "libnfs/xdr.h"
#include "libnfs/callback.h"

static int initialised = 0;
static struct cookie nfs_file_handle;

#define READ_OFFSET 0
#define READ_SIZE 1024

struct read_request{
	struct pbuf pbuf;
};

struct write_request{
	struct pbuf pbuf;
	uint8_t data[READ_SIZE];
};

struct nfs_request{
	void (*callback)(struct callback *c, struct pbuf *pbuf);
	union{
		struct read_request r;
		struct write_request w;
	} *request;
} *nfs_cache = NULL;

uint64_t last_sample = 0;

static void
read_callback(struct callback *c, struct pbuf *pbuf){
	assert(c->func==read_callback);
	assert(last_sample==0);

	/* XXX we should really check the NFS error code here */

	last_sample = c->param.time.timestamp;
}

/*
 * instead of using nfs_read(), we generate ourselves a read with a 
 * special timing callback.
 */
static struct nfs_request
create_read_request(){
	struct nfs_request rr;
	readargs_t args;

	/* initialise read request */
	rr.request = malloc(sizeof(struct read_request));
	assert(rr.request!=NULL);

	/* set up buffer */
	initbuf(&rr.request->r.pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_READ);

	args.file = nfs_file_handle;
	args.offset = READ_OFFSET;
	args.count = READ_SIZE;
	args.totalcount = 0; /* unused as per RFC */

	addtobuf(&rr.request->r.pbuf, &args, sizeof(args), 1);

	rr.callback = read_callback;

	return rr;
}

static void
write_callback(struct callback *c, struct pbuf *pbuf){
	assert(c->func==write_callback);
	assert(last_sample==0);

	/* XXX we should really check the NFS error code here */

	last_sample = c->param.time.timestamp;
}

/*
 * instead of using nfs_write(), we generate ourselves a read with a 
 * special timing callback.
 */
static struct nfs_request
create_write_request(){
	struct nfs_request wr;
	int i;
	writeargs_t args;

	/* initialise write request */
	wr.request = malloc(sizeof(struct write_request));
	assert(wr.request!=NULL);


	for(i=0; i<READ_SIZE; i++){
		wr.request->w.data[i] = (uint8_t)(random() & 0xFF);
	}

	/* set up buffer */
	initbuf(&wr.request->w.pbuf, NFS_NUMBER, NFS_VERSION, NFSPROC_WRITE);

	args.file = nfs_file_handle;
	args.offset = READ_OFFSET;
	args.beginoffset = 0; /* unused as per RFC */
	args.totalcount = 0;  /* unused as per RFC */

	addtobuf(&wr.request->w.pbuf, &args, sizeof(args), 1);
	/* XXX why is data sticking around once encoded? */
	adddata(&wr.request->w.pbuf, wr.request->w.data, READ_SIZE, 0);

	wr.callback = write_callback;

	return wr;
}

int
initialise_cache(){
	assert(nfs_cache==NULL);

	nfs_cache = malloc(sizeof(struct nfs_request)*2);
	assert(nfs_cache!=NULL);

	nfs_cache[0] = create_read_request();
	nfs_cache[1] = create_write_request();

	return 0;
}

/*
 * In the future this can be used to generate a varied workload.
 */
int
generate_request(uint64_t timestamp){
	struct callback c;
	int i;

	assert(initialised==1);

	/* pick a random cache entry */
	//i = 0;
	i = random()&1;

	/* set a new xid */
	reset_xid(nfs_cache[i].request->r.pbuf.buf);

	/* set up callback */
	c.func = nfs_cache[i].callback;
	c.param.time.timestamp = timestamp;

	return rpc_send(&nfs_cache[i].request->r.pbuf, nfs_fd, &c);
}

int
process_reply(uint64_t *timestamp){
	struct pbuf pbuf;

	assert(initialised==1);
	last_sample=0;

	/* do a non-blocking recv */
	rpc_recv(&pbuf, nfs_fd, 0);

	if(last_sample==0)
		return -1;

	*timestamp = last_sample;

	return 0;
}

int resolve(char *name, struct sockaddr_in *sock){
	struct hostent *hp;

	if ((hp = gethostbyname(name)) == NULL) {
		dbprintf("Cannot resolve %s\n", name);
		return -1;
	}

	memset(sock, 0, sizeof(struct sockaddr_in));
	memcpy(&sock->sin_addr, hp->h_addr, hp->h_length);

	assert(hp->h_addrtype == PF_INET);

	sock->sin_family = PF_INET;
	sock->sin_port = htons(0);

	return 0;
}

static void
open_cb(uintptr_t token, int status, struct cookie *fh, fattr_t *pattrs){
	static int idem = 0;

	assert(idem==0);
	idem = 1;

	if (status == NFS_OK){
		nfs_file_handle = *fh;
		initialised = 1;
	} else {
		initialised = -1;
	}

	return;
}


struct lookup_dir {
	char *tail;
	char *linkname;
	struct cookie cwd;
	int ok;
};

static void
dir_cb(uintptr_t token, int status, struct cookie *fh, fattr_t *pattrs)
{
	struct lookup_dir *ldp = (struct lookup_dir *)token;

	if (status == NFS_OK) {
		ldp->cwd = *fh;
		ldp->ok = 1;
	} else {
		dbprintf("Directory %s open failed: %d\n",
			ldp->linkname, status);
		ldp->ok = -1;
	}
}


void
recursive_lookup(struct cookie *cwd, char *name, 
	   void (*func) (uintptr_t, int, struct cookie *, fattr_t *),
	   uintptr_t token)
{
	struct lookup_dir ld;
	char *slashp;
	char *s;
	s = ld.tail = strdup(name);
	ld.cwd = *cwd;

	do {
		ld.ok = 0;
		ld.linkname = ld.tail;
		while (*ld.linkname == '/')
			ld.linkname++;
		slashp = strchr(ld.linkname, '/');
		if (slashp) {
			*slashp = '\0';
			ld.tail = slashp + 1;
			dbprintf("Looking up %s\n", ld.linkname);
			nfs_lookup(&ld.cwd, ld.linkname, dir_cb, (uintptr_t)&ld);
			do {
				struct pbuf buf;
				rpc_recv(&buf, nfs_fd, -1);
			} while (!ld.ok);
		} else {
			dbprintf("Final component: %s\n", ld.linkname);
			nfs_lookup(&ld.cwd, ld.linkname, func, token);
		}
	} while (slashp && ld.ok >= 0);
	free(s);
}		 

int
init_and_open(char *hostname, char *mountpoint, char *filename){
	struct sockaddr_in addr;
	struct cookie pfh;
	static int calibrated = 0;

	if (!calibrated) {
		microuptime_calibrate();
		calibrated = 1;

		/* XXX these should be dynamically controlled */
		callback_init(10,100000);
	}

	if (resolve(hostname, &addr) < 0){
		dbprintf("error resolving \"%s\"\n", hostname);
		exit(-1);
	}


	map_init(&addr);
	mnt_init(&addr);
	nfs_init(&addr);

	mnt_get_export_list();
	if (mnt_mount(mountpoint, &pfh)) {
		dbprintf("Can't mount\n");
		return -1;
	}
	recursive_lookup(&pfh, filename, open_cb, 0);

	initialised = 0;
	while (initialised == 0){
		struct pbuf buf;
		rpc_recv(&buf, nfs_fd, -1);
	}

	if (initialised < 0){
		printf("Can't initialise: got %d\n", initialised);
		return -1;
	}

	initialise_cache();

	return 0;
}

void
nfs_lat_cleanup(void)
{
	nfs_cleanup();
	mnt_cleanup();
	map_cleanup();
}
