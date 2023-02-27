#include <inttypes.h>
#include "nfs_latency.h"
#include "nfs_glue.h"
#include <math.h>

/*
 * setup, start, stop marshall, marshall cleanup on the client
 * all others on the controller
 */

static struct result{
	uint64_t microseconds;
	uint64_t sends;
	uint64_t recvs;
	uint64_t rtt_av;
	uint64_t rtt_min;
	uint64_t rtt_max;
	uint64_t rtt_std;
	uint64_t rtt_med;
} result;

/* We marshall the result up into this structure */
struct marshalled_result {
	uint64_t time;
	uint64_t samples;
	uint64_t sends;
	uint64_t recvs;
	uint64_t data[];
};

struct nfs_options{
	char *path;
	char *filename;
	uint64_t rate;
	uint64_t warmup; /* usec */
	uint64_t cooldown; /* usec */
	uint64_t samples; /* count */
} nfs_options;

static int parse_arg(char *arg)
{
	char *p, cmd[200], *val;

	/* set up default values */
	nfs_options.path = strdup("/tmp/%h");
	nfs_options.filename = strdup("bench.file");
	nfs_options.rate = 10000; /* per second */
	nfs_options.cooldown = 0;
	nfs_options.warmup = 0;
	nfs_options.samples = 20000;

	dbprintf("parse_arg(%s)\n", arg);
	/* parse parameters */
	while (arg && next_token(&arg, cmd, " \t,")){
		if ((p = strchr(cmd, '='))) {
			*p = '\0';
			val = p+1;
		} else {
			val = NULL;
		}

		/* test arguments */
		dbprintf("Got cmd %s val %s\n", cmd, val);

		if (!strcmp(cmd, "path")){
			free(nfs_options.path);
			nfs_options.path = strdup(val);
			continue;
		}
		if (!strcmp(cmd, "filename")){
			free(nfs_options.filename);
			nfs_options.filename = strdup(val);
			continue;
		}
		if (!strcmp(cmd, "rate")){
			nfs_options.rate = strtoll(val, NULL, 10);
			continue;
		}

		if (!strcmp(cmd, "warmup")) {
			nfs_options.warmup = strtoll(val, (char**)NULL, 10) * US_PER_S;
			continue;
		}
		if (!strcmp(cmd, "cooldown")) {
			nfs_options.cooldown = strtoll(val, (char**)NULL, 10) * US_PER_S;
			continue;
		}
		if (!strcmp(cmd, "samples")) {
			nfs_options.samples = strtoll(val, (char**)NULL, 10);
			continue;
		}
			
		
		dbprintf("Invalid argument %s=%s.\n", cmd, val);
	}


	dbprintf("Scanning path %s for %%h\n", nfs_options.path);
	if ((p = strchr(nfs_options.path, '%'))) {
		if (*++p == 'h') {
			char name[200];
			*p='s';
			gethostname(name, sizeof name);
			snprintf(cmd, sizeof cmd, nfs_options.path, name);
			dbprintf("Expanded to %s\n", cmd);
			free(nfs_options.path);
			nfs_options.path = strdup(cmd);
		}
	}
	dbprintf("Scanning path %s for %%h\n", nfs_options.filename);
	if ((p = strchr(nfs_options.filename, '%'))) {
		if (*++p == 'h') {
			char name[200];
			dbprintf("Found %%h at %d\n", p-nfs_options.filename);
			*p='s';
			gethostname(name, sizeof name);
			snprintf(cmd, sizeof cmd, nfs_options.filename, name);
			free(nfs_options.filename);
			nfs_options.filename = strdup(cmd);
		}
	}

	dbprintf("parse_arg: using: path=%s,filename=%s,rate=%lld\n",
	         nfs_options.path,
	         nfs_options.filename,
	         nfs_options.rate);
	dbprintf("parse_arg: using: warmup = %lld, cooldown = %lld, nsamples = %lld\n",
	         nfs_options.warmup,
	         nfs_options.cooldown,
	         nfs_options.samples);

	return 0;
}

/*
 * Setup your test.  After this is complete you should be ready to
 * start; if you need a warmup period etc do it in start(); just don't
 * timestamp / start collecting statistics before you are ready.
 */
int
nfs_latency_setup(char *hostname, int port, char *arg)
{
	int x;

	dbprintf("nfs_latency_setup - %s - %d - %s\n", hostname, port, arg);

	/* hostname is the machine you want to hit with nfs traffic.  
	 * port you can ignore.
	 * arg is just a string that you need to parse however you want
	 */

	/* configure nfs options */
	dbprintf("Calling parse_arg(%s)\n", arg);
	if (parse_arg(arg)) {
	      dbprintf("parse_arg returned non-zero\n");
	      return -1;
	}

	x = init_and_open(hostname, nfs_options.path, nfs_options.filename);

	return x;
}

int
nfs_latency_setup_controller(char *arg)
{
	dbprintf("nfs_latency_setup_controller - %s\n", arg);
	return 0;
}

uint64_t samples;
uint64_t* sample;
int nfs_latency_finished = 0;

/*
 * Start your test -- timestamp start with gettimeofday() as the last
 * step before you start the testing propper.  This function should
 * *not* return until you want the stop() fn called.
 */
int
nfs_latency_start (struct timeval *start)
{
	int i=0;
	int target=nfs_options.samples;
	int r;
	uint64_t now, then, delta, cooldown;
	uint64_t start_time, warmup_time, cooldown_time, end_time;
	/* start_time < warmup_time < cooldown_time < end_time */
	uint64_t requests, replies;
	uint64_t predicted_requests;
	uint64_t offset;

	dbprintf("NFS_LATENCY START\n");
	gettimeofday(start, NULL);
	dbprintf("NFS latency start warmup,\n");

	if (nfs_latency_finished==0){
		/* allocate memory for samples */
		samples = target;
		sample = malloc(sizeof(uint64_t) * target);
	}else{
		/* re-use old samples */
		nfs_latency_finished = 0;
	}

	start_time = time_stamp();
	offset = requests = replies = 0;
	warmup_time = start_time + usec_to_tick(nfs_options.warmup);
	cooldown_time = end_time = 0; /* calculated later */
	cooldown = usec_to_tick(nfs_options.cooldown);
	
	while ((now = time_stamp()) && (end_time == 0 || now < end_time)) {

		delta = tick_to_usec(now - start_time);
		predicted_requests = (delta * nfs_options.rate) / US_PER_S;

		if (predicted_requests > requests){
			r = generate_request(now);
			if (r > 0){
				requests++;
			} else {
				//dbprintf("generate request failed\n");
			}
		}

		if ((process_reply(&then) == 0) && (now > warmup_time) && (cooldown_time == 0)){
			if (offset == 0) {
				dbprintf("Warmup done, start test\n");
				offset = requests;
			}
			replies++;
			sample[i] = now - then;
			i++;
		}

		if (replies == target && cooldown_time == 0) {
			cooldown_time = now;
			result.sends = requests - offset;
			end_time = now + cooldown;
			dbprintf("Starting cooldown; now=%lld, end=%lld\n",
				 now, end_time);
		}
	}

	result.microseconds = tick_to_usec(cooldown_time - warmup_time);
	result.recvs = replies;	

	nfs_latency_finished = 1;
	dbprintf("NFS latency test complete.\n");

	return 0;
}

/*
 * Called on test stop.  Timestamp stop with gettimeofday() as the
 * last step before you return
 */
int
nfs_latency_stop (struct timeval *stop)
{
	gettimeofday (stop, NULL);
	dbprintf ("NFS_LATENCY STOP\n");
	nfs_lat_cleanup();
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
nfs_latency_marshall (char **data, int * size, double running_time)
{
	uint64_t i;
	struct marshalled_result *tosend;

	dbprintf("nfs_latency marshall arguments.\n");

	tosend = malloc(sizeof(struct marshalled_result) + 
	                sizeof(uint64_t) * samples);

	if (tosend == NULL){
		dbprintf("Can't malloc %d bytes\n",
		         sizeof(struct marshalled_result) +
		         sizeof(uint64_t) * samples);
		return -1;
	}

	for (i = 0; i < samples; i++){
		tosend->data[i] = htonll(tick_to_usec(sample[i]));
	}

	tosend->time = htonll(result.microseconds);
	tosend->samples = htonll(samples);
	tosend->sends = htonll(result.sends);
	tosend->recvs = htonll(result.recvs);

	*data = (char*)tosend;
	*size = sizeof(struct marshalled_result) + (sizeof(uint64_t)*samples);

	return 0;
}

/*
 * After arguments have been marshalled this is called with the data
 * passed in, in case you want to free it or something
 */
void
nfs_latency_marshall_cleanup(char **data)
{
	free(*data);
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
nfs_latency_unmarshall(char *input, int input_len, char **data,
                       int *data_len)
{
	uint64_t i;
	struct marshalled_result *theresult;

	theresult = (struct marshalled_result *)(input);
	theresult->time = ntohll(theresult->time);
	theresult->samples = ntohll(theresult->samples);
	theresult->sends   = ntohll(theresult->sends);
	theresult->recvs   = ntohll(theresult->recvs);

#if 0
	printf("got %lld time\n", theresult->time);
	printf("got %lld samples\n", theresult->samples);
	printf("got %lld sends\n", theresult->sends);
	printf("got %lld recvs\n", theresult->recvs);
#endif

	for (i = 0; i < theresult->samples; i++){
		theresult->data[i] = ntohll(theresult->data[i]);
	}

	*data_len = sizeof(struct marshalled_result) +
	            (sizeof(uint64_t) * theresult->samples);
	*data = malloc(*data_len);
	if (*data == NULL){
		dbprintf("Out of buffer space.\n");
		return -1;
	}

	assert(input_len == *data_len);

	memcpy(*data, input, input_len);

	return 0;
}

/*
 * You are passed back your data pointer from above here in case you
 * need to free it or do any other cleaning up
 */
void
nfs_latency_unmarshall_cleanup(char **data)
{
	free(*data);
}

/* needed for qsort */
static int
compare_uint64(const void *a, const void *b){
	uint64_t x, y;

	x = *(uint64_t *)a;
	y = *(uint64_t *)b;

	if (x < y)
		return -1;

	if (x > y)
		return 1;

	return 0;
}

static uint64_t
average_uint64(uint64_t *x, unsigned n, double *stddevp){
	double sumsqr;
	uint64_t total, new_total;
	unsigned i;
	double stddev;

	assert(n>1);

	total = 0;
	sumsqr = 0.0;
	for (i = 0; i < n; i++){
		/* guard against overflow */
		new_total = total + x[i];
		sumsqr += x[i] * x[i];
		assert(new_total >= total);
		total = new_total;
	}

	stddev = sqrt((sumsqr - (double)(total*total)/n)/(n-1));
	*stddevp = stddev;
	return (total / n);
}

static double
absolute_deviation_uint64(uint64_t *samples, unsigned n, uint64_t median)
{
	unsigned i;
	int64_t d;
	double deviation = 0.0;
	for (i = 0; i < n; i++) {
		d = *samples++ - median;
		deviation += d > 0 ? d : -d;
	}
	return deviation / n;
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
nfs_latency_output(struct client_data data[], int nelem)
{
	int i;
	uint64_t aggregate_count=0, *aggregate=NULL, *tmp;
	uint64_t *runtime=NULL;
	uint64_t avg_latency, avg_runtime;
	uint64_t tot_requests=0, tot_replies=0;
	double stddevR, stddevL;

	dbprintf("NFS_LATENCY OUTPUT (nelem %d)\n", nelem);

	runtime = malloc(nelem*sizeof(uint64_t));
	assert(runtime!=NULL);

	printf("#");
	printf("Achieved request rate,");
	printf("Achieved reply rate,");
	printf("Min latency,");
	printf("Average latency,");
	printf("Stddeviation,");
	printf("Median latency,");
	printf("Average abs. deviation,");
	printf("Max latency,");
	printf("Samples,");
	printf("Min runtime,");
	printf("Average runtime,");
	printf("Median runtime,");
	printf("Std dev runtime,");
	printf("Max runtime");
	printf("\n");

	for (i = 0; i < nelem; i++) {
		int j;
		struct marshalled_result *theresult;

		theresult = (struct marshalled_result*)data[i].data;

#if 0
		printf("load generator %d...\n", i);
		printf("size is %d\n", data[i].size);
		printf("valid is %d\n", data[i].valid);
		printf("time=%lld\n", theresult->time);
		printf("requests=%lld\n", theresult->sends);
		printf("reples=%lld\n", theresult->recvs);
		printf("samples=%lld\n", theresult->samples);
#endif

		tot_requests += theresult->sends;
		tot_replies += theresult->recvs;

		/* aggregate runtime */
		runtime[i] = theresult->time;

		/* aggreagate samples */
		assert(theresult->samples>0);
		tmp = realloc(aggregate,
		              (aggregate_count + theresult->samples)
		              * sizeof(uint64_t));
		if(tmp==NULL){
			dbprintf("Out of memory!\n");
			return -1;
		}
		aggregate = tmp;

		for(j=0; j<theresult->samples; j++){
			aggregate[aggregate_count++] = theresult->data[j];
		}
	}


	/* sort the samples */

	qsort(aggregate, aggregate_count, sizeof(uint64_t), compare_uint64);
	/* Discard 0-length samples */
	for (i = 0; i < aggregate_count && aggregate[i] == 0; i++)
		;

	aggregate += i;
	aggregate_count -= i;
	avg_latency = average_uint64(aggregate, aggregate_count, &stddevL);

	/* sort the runtimes */
	qsort(runtime, nelem, sizeof(uint64_t), compare_uint64);
	/* Discard 0-length samples */
	for (i = 0; i < nelem && runtime[i] == 0; i++)
		;
	
	runtime += i;
	nelem -= i;
	avg_runtime = average_uint64(runtime, nelem, &stddevR);

	printf("%" PRIu64 ",", (tot_requests*1000000) / avg_runtime);
	printf("%" PRIu64 ",", (tot_replies*1000000) / avg_runtime);

	printf("%" PRIu64 ",", aggregate[0]);
	printf("%" PRIu64 ",", avg_latency);
	printf("%g,", stddevL);
	printf("%" PRIu64 ",", aggregate[aggregate_count/2]);
	printf("%g,", absolute_deviation_uint64(aggregate, aggregate_count, aggregate[aggregate_count/2]));
	printf("%" PRIu64 ",", aggregate[aggregate_count-1]);
	printf("%" PRIu64 ",", aggregate_count);

	printf("%" PRIu64 ",", runtime[0]);
	printf("%" PRIu64 ",", avg_runtime);
	printf("%g,", stddevR);
	printf("%" PRIu64 ",", runtime[nelem/2]);
	printf("%" PRIu64, runtime[nelem-1]);
	printf("\n");

	dbprintf ("NFS_LATENCY DONE\n");
	return 0;
}
