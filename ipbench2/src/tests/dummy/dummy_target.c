#include "dummy_target.h" 

/* a dummy target test */

static int running=1;

/*
 * Setup your test.  The target will be setup before any clients are
 * setup (in case it is a server process, etc).
 */
int
dummy_target_setup(char *arg)
{
	dbprintf ("[target] [target_setup] : START\n");
	dbprintf ("dummy_target_setup - %s\n", arg);
	return 0;
}

int
dummy_target_setup_controller(char *arg)
{
	dbprintf ("[target] [target_setup_controller] : START\n");
	dbprintf ("dummy_target_setup - %s\n", arg);
	return 0;
}

/*
 * Start your test -- timestamp start with gettimeofday() as the last
 * step before you start the testing propper.  This will bet
 * interrupted by dummy_target_stop() and must exit after that.  The
 * way we do it here is using a flag, though any mechanism to stop
 * after calling dummy_target_stop() is acceptable.
 */
int
dummy_target_start (struct timeval *start)
{
	int i=0;
	dbprintf("DUMMY TARGET START\n");
	gettimeofday(start, NULL);

	while (running) {
		i++;
		usleep(500000);
	}
	return 0;
}

/*
 * Stop the test.  Timestamp stop with gettimeofday() as the last step
 * before you return.
 */
int
dummy_target_stop(struct timeval *stop)
{
	gettimeofday(stop, NULL);
	running = 0;
	dbprintf("DUMMY TARGET STOP\n");
	return 0;
}

/*
 * Marshall all arguments into a single array *data of length size.
 * This will be passed back to unmarshall().  You also get the running
 * time; this may be useful for some figures before packaging them up
 * for sending.
 *
 * Test authors are responsible for cleaning up any data they allocate
 * here
 */
int
dummy_target_marshall(char **data, int * size, double running_time)
{
	char *ok_string = "You have successfully called the dummy target test!";

	dbprintf ("DUMMY TARGET MARSHALL\n");

	*data = ok_string;
	*size = strlen(ok_string) + 1;
	return 0;
}

/*
 * After arguments have been marshalled this is called with the data
 * passed in, in case you want to free it or something
 */
void
dummy_target_marshall_cleanup(char **data)
{
	dbprintf("DUMMY TARGET CLEANUP MARSHALL\n");
}


/*
 * The parts below run in the controller, rather than in the tester.
 * Remember this, or otherwise split it out into another file, etc.
 */


/*
 * Unmarshall *input into data.  *data should really be some sort of
 * typecast struct that holds all the info you get from the test.
 */
int
dummy_target_unmarshall(char *input, int input_len, char **data,
		  int *data_len)
{
	char *buf = malloc (input_len);
	*data = buf;
	memcpy (buf, input, input_len);
	*data_len = input_len;
	return 0;
}


/*
 * You are passed back your data pointer from above here
 * in case you need to free it or do any other cleaning up
 */
void
dummy_target_unmarshall_cleanup(char **data)
{
	free(*data);
}


/* You only get one client_data struct here, unlike in the client test
 * where you get an array of every clients data */
int
dummy_target_output(struct client_data *data)
{
	int ret = 0;
	dbprintf("DUMMY TARGET OUTPUT\n");
	if (!data->valid) {
		dbprintf("We got some invalid data! Trying again\n");
			ret = 1;
	}
	printf ("DUMMY TARGET RETURNED: %s\n", (char *)data->data);
	dbprintf ("DUMMY TARGET DONE\n");
	return ret;
}
