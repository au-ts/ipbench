/*
 * transport.c - all the crappy functions that should be hidden at all
 * costs
 */ 

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "nfs.h"
#include "rpc.h"
#include "xdr.h"
#include "nfsrpc.h"

/************************************************************
 *  Debugging defines
 ***********************************************************/ 

#define DEBUG_RPC

#ifdef DEBUG_RPC
#define debug(x...) fprintf(stderr, x )
#else
#define debug(x...)
#endif

#define NFS_MACHINE_NAME "boggo"

/************************************************************
 *  XID Code
 ***********************************************************/ 

static xid      cur_xid = 100;

int            
get_xid(void)
{	
	return ++cur_xid;
} 

/***************************************************************
 *  Buffer handling code                                       *
 ***************************************************************/ 

/* Add a string to the packet */ 
void
addstring(struct pbuf *pbuf, char *data)
{
	adddata(pbuf, data, strlen(data), 0);
} 

/* Add a fixed sized buffer to the packet */ 
void           
adddata(struct pbuf *pbuf, void *data, uintptr_t size, int byteswap)
{
	int padded;
	int fill = 0;
	
	padded = size;
	
	addtobuf(pbuf, &size, sizeof(int), 1);

	addtobuf(pbuf, data, size, byteswap);
	
	if (padded % 4) {
		addtobuf(pbuf, (char *)&fill, 4 - (padded % 4), 0);
	}
}

void
skipstring(struct pbuf* pbuf) 
{
	int size;
	
	/* extract the size */ 
	getfrombuf(pbuf, (char *)&size, sizeof(size), 1);
	
	if (size % 4)
		size += 4 - (size % 4);
	
	pbuf->pos += size;
}

void           
getstring(struct pbuf *pbuf, char *data, int len)
{	
	getdata(pbuf, data, len, 1, 0);	
} 

int            
getdata(struct pbuf *pbuf, char *data, int len, int null, int byteswap)
{
	int size, padsize;
	assert(len > 0);
	
	/* extract the size */ 
	getfrombuf(pbuf, (char *)&size, sizeof(size), 1);

	padsize = size;
	
	if (size < len)
		len = size;
	
	/* copy bytes into tmp */ 
	if (padsize % 4)
		padsize += 4 - (padsize % 4);
	
	getfrombuf(pbuf, data, len, byteswap);
	
	pbuf->pos += (padsize - len);
	
	/* add the null pointer to the name */ 
	if (null)
		data[len] = '\0';
	
	return len;
} 

void*
getpointfrombuf(struct pbuf * pbuf, int len)
{
	void *ret = &pbuf->buf[pbuf->pos];
	pbuf->pos += len;
	return ret;
} 

#define SWAP_NTOH 0
#define SWAP_HTON 1

/*
 * XXX byte-swapping is a nasty hack
 * assumes all sub-types are 32-bit aligned 32-bit entities.
 */
static void
bswap_memcpy(void *dst, void *src, uintptr_t len, int direction){
	uint32_t *d=dst, *s=src;
	uintptr_t i, count;

	/* XXX catch possible alignment problems */
	assert(((uintptr_t)s)%4==0);
	assert(((uintptr_t)d)%4==0);
	assert(len%4==0);

	count = len / sizeof(uint32_t);

	if(direction==SWAP_NTOH){
		for(i=0; i<count; i++){
			d[i] = ntohl(s[i]);
		}
	}else{
		for(i=0; i<count; i++){
			d[i] = htonl(s[i]);
		}
	}		
}

void           
getfrombuf(struct pbuf *pbuf, void *data, uintptr_t len, int byteswap)
{
	if(byteswap==0){
		memcpy(data, &pbuf->buf[pbuf->pos], len);
	}else{
		bswap_memcpy(data, &pbuf->buf[pbuf->pos], len, SWAP_NTOH);
	}
	pbuf->pos += len;
} 

void
addtobuf(struct pbuf *pbuf, void *data, uintptr_t len, int byteswap)
{
	if(byteswap==0){
		memcpy(&pbuf->buf[pbuf->pos], data, len);
	}else{
		bswap_memcpy(&pbuf->buf[pbuf->pos], data, len, SWAP_HTON);
	}
	pbuf->pos += len;
}

/* for synchronous calls */ 
void
initbuf(struct pbuf *pbuf, int prognum, int vernum, int procnum)
{
	int txid = get_xid();
	int calltype = MSG_CALL;
	call_body bod;
	opaque_auth_t cred, verf;
	int nsize;
	int tval;
	
	//debug("initbuf(): creating xid: %d\n", txid);

	/* zero the pbuf */
	pbuf->pos = 0;
	memset(pbuf->buf, 0, PBUF_SIZE);
	
	/* add the xid */
	addtobuf(pbuf, (char *)&txid, sizeof(xid), 1);
	
	/* set it to call */ 
	addtobuf(pbuf, (char *)&calltype, sizeof(int), 1);
	
	/* add the call body - prog/proc info */
	/* XXX lukem --- this should zero the struct first? (no cred) */
	bod.rpcvers = SRPC_VERSION;
	bod.prog = prognum;
	bod.vers = vernum;
	bod.proc = procnum;
	addtobuf(pbuf, (char *)&bod, sizeof(bod), 1);
	
	/* work out size of name */
	nsize = strlen(NFS_MACHINE_NAME);
	if (nsize % 4)
		nsize += 4 - (nsize % 4);
	
	/* now add the authentication */ 
	cred.flavour = AUTH_UNIX;
	cred.size = 5 * sizeof(int) + nsize;
	addtobuf(pbuf, (char *)&cred, sizeof(cred), 1);
	
	/* add the STAMP field */ 
	tval = 37;	/* FIXME: Magic number! */
	addtobuf(pbuf, (char *)&tval, sizeof(tval), 1);
	
	/* add machine name */ 
	addstring(pbuf, NFS_MACHINE_NAME);
	
	/* add uid */ 
	tval = 0;	/* root */
	addtobuf(pbuf, (char *)&tval, sizeof(tval), 1);

	/* add gid */ 
	tval = 0;	/* root */
	addtobuf(pbuf, (char *)&tval, sizeof(tval), 1);
	
	/* add gids */ 
	tval = 0;
	addtobuf(pbuf, (char *)&tval, sizeof(tval), 1);
	
	verf.flavour = AUTH_NULL;
	verf.size = 0;
	addtobuf(pbuf, (char *)&verf, sizeof(verf), 1);
} 

/********************************************************
 *  General functions
 *********************************************************/ 
int
extract_xid(char *data)
{
	int xid;
	
	/* extract the xid */ 
	memcpy(&xid, data, sizeof(int));
	
	return ntohl(xid);
}

int
reset_xid(char *data)
{
	int xid, tmp;
	xid = get_xid();
	tmp = htonl(xid);
	memcpy(data, &tmp, sizeof(int));
	return xid;
}
