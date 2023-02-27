/* transport.h */ 
#ifndef __RPC_H
#define __RPC_H

#define PBUF_SIZE 1460

struct pbuf{
	int pos;           /* start of data inside buf   */
	char buf[PBUF_SIZE]; /* space where data is stored */
};

struct callback_args_nfs{
	void *cb;
	uintptr_t arg;
};

struct callback_args_time{
	uint64_t timestamp;
};

struct callback_args_call{
	int *notify;
	struct pbuf **result;
};

struct callback{
	void (*func)(struct callback *c, struct pbuf *p);
	union {
		struct callback_args_nfs nfs;
		struct callback_args_time time;
		struct callback_args_call call;
	} param;
};

int rpc_send(struct pbuf *pbuf, int fd, struct callback *c);
int rpc_recv(struct pbuf *pbuf, int fd, int timeout);
int rpc_call(struct pbuf *snd, struct pbuf *rcv, int fd);

int init_transport(struct sockaddr *name, socklen_t namelen);

struct callback
create_nfs_callback(void (*func)(struct callback *c, struct pbuf *p),
                    void *callback, uintptr_t arg);

#endif	/* __RPC_H */
