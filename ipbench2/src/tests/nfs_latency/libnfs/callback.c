#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inttypes.h>

#include "lib/util.h"
#include "rpc.h"
#include "callback.h"

struct callback_node{
	uint32_t xid;
	struct callback callback;
};

static struct callback_node *callback_list = NULL;
static uintptr_t callback_list_length = 0;
static uintptr_t callback_list_head = 0;
static uint64_t drops = 0;
static uint32_t last_xid = 0;

static void
callback_init2(uintptr_t slots){
	assert(callback_list==NULL && callback_list_length==0);

	callback_list = malloc(slots*sizeof(struct callback_node));
	assert(callback_list!=NULL);

	memset(callback_list, 0, (slots*sizeof(struct callback_node)));

	callback_list_length = slots;
}

void
callback_init(uint32_t secs, uint32_t req_per_sec){
	callback_init2(secs*req_per_sec);
}

void
callback_add(uint32_t xid, struct callback c)
{
	/* provided xid's must be increment by 1 for each call */
	assert(xid>0);
	if(last_xid>0){
		if(xid != (last_xid+1)){
			dbprintf("xid=%d, last_xid=%d\n", xid, last_xid);
			assert(xid > last_xid);
		}
	}
	last_xid = xid;

	/* increment callback list position */
	callback_list_head = (callback_list_head + 1) % callback_list_length;

	/* if we are overwriting a slot, increment drops */
	drops += (callback_list[callback_list_head].xid != 0);

	callback_list[callback_list_head].xid = xid;
	callback_list[callback_list_head].callback = c;
}

int
callback_del(uint32_t xid, struct callback *c)
{
	uint64_t offset, index;

	assert(xid > 0);

	offset = callback_list[callback_list_head].xid - xid;

	/* callback is too old */
	if (offset >= callback_list_length){
		return -1;
	}

	if (callback_list_head >= offset) {
		index = callback_list_head - offset;
	} else {
		index = callback_list_length - (offset - callback_list_head);
	}

	assert(index < callback_list_length);

	if (callback_list[index].xid != xid){
		/* no matching xid */
		return -1;
	}

	/* found matching xid */
	if(c!=NULL){
		*c = callback_list[index].callback;
	}
	callback_list[index].xid = 0;

	return 0;
}

