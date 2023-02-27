/* Latency CPU test; core designed by Luke Mcpherson
 * Bits stripped out and added by Ian Wienand <ianw@gelato.unsw.edu.au>
 * (C) 2005
 * See header and README for important information
 */
#define IPBENCH_TEST_TARGET
#include "plugin.h"
#include "ipbench.h"

#include "cpu_target_lukem.h"

/* options */
int nr_cpus;
int warmup_time;
int cooldown_time;
int fancy_output;

/* stores timer information */
volatile struct timer_buffer_t timer_buffer;

/* handle for the idle thread */
static pthread_t ithr;
/* when we flip this, we stop calculating */
static int calc = 1;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void *idle_thread(void *arg)
{
	cycle_t x0, x1, delta, total, idle;

	idle = total = 0;
	x0 = get_cycles();

	while (calc) {
		x1 = x0;
		x0 = get_cycles();
		
		delta = x0 - x1;
		total += delta;
		
		/* If the delta looks like less than a context switch,
		 * add this to idle time; otherwise add it to busy
		 * time */
		if (delta < PROFILE_CONTEXT_COST)
			idle += delta;
			
		timer_buffer.idle = idle;
		timer_buffer.total = total;
	}

	return 0;
}

/* Parse commands from an argument of the form
 * cmd1=val1,cmd2=val2...
 */
static int parse_arg(char *arg)
{
	char *c, *cmd, *val;
	char *arg_ptr, *cmd_ptr;

	c = strtok_r(arg, ",", &arg_ptr);
	if (c == NULL) {
		dbprintf("Invalid argument %s.\n", arg);
		return -1;
	}
	cmd = strtok_r(c, "=", &cmd_ptr);
	val = strtok_r(NULL, "=", &cmd_ptr);

	do {
		dbprintf("Got cmd %s val %s\n", cmd, val);
		if (!strcmp(cmd, "cpus"))
			nr_cpus = strtol(val, (char**)NULL, 10);
		else if (!strcmp(cmd, "warmup"))
			warmup_time = strtol(val, (char**)NULL, 10);
		else if (!strcmp(cmd, "cooldown"))
			cooldown_time = strtol(val, (char**)NULL, 10);
		else if (!strcmp(cmd, "fancy_output"))
			fancy_output = 1;

		if ((c = strtok_r(NULL, ",", &arg_ptr)) == NULL)
			break;
		cmd_ptr = NULL;
		cmd = strtok_r(c, "=", &cmd_ptr);
		val = strtok_r(NULL, "=", &cmd_ptr);

	} while (1);

	return 0;
}


int 
cpu_target_lukem_setup_controller(char *arg)
{
	return 0;
}

int
cpu_target_lukem_setup (char *arg)
{
	dbprintf ("Setup test [%c].\n", *arg);

	if (strlen(arg) != 0)
		if (parse_arg(arg))
			return -1;
	
	/* put ourselves at a low priority */
	if (nice(20) == -1)
		dbprintf("nice for cpu measurement: %s\n", strerror(errno));

	/* create the idle thread */
	pthread_create(&ithr, NULL, idle_thread, NULL);

	/* lock the mutex */
	pthread_mutex_lock(&mutex);

	return 0;
}

int
cpu_target_lukem_start(struct timeval *start)
{
	gettimeofday(start, NULL);
	dbprintf("Starting.\n");
	
	/* wait on the conditional */
	pthread_cond_wait(&cond, &mutex);

	dbprintf("Complete.\n");
	return 0;
}

int
cpu_target_lukem_stop(struct timeval *stop)
{
	gettimeofday(stop, NULL);
	calc = 0;
	pthread_cond_signal(&cond);
	dbprintf("Stopping.\n");
	return 0;
}

int
cpu_target_lukem_marshall(char **data, int *size, double running_time)
{
	char buf[BUFSIZ];
	snprintf(buf, BUFSIZ, "%.1f\n", (1.f - 
		((double)timer_buffer.idle/(double)timer_buffer.total)) 
		 * 100 );

	dbprintf("Average CPU time is %s\n", buf );

	*data = malloc(strlen(buf)+1);
	memcpy(*data, buf, strlen(buf)+1);
	*size = strlen(buf) + 1;

	return 0;
}

void
cpu_target_lukem_marshall_cleanup(char **data)
{
	free(*data);
}


int
cpu_target_lukem_unmarshall(char *input, int input_len, char **data,
		  int *data_len)
{
	dbprintf("[cpu_target_lukem_unmarhsall] start\n");
	char *buf = calloc(sizeof(char),input_len);
	*data = buf;
	memcpy(buf, input, input_len);
	*data_len = input_len;
	dbprintf("[cpu_target_lukem_unmarshall] cpu usage %s\n", (char*)(*data));
	return 0;
}


void
cpu_target_lukem_unmarshall_cleanup(char **data)
{
	free(*data);
}


int 
cpu_target_lukem_output(struct client_data *data) 
{
	/* fancy output really isn't *that* fancy :) */
	if (fancy_output)
		printf("CPU USAGE: ");
	printf ("%s\n", (char *)data->data);
	return 0;

}

struct ipbench_plugin ipbench_plugin = 
{
	.magic = "IPBENCH_PLUGIN",
	.name = "cpu_target_lukem",
	.id = 0x20,
	.descr = "Measure CPU usage on the target (lukem version)",
	.default_port = 7,
	.setup = &cpu_target_lukem_setup,
	.setup_controller = &cpu_target_lukem_setup_controller,
	.start = &cpu_target_lukem_start,
	.stop = &cpu_target_lukem_stop,
	.marshall = &cpu_target_lukem_marshall,
	.marshall_cleanup = &cpu_target_lukem_marshall_cleanup,
	.unmarshall = &cpu_target_lukem_unmarshall,
	.unmarshall_cleanup = &cpu_target_lukem_unmarshall_cleanup,
	.output = &cpu_target_lukem_output,
};

