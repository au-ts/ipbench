/* a test that simply waits and does nothing */

#define IPBENCH_TEST_CLIENT 1
#include "plugin.h"
#include "ipbench.h"


void sigint_handler(int signal)
{
	printf("Got a SIGINT, ending test\n");
	fclose(stdin);
	return;
}

int
wait_setup(char *hostname, int port, char *arg)
{
	dbprintf("waitsetup - %s - %d - %s\n", hostname, port, arg);
	signal(SIGINT, sigint_handler);
	return 0;
}

int wait_setup_controller(char *arg)
{
	return 0;
}

int
wait_start (struct timeval *start)
{
	gettimeofday(start, NULL);

	/* put test here */
	printf("Please press ENTER or send a SIGINT to %d to stop the test\n", getpid());
	getc(stdin);
	printf("Ending test\n");

	return 0;
}

int
wait_stop (struct timeval *stop)
{
	gettimeofday (stop, NULL);
	return 0;
}

int
wait_marshall (char **data, int * size, double running_time)
{
	char *str = malloc(strlen("OK")+1);
	*size = strlen("OK") + 1;
	strcpy(str, "OK");
	*data = str;
	return 0;
}

void
wait_marshall_cleanup(char **data)
{
	free(*data);
}

int
wait_unmarshall(char *input, int input_len, char **data,
		  int * data_len)
{
	*data = malloc(input_len);
	memcpy(*data, input, input_len);
	*data_len = input_len;
	return 0;
}

void
wait_unmarshall_cleanup(char **data)
{
	return;
}

int
wait_output(struct client_data data[], int nelem)
{
	printf("%s\n", data[0].data);
	return 0;
}

struct ipbench_plugin ipbench_plugin = {
	.magic = "IPBENCH_PLUGIN",
	.name = "wait",
	.id = 0x15,
	.descr = "The waiting test",
	.default_port = 6123,
	.type = IPBENCH_CLIENT,
	.setup = &wait_setup,
	.setup_controller = &wait_setup_controller,
	.start = &wait_start,
	.stop = &wait_stop,
	.marshall = &wait_marshall,
	.marshall_cleanup = &wait_marshall_cleanup,
	.unmarshall = &wait_unmarshall,
	.unmarshall_cleanup = &wait_unmarshall_cleanup,
	.output = &wait_output,
};
