#include <string.h>

int errcode;
char errmsg[256];

int ipbench_error(int code, char *msg)
{
	errcode = code;
	strncpy(errmsg, msg, 255);
        errmsg[255] = 0;
	return -1;
}
