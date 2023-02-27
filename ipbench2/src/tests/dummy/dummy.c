#include "dummy.h"
/* a dummy test */

/*
 * Setup your test.  After this is complete you should be ready to
 * start; if you need a warmup period etc do it in start(); just don't
 * timestamp / start collecting statistics before you are ready.
 */
int
dummy_setup(char *hostname, int port, char *arg)
{
	dbprintf("dummy_setup - %s - %d - %s\n", hostname, port, arg);
	return 0;
}

int dummy_setup_controller(char *arg)
{
	dbprintf("dummy_setup_controller - %s\n", arg);
	return 0;
}

/*
 * Start your test -- timestamp start with gettimeofday() as the last
 * step before you start the testing propper.  This function should
 * *not* return until you want the stop() fn called.
 */
int
dummy_start (struct timeval *start)
{
	dbprintf("DUMMY START\n");
	gettimeofday(start, NULL);

	/* put test here */
	dbprintf("Running test (sleep 5)\n");
	sleep(5);
	dbprintf("Test done!\n");

	return 0;
}

/*
 * Called on test stop.  Timestamp stop with gettimeofday() as the
 * last step before you return
 */
int
dummy_stop (struct timeval *stop)
{
	gettimeofday (stop, NULL);
	dbprintf ("DUMMY STOP\n");
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
 *
 * Return !0 to flag the data as invalid.
 */
int
dummy_marshall (char **data, int * size, double running_time)
{
	char *ok_string = "You have successfully called the dummy test!";
	char *not_ok_string = "You have unsuccessfully called the dummy test!\n";

	dbprintf("DUMMY MARSHALL\n");

	/* We randomly fail this */
	srand(time(NULL));
	int j = 1 + (int)(10.0 * rand()/(RAND_MAX + 1.0));;

	if (j % 2) {
		dbprintf("Test Passed!\n");
		*data = ok_string;
	} else {
		dbprintf("Test Failed!\n");
		*data = not_ok_string;
	}

	*size = strlen (*data) + 1;
	if (j % 2)
		return 0;

	return 1;
}

/*
 * After arguments have been marshalled this is called with the data
 * passed in, in case you want to free it or something
 */
void
dummy_marshall_cleanup(char **data)
{
	dbprintf("DUMMY CLEANUP MARSHALL\n");
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
dummy_unmarshall(char *input, int input_len, char **data,
		  int * data_len)
{
	char *buf = malloc (input_len);
	dbprintf("[unmarshall] start\n");
	*data = buf;
	memcpy(buf, input, input_len);
	*data_len = input_len;
	dbprintf("[unmarshall] end\n");
	return 0;
}

/*
 * You are passed back your data pointer from above here in case you
 * need to free it or do any other cleaning up
 */
void
dummy_unmarshall_cleanup(char **data)
{
	free(*data);
}

/*
 * data[] is an array that has whatever was passed back from each
 * client.  Here you can loop through the array and aggreage or
 * otherwise do interesting things.  Print out your results in any
 * particular format here.
 *
 * target_data comes from the target machine, or if no target testing
 * was done is NULL.
 */
int
dummy_output(struct client_data data[], int nelem)
{
	int i, ret = 0;
	dbprintf("DUMMY OUTPUT (nelem %d)\n", nelem);
	for (i = 0; i < nelem; i++) {
		if (!data[i].valid) {
			dbprintf("We got some invalid data! Trying again\n");
			ret = 1;
		}
		printf ("DUMMY RETURNED: %s\n", (char *)data[i].data);
	}
	dbprintf ("DUMMY DONE\n");
	return ret;
}
