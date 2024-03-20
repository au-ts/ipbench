#include <string.h>
#include <stdio.h>

int errcode;
char errmsg[256];
/*
   Copy an error message to the errmsg buffer + an error code for errcode
   for return to Python code.
*/
int ipbench_error(int code, char *msg)
{
	errcode = code;
	strncpy(errmsg, msg, 255);
        errmsg[255] = 0;
	return -1;
}

/*
   Wrapper around ipbench_error which also includes plugin name.
*/
int ipbench_plugin_error(int code, char *msg, char *source) {
	char buf[255];
	snprintf(buf, 255, "%s: %s", msg, source);
	return ipbench_error(code, buf);
}
