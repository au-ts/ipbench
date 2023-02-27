#include "target_wrapper.h" 

/* a wrapper around shell scripts */

static int running=1;

int
target_wrap_setup(char *arg)
{
	dbprintf ("[target_wrap_setup] - got args %s\n", arg);
	return parse_arg(arg);
}

int
target_wrap_controller_setup(char *arg)
{
	return 0;
}

int
target_wrap_start (struct timeval *start)
{
	int ret;
	dbprintf("[target_wrap_start] starting %s\n", start_filename);
	gettimeofday(start, NULL);

	ret = system(start_filename);
	/* system failure */
	if (ret == -1)
	{
		fprintf(stderr,"start script failed: %s\n", strerror(errno));
		return ret;
	}

	/* if the program we run returns non-zero, return an error
	   so an exception gets raised back to the controller */
	if (ret != 0)
	{
		dbprintf("start script returned non-zero (%d)\n", ret);
		return ret;
	}

	/* otherwise, sit here and wait until we are told to stop running and 
	   return */
	while (running) {
		usleep(500000);
	}
	return 0;
}

int
target_wrap_stop(struct timeval *stop)
{
	int ret;
	gettimeofday(stop, NULL);
	running = 0;
	dbprintf("[target_wrap_stop] calling stop script\n");

	ret = system(stop_filename);
	if (ret == -1)
		dbprintf("[target_wrap_stop] error calling %s (%s)\n", 
			 stop_filename, strerror(errno));

	return 0;
}


/* call the output script and dump data through to the client */
int
target_wrap_marshall(char **data, int *size, double running_time)
{
	int sts;
	char *retdata;
	sts = common_marshall(&retdata, size, running_time);
	*data = retdata;
	return sts;
}

void
target_wrap_marshall_cleanup(char **data)
{
	/* data is return_data from above, which was malloced */
	free(*data);
}

/* just copy */
int
target_wrap_unmarshall(char *input, int input_len, char **data,
		  int *data_len)
{
	char *buf = malloc (input_len);
	*data = buf;
	memcpy (buf, input, input_len);
	*data_len = input_len;
	return 0;
}


void
target_wrap_unmarshall_cleanup(char **data)
{
	free(*data);
}


int
target_wrap_output(struct client_data *data)
{
	int ret = 0;
	dbprintf("[target_wrap_output] start target output\n");
	printf ("%s", (char *)data->data);
	dbprintf ("[target_wrap_output] stop target output\n");
	return ret;
}
