#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include "nfs.h"
#include "rpc.h"
#include "xdr.h"
#include "callback.h"
#include "nfsrpc.h"

/************************************************************
 *  Debugging defines
 ***********************************************************/ 

#define DEBUG_RPC
#undef DEBUG_RPC

#ifdef DEBUG_RPC
#define debug(x...) fprintf(stderr, x )
#else
#define debug(x...)
#endif

#ifdef DEBUG_RPC
static void
rpc_print(const char *text, unsigned char *data, size_t length){
	size_t i, j;

	printf("%s\n", text);

	i = 0;
	while(i<length){
		j = 0;
		printf("%04x", (int)i);
		while(j<16 && i<length){
			printf(" %02x", (int)data[i]);
			i++;
			j++;
		}
		printf("\n");
	}
}
#else
static void rpc_print(const char *text, unsigned char *data, size_t length)
{
}
#endif
int            
rpc_send(struct pbuf *pbuf, int fd, struct callback *c)
{
	int xid, r;

	assert(pbuf && pbuf->pos >=0 && pbuf->pos <= PBUF_SIZE);

	/* we always add the callback */
	/* anything after here is just packet loss */
	if (c != NULL){
		xid = extract_xid(pbuf->buf);
		callback_add(xid, *c);
	}

	if (pbuf->pos==0)
		return -1; /* don't send zero length rpc */

	rpc_print("sending:", (unsigned char *)pbuf->buf, pbuf->pos);

	r = send(fd, pbuf->buf, pbuf->pos, 0);

	//if (r < 0) {
	//	if(errno==EAGAIN){
	//		return 0; /* sent 0 bytes successfully */
	//	}
	//	return -1;
	//}

	return r;
}

/* returns 0 on success, -1 on error */
int
rpc_recv(struct pbuf *pbuf, int fd, int timeout)
{
	int x, xid;
	struct callback c;

	if (timeout != 0){
		int r;
		struct pollfd p = {fd, POLLIN, 0};
		r = poll(&p, 1, timeout);
		if (r <= 0) {
			return -1;
		}
	}

	/* zero the pbuf */
	pbuf->pos = 0;
	memset(pbuf->buf, 0, PBUF_SIZE);

	/* receive new packet */
	x = recv(fd, pbuf->buf, PBUF_SIZE, 0);
	switch(x){
	case -1:
		if (errno == EAGAIN){
			return 0;
		}
		perror("rpc_recv(): recv failed");
		return -1;

	case 0:
		printf("rpc_recv(): remote host closed connection?\n");
		exit(-1);
	}

	rpc_print("received:", (unsigned char *)pbuf->buf, x);
	
	/* find and execute callback function */
	xid = extract_xid(pbuf->buf);
	if (callback_del(xid, &c) < 0)
		return -1;

	/* do the callback */
	if (c.func != NULL)
		c.func(&c, pbuf);

	return 0;
}

void
rpc_call_cb(struct callback *c, struct pbuf *p){
	int *notify = c->param.call.notify;
	struct pbuf **result = c->param.call.result;

	*result = p;
	*notify = ~0;
}

int
rpc_call(struct pbuf *snd, struct pbuf *rcv, int fd)
{
	struct callback c, *tx;
	int notify = 0;
	struct pbuf *result;

	c.func = rpc_call_cb;
	c.param.call.notify = &notify;
	c.param.call.result = &result;
	tx = &c;

	/* retransmit every 100ms until success */
	while (notify == 0){
		assert(rpc_send(snd, fd, tx)==snd->pos);
		tx = NULL;
		rpc_recv(rcv, fd, 100);
	}

	assert(result == rcv);

	/* parse the rpc headers */
	{
		opaque_auth_t auth;
		reply_stat r;

		rcv->pos += 8;

        	/* check if it was an accepted reply */
		getfrombuf(rcv, (char*) &r, sizeof(r), 1);

		if (r != MSG_ACCEPTED) {
			debug( "Message NOT accepted (%d)\n", r );

			/* extract error code */
			getfrombuf(rcv, (char*) &r, sizeof(r), 1);
			debug( "Error code %d\n", r );

			if (r == 1) {
				/* get the auth problem */
				getfrombuf(rcv, (char*) &r, sizeof(r), 1);
				debug( "auth_stat %d\n", r );
			}

			return 0;
		}

		/* and the auth data!*/
		getfrombuf(rcv, (char*) &auth, sizeof(auth), 1);

		debug("Got auth data. size is %d\n", auth.size);

		/* check its accept stat */
		getfrombuf(rcv, (char*) &r, sizeof(r), 1);

		if (r != SUCCESS) {
			debug("reply stat was %d\n", r);
			return 0;
		}
	}

	return 0;
} 

struct callback
create_nfs_callback(void (*func)(struct callback *c, struct pbuf *p),
                void *callback, uintptr_t arg){
	struct callback c;

	c.func = func;
	c.param.nfs.cb = callback;
	c.param.nfs.arg = arg;
 
	return c;
}
