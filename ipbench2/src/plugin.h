#ifndef _PLUGIN_H
#define _PLUGIN_H 1

#include "ipbench.h"
#include "lib/util.h"
/*
 * This header describes the interface an ipbench plugin 
 * must implement.
 * 
 * Each test must #define IPBENCH_TEST_CLIENT or IPBENCH_TEST_TARGET
 * to describe if it should be run on the client machine or on the
 * DUT.  The prototypes then become slightly different; for example if
 * you are a target test then setup() does not need information about
 * the target machine and what port, only arguments.  Similarly, the
 * output function will only be passed one set of data (from the
 * target) so doesn't get a count.
 */

#define MAX_PLUGINS 15

#define IPBENCH_CLIENT 0
#define IPBENCH_TARGET 1


/* The data a client returns.  Note that we use a char* and size is an
 * "int" rather than a "size_t" because that's what Python takes as
 * arguments. */
struct client_data
{
	char *data;   // data from the client
	int size;     // length of data
	int valid;    // is this valid?
	int type;     // either IPBENCH_CLIENT || IPBENCH_TARGET
};

struct ipbench_plugin {
	/* always IPBENCH_PLUGIN */
	char *magic;
	/* the name of the test (for --test) */
	char *name;
	/* a unique test id */
	int id;
	/* a short description of the test */
	char *descr;
	/* the default port clients to connect to on the target */
	int default_port;
	/* is this test designed for a client or a target ? */
	int type;
	/* do any setup before running the test. arg is either a string or NULL if no arguments.
	 * Since the client needs to target a machine, it needs a few extra args */
#ifdef IPBENCH_TEST_CLIENT
	int (*setup)(char *hostname, int port, char* arg);
#elif defined IPBENCH_TEST_TARGET
	int (*setup)(char* arg);
#else
#error "Please define either IPBENCH_TEST_CLIENT or IPBENCH_TEST_TARGET"
#endif
	/* any arguments the controller may need to parse */
	int (*setup_controller)(char *arg);

	/* start the test.  Timestamp *start with gettimeofday */
	int (*start)(struct timeval *start);

	/* stop the test.  Once again, timestamp *stop */
	int (*stop)(struct timeval *stop);

	/* point data to a block of size for sending back to controller.
           running_time will be passed to the test as the difference between
           start and stop above */
	int (*marshall)(char **data, int *size, double running_time);

	/* clean up after marshalling (get the block of data you passed to marshall as an argument) */
	void (*marshall_cleanup)(char **data);

	/* called from controller to decode results */
	int (*unmarshall)(char *input, int input_len, char **data, int *data_len);
	
	/* clean up after unmarshalling */
	void (*unmarshall_cleanup)(char **data);
	
	/* called to aggregate and output results */
#ifdef IPBENCH_TEST_CLIENT       
	int (*output)(struct client_data data[], int nelem);
#elif defined IPBENCH_TEST_TARGET
	int (*output)(struct client_data *data);
#endif
};


/*
 * utility functions exported by the ipbench suite
 * you can find these in the library in src/lib/
 */

extern int next_token(char **, char *, char *);
extern int set_socket_options(int, char *);

extern int do_debug;
extern void dbprintf (const char *, ...);

#endif /* _PLUGIN_H */
