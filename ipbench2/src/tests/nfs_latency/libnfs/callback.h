/* callback.h */
#ifndef __CALLBACK_H
#define __CALLBACK_H

void callback_init(uint32_t secs, uint32_t req_per_sec);

void callback_add(uint32_t xid, struct callback c);

/* returns -1 on fail, zero on success */
int callback_del(uint32_t xid, struct callback *c);

#endif  /* __CALLBACK_H */
