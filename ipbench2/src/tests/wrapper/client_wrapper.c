#include "client_wrapper.h"

int
client_wrap_setup(char *hostname, int port, char *arg)
{
	dbprintf("[client_wrap_setup] %s - %d - %s\n", hostname, port, arg);
	return parse_arg(arg);
}

int
client_wrap_controller_setup(char *arg)
{
	return 0;
}

int
client_wrap_start (struct timeval *start)
{
	dbprintf("[client_wrap_start] start test\n");
	gettimeofday(start, NULL);

	/* XXX arguments */
	if (system(start_filename) == -1) {
		dbprintf("%s failed: %s\n", start_filename, strerror(errno));
		return 1;
	}

	return 0;
}

int
client_wrap_stop (struct timeval *stop)
{
	gettimeofday (stop, NULL);
	dbprintf ("[client_wrap_stop] calling %s \n", stop_filename);

	if (system(stop_filename) == -1) {
		dbprintf("%s failed: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int
client_wrap_marshall (char **data, int *size, double running_time)
{
	int sts;
	char *retdata;
	sts = common_marshall(&retdata, size, running_time);
	*data = retdata;
	return sts;
}

void
client_wrap_marshall_cleanup(char **data)
{
	free(*data);
}


/* just copy */
int
client_wrap_unmarshall(char *input, int input_len, char **data,
		  int * data_len)
{
	char *buf = malloc (input_len);
	dbprintf("[client_wrap_unmarshall] start\n");
	*data = buf;
	memcpy(buf, input, input_len);
	*data_len = input_len;
	dbprintf("[client_wrap_unmarshall] end\n");
	return 0;
}

void
client_wrap_unmarshall_cleanup(char **data)
{
	free(*data);
}

int
client_wrap_output(struct client_data data[], int nelem)
{
	int i, ret = 0;
	dbprintf("[client_wrap_output] (nelem %d)\n", nelem);
	for (i = 0; i < nelem; i++) {
		dbprintf("[client_wrap_output: client %d\n", i);
		printf ("%s\n", (char *)data[i].data);
	}
	dbprintf ("[client_wrap_output] done\n");
	return ret;
}
