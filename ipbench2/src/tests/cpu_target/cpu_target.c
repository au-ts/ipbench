/* Latency CPU test.
 *
 * The parts of this code that do anything interesting comes from
 * cyclesoak by Andrew Morton <akpm@zip.com.au>
 * http://www.zipworld.com.au/~akpm/linux/
 *
 * Bits stripped out and added by Ian Wienand
 * <ianw@gelato.unsw.edu.au>
 *
 */
/* For SMP */
#define _GNU_SOURCE
#include <sched.h>

#define IPBENCH_TEST_TARGET
#include "plugin.h"
#include "ipbench.h"

static int running = 1;

#define N_CPUS	64
#define CACHE_LINE_SIZE	128		/* Plenty */
#define BLOCK_SIZE	512

#define DB_KEY_SIZE	64
#define DB_DATA_SIZE	64

static int		debug;
static int		do_calibrate = 0;
static int		calibrate_state = 0;
static long int		nr_cpus;
static unsigned long	calibrated_loops_per_sec[N_CPUS];
static unsigned int	calibration_complete = 0;
static struct timeval	alarm_time;
static unsigned long	counts_per_sec[N_CPUS];
static double		d_counts_per_sec[N_CPUS];
static char*		busyloop_buf;
static unsigned long	busyloop_size = 1000000;
static unsigned long*	busyloop_progress;
static long int		period_secs = 1;
static int		rw_ratio = 5;
static int		cacheline_size = 32;
static unsigned long	sum;
static unsigned long	twiddle = 1000;
static int		do_bonding = 1;
static pid_t		kids[N_CPUS];

static int warmup_time;
static int cooldown_time;
/* an array of samples points */
static double		*cpu_load;

static int fancy_output = 0;

/* We figure out the number of samples that we would take in the
 * cooldown period, and then take twice as many samples (n_samples).
 * This way when we take the average, we throw away the most recent
 * half and consequently disregard the cooldown period.
 */
static int 		n_samples;
/* count of sample points during warmup, these are ignored. */
static long int         warmup_samples = 0;
/* of sample points */
static long int		cpu_samples = 0;

#define CPS_FILE "counts_per_sec"

static long int
count_cpus(void)
{
        FILE *fp;
        long int n = -1UL;
        char nr_cpus_str[8];
                

	fp = popen("cat /proc/cpuinfo | grep processor | wc -l", "r");
	if (fp == NULL || fgets(nr_cpus_str, sizeof(nr_cpus_str)-1, fp) == NULL)
		dbprintf("Failed to count number of processors. Not performing bonding.\n" );
	else {
		n = strtol(nr_cpus_str, NULL, 10);
		dbprintf("%d cpus found\n", nr_cpus);
	}
	pclose(fp);

        if (n > N_CPUS) {
                dbprintf("Recompile with more than %d N_CPU --- too many for test\n", N_CPUS);
                return -1L;
        }
        return n;
}

static double average_cpu(void)
{
	double sum = 0;
	int i;
	int tosum;

	if ( cooldown_time ) {
		/* If we did not get enough samples so that we can discard the
		 * cooldown time for some reason, warn.  */
		if ((cpu_samples)< n_samples/2) {
			dbprintf("Not enough samples to cover cooldown period!\n");
			tosum = cpu_samples;
		} else
			tosum = n_samples/2;
	}
	else 
		tosum = cpu_samples;
		
	dbprintf("Averaging %d samples\n", tosum);

	for(i = 0 ; i < tosum; i++) {
		sum += cpu_load[i];
	}

	return sum / i;
}


static void busyloop(int instance)
{
	int idx = 0;
	int rw = 0;

	for (;;) {
		int thumb;

		rw++;
		if (rw == rw_ratio) {
			busyloop_buf[idx]++;			/* Dirty a cacheline */
			rw = 0;
		} else {
			sum += busyloop_buf[idx];
		}

		for (thumb = 0; thumb < twiddle; thumb++)
			;				/* twiddle */

		busyloop_progress[instance * CACHE_LINE_SIZE]++;

		idx += cacheline_size;
		if (idx >= busyloop_size)
			idx = 0;
	}
}

static void itimer(int sig)
{
	struct timeval tv;
	unsigned long delta;
	unsigned long long blp[N_CPUS];
	unsigned long long total_blp = 0;
	unsigned long long blp_snapshot[N_CPUS];
	static unsigned long long old_blp[N_CPUS];
	char print_out [1024];
	int i;

	gettimeofday(&tv, 0);
	delta = (tv.tv_sec - alarm_time.tv_sec) * 1000000;
	delta += tv.tv_usec - alarm_time.tv_usec;
	delta /= 1000;		/* Milliseconds */

	for (i = 0; i < nr_cpus; i++) {
		blp_snapshot[i] = busyloop_progress[i * CACHE_LINE_SIZE];
		blp[i] = (blp_snapshot[i] - old_blp[i]);
		if (debug)
			dbprintf("CPU%d: delta=%lu, blp_snapshot=%llu, old_blp=%llu, diff=%llu ",
				i, delta, blp_snapshot[i], old_blp[i], blp[i]);
		old_blp[i] = blp_snapshot[i];
		blp[i] *= 1000;
		blp[i] /= delta;
		total_blp += blp[i];
	}

	alarm_time = tv;

	if (do_calibrate) {
		if (calibrate_state++ == 3) {
			for(i = 0; i < nr_cpus; i++) {
				calibrated_loops_per_sec[i] = blp[i];
			}
			calibration_complete = 1;
		}
		snprintf(print_out, sizeof print_out,  "Calibrating -- total: %llu loops/sec", total_blp);
		if (do_bonding) {
			for(i = 0; i < nr_cpus; i++) {
                            snprintf(print_out + strlen(print_out), sizeof print_out - strlen(print_out),
					" || CPU%d: %llu loops/sec", i, blp[i]);
			}
		}
		dbprintf("%s\n", print_out);

	} else {
		double this_cpu_load[N_CPUS];
		double d_blp[N_CPUS];
		double total_cpu_load = 0;

		for (i = 0; i < nr_cpus; i++) {
			d_blp[i] = blp[i];
			this_cpu_load[i] = 1.0 - (d_blp[i] / d_counts_per_sec[i]);

			/* round up */
			if (this_cpu_load[i] < 0) this_cpu_load[i] = 0;
			total_cpu_load += this_cpu_load[i] / nr_cpus;
		}

		if (warmup_samples * period_secs < warmup_time)
		{
			if (cpu_samples % 5)
				dbprintf("Warmup period:%5.1f%%\n", total_cpu_load * 100.0);
			warmup_samples++;
		}
		else
		{
			cpu_load[cpu_samples % n_samples] = total_cpu_load;
			cpu_samples++;
			if (cpu_samples % 5) {
				sprintf(print_out, "[%d] System load:%5.1f%%", getpid(), total_cpu_load * 100.0);

				if (do_bonding) {
					for (i = 0; i < nr_cpus; i++)
						sprintf(print_out + strlen(print_out), " || CPU%d: %5.1f%%", i, this_cpu_load[i] * 100.0);
				}
				dbprintf("%s\n", print_out);
			}
		}
	}
}

static void prep_cyclesoak(void)
{
	struct itimerval it = {
		{ period_secs, 0 },
		{ 1, 0 },
	};
	FILE *f;
	char buf[80];
	int cpu;
	cpu_set_t cpu_set;
	int i;

	if (!do_calibrate) {
		f = fopen(CPS_FILE, "r");
		if (!f) {
			fprintf(stderr, "Please run `cyclesoak -C' on an unloaded system\n");
			exit(1);
		}

		for (i = 0; i < nr_cpus; i++) {
			if (fgets(buf, sizeof(buf), f) == 0) {
				fprintf(stderr, "Reading calibration file failed!\n");
				exit(1);
			}
			counts_per_sec[i] = strtoul(buf, 0, 10);
			if (counts_per_sec[i] == 0) {
				fprintf(stderr, "Calibration implies 0 idle time. Likely a bug.\n");
				exit(1);
			}
			d_counts_per_sec[i] = counts_per_sec[i];
		}
		fclose(f);
	}

	busyloop_buf = malloc(busyloop_size);
	if (busyloop_buf == 0) {
		fprintf(stderr, "busyloop_buf: no mem\n");
		exit(1);
	}
	busyloop_progress = mmap(0, nr_cpus * CACHE_LINE_SIZE * sizeof(long),
				PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
	if (busyloop_progress == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	memset(busyloop_progress, 0, nr_cpus * CACHE_LINE_SIZE);

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if ((kids[cpu]=fork()) == 0) {
			if (setpriority(PRIO_PROCESS, getpid(), 40)) {	/* As low as as we can go */
				perror("setpriority");
				exit(1);
			}
			if (do_bonding) {
				CPU_ZERO(&cpu_set);
				CPU_SET(cpu, &cpu_set);
				sched_setaffinity(kids[cpu], sizeof(cpu_set), &cpu_set);
			}
			busyloop(cpu);
		}

	}

	signal(SIGALRM, itimer);
	gettimeofday(&alarm_time, 0);
	if (setitimer(ITIMER_REAL, &it, 0)) {
		perror("setitimer");
		exit(1);
	}
}

static void calibrate(void)
{
	int i;

	prep_cyclesoak();

	for ( ; ; ) {
		sleep(10);
		if (calibration_complete) {
			FILE *f = fopen(CPS_FILE, "w");
			if (f == 0) {
				fprintf(stderr, "error opening `%s' for writing\n", CPS_FILE);
				perror("fopen");
				exit(1);
			}
			for (i = 0; i < nr_cpus; i++) {
				fprintf(f, "%lu\n", calibrated_loops_per_sec[i]);
			}
			fclose(f);
			return;
		}
	}
}

/* once we have received the signal to stop running this fn will exit */
static void do_cyclesoak(void)
{
	prep_cyclesoak();
	while (running)
		sleep(2);
}

static void exit_handler(void)
{
	int i;
	for (i=0; i < nr_cpus; i++){
		dbprintf("Killing %d.\n", kids[i]);
		kill(kids[i], SIGKILL);
	}
}


/* Parse commands from an argument of the form
 * cmd1=val1,cmd2=val2...
 */
static int parse_arg(char *arg)
{
	char *c, *cmd, *val;
	char *arg_ptr = NULL, *cmd_ptr = NULL;

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
		else if (!strcmp(cmd, "period"))
			period_secs = strtol(val, (char**)NULL, 10);
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
cpu_target_controller_setup(char *arg)
{
	return 0;
}

int
cpu_target_setup (char *arg)
{
	/* disable timer */
	struct itimerval it = {{0,0},{0,0},};
	dbprintf ("Setup test.\n");

	if (strlen(arg) != 0)
		if (parse_arg(arg))
			return -1;

        nr_cpus = count_cpus();
        if (nr_cpus < 0)
                return -1;

	/* 
	 * We need to take enough cycles so we can disregard the
	 * cooldown time at completion.  If we don't have a cooldown
	 * period, take 3600 samples (i.e. one hour at one per second)
	 */
	if (cooldown_time)
		n_samples = (2 * ((cooldown_time + 1) / period_secs));
	else 
		n_samples = 3600;
	dbprintf("Averaging (up to) %d samples\n", n_samples);
	cpu_load = malloc(n_samples * sizeof(double));

	/* calibrate & stop */
	do_calibrate = 1;
	calibrate();
	do_calibrate = 0;
	dbprintf("calibrated OK. %lu loops/sec\n", calibrated_loops_per_sec[0]);
	exit_handler();

	/* stop the timer firing every second and clear any counters */
	cpu_samples = 0;
	if (setitimer(ITIMER_REAL, &it, 0)) {
		perror("setitimer");
		exit(1);
	}

	return 0;
}

int
cpu_target_start(struct timeval *start)
{
	gettimeofday(start, NULL);
	dbprintf("Starting.\n");
	do_cyclesoak();
	dbprintf("Complete.\n");
	exit_handler();
	return 0;
}

int
cpu_target_stop(struct timeval *stop)
{
	gettimeofday(stop, NULL);
	running = 0;
	dbprintf("Stopping.\n");
	return 0;
}

int
cpu_target_marshall(char **data, int *size, double running_time)
{
	static char buf[BUFSIZ];
        char *bp;

	double av = average_cpu();

	snprintf(buf, BUFSIZ, "%.1f\n", av*100);
        bp = buf;
	dbprintf("Average CPU time is %5.1f%%.\n", av*100);
        for (int i = 0; i < nr_cpus; i++) {
            int len;
            bp = strdup(bp);
            snprintf(buf, BUFSIZ, ",CPU%d: %.1f", i, cpu_load[i]);
            len = strlen(bp) + strlen(buf) + 1;
            bp = realloc(bp, len);
            strncat(bp, buf, len);
        }

	*data = bp;
	*size = strlen(bp) + 1;

	return 0;
}


void
cpu_target_marshall_cleanup(char **data)
{
	free(cpu_load);
}


int
cpu_target_unmarshall(char *input, int input_len, char **data,
		  int *data_len)
{
	dbprintf("[cpu_target_unmarhsall] start\n");
	char *buf = calloc(sizeof(char),input_len);
	*data = buf;
	memcpy(buf, input, input_len);
	*data_len = input_len;
	dbprintf("[cpu_target_unmarshall] cpu usage %s\n", (char*)(*data));
	return 0;
}


void
cpu_target_unmarshall_cleanup(char **data)
{
	free(*data);
}


int 
cpu_target_output(struct client_data *data) 
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
	.name = "cpu_target",
	.id = 0x20,
	.descr = "Measure CPU usage on the target",
	.default_port = 7,
	.setup = &cpu_target_setup,
	.setup_controller = &cpu_target_controller_setup,
	.start = &cpu_target_start,
	.stop = &cpu_target_stop,
	.marshall = &cpu_target_marshall,
	.marshall_cleanup = &cpu_target_marshall_cleanup,
	.unmarshall = &cpu_target_unmarshall,
	.unmarshall_cleanup = &cpu_target_unmarshall_cleanup,
	.output = &cpu_target_output,
};

