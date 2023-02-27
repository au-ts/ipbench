/* transport.h */ 
#ifndef __XDR_H
#define __XDR_H

/* these all modify RPC buffers */ 
void clearbuf(struct pbuf * pbuf);
void initbuf(struct pbuf *pbuf, int prognum, int vernum, int procnum);
void addtobuf(struct pbuf *pbuf, void *data, uintptr_t len, int byteswap);
void getfrombuf(struct pbuf *pbuf, void *data, uintptr_t len, int byteswap);
void getstring(struct pbuf *pbuf, char *data, int len);
int  getdata(struct pbuf *pbuf, char *data, int len, int null, int byteswap);
void* getpointfrombuf(struct pbuf *pbuf, int len);
void addstring(struct pbuf *pbuf, char *data);
void adddata(struct pbuf *pbuf, void *data, uintptr_t size, int byteswap);
void skipstring(struct pbuf *pbuf);

/* do we need this in transport?? */ 
void change_endian(struct pbuf *pbuf);

void resetbuf(struct pbuf *pbuf);

struct pbuf *initbuf_xid(int txid, int prognum, int vernum, int procnum);

int extract_xid(char *data);
int reset_xid(char *data);

#endif	/* __XDR_H */
