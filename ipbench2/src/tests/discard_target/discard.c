#include "../latency/latency.h"
#include "discard.h"
#include <assert.h>
#include <fcntl.h>
#include <net/if.h>

#include "../latency/raw.h"


/* Globals and their defaults */
static int fd;

static unsigned long long bps=10000000;	/* Throughput to attempt            */
static unsigned size=64;		/* Size of chunks to send           */
static unsigned long long warmup=10;	/* Warmup for 10 seconds 	    */
static unsigned long long cooldown=10;  /* Cooldown for 10 seconds 	    */
static unsigned long long test_time=10;  /* run testfor 10 seconds 	    */
static char ifname[IFNAMSIZ] = "eth1";
char sockopts[200];

short protocol;

struct discard_result {
	uint64_t transmitted_bytes;
	uint64_t microseconds;
	uint64_t bps_sent;
	uint64_t sends;
	uint64_t size;
	uint64_t bps_requested;
	uint64_t eagains;
	uint64_t badsends;
} result;

const struct args {
	const char *fmt;
	void *varp;
} arg_desc[] = {
	{"bps=%llu%n", &bps},
	{"size=%u%n", &size},
	{"warmup=%llu%n", &warmup},
	{"cooldown=%llu%n", &cooldown},
	{"test_time=%llu%n", &test_time},
	{"if=%16[^, ]%n", ifname},
	{"proto=%hu%n", &protocol},
	{0,0}
};



int
parse_args(char *arg)
{
	int i;
	int nchar;
	int count = 0;

	dbprintf("discard parse arg \"%s\"\n", arg);
	while (*arg && *arg != ' ') {
		for (i = 0; arg_desc[i].fmt; i++) {
			count = sscanf(arg, arg_desc[i].fmt, arg_desc[i].varp, &nchar);
			if (count) {
				dbprintf("Arg %s\n", arg_desc[i].fmt);
				arg += nchar;
				if (arg[0] == ',')
					arg++;
				break;
			}
		}
		if (count)
			continue;
		dbprintf("Unrecognised argument %s\n", arg);
		return 1;
	}
	return 0;
}

static int
generate_load(int sock)
{
	uint64_t sid = 0, predicted_sends, sends;
	ssize_t tosend = 0;
	uint64_t sent_packets;
	/* start_time < warmup_time < cooldown_time < end_time */
	uint64_t start_time, warmup_time, cooldown_time, end_time, cooldown_period;
	uint64_t now;
	uint64_t real_start = 0, real_end = 0;
	char *sbuf = NULL;
	double send_rate;

	uint64_t send_eagains=0, broken_sends=0;

	int s;
	/* initialisation */
	raw_setup_packet(&sbuf, size);

	assert(sbuf != NULL);


	sent_packets = 0; /* used for actual benchmark time */
	sends = 0; /* used for rate control */

	/* work out the send interval we should be using */
	start_time = time_stamp();
	warmup_time = start_time + usec_to_tick(warmup * 1000000);
	cooldown_time = warmup_time + usec_to_tick(test_time * 1000000);
	cooldown_period = usec_to_tick(cooldown * 1000000);
	send_rate = (double)bps / (8.0 * US_PER_S * size * tick_rate);

	dbprintf("About to start sending.  send_rate is %g, start_time is %llu, warmup_time %llu, cooldown_time %llu\n",
		 send_rate, start_time, warmup_time, cooldown_time);

	/* start sending */

	/* XXX: you will have outstanding requests from the warmup
	 * period ... sends will be less than recvs.
	 */

	end_time = 0;
	now = time_stamp();
	while (end_time == 0 || now < end_time) {

		predicted_sends = (now - start_time) * send_rate;

		if (predicted_sends > sends) {
			if (tosend == 0){
				raw_fill_packet(sbuf, sid);
                                sid++;
				tosend = size;
			}

			s = raw_send_packet(&sbuf[size - tosend], tosend, 0);

			if (s < 0) {
				if (errno != EAGAIN){
					printf("ERRNO IS %d\n",errno);
					perror("send");
					assert(!"unhandled send error");
				}
				send_eagains++;
				continue;
			}
			tosend -= s;

			if (tosend != 0) {
				dbprintf("tosend = %d\n", tosend);
				broken_sends++;
			} else {
				/* if we are measuring */
				if (now >= warmup_time && cooldown_time != 0){
					if (real_start == 0) {	
						real_start = now;
						cooldown_time += now - warmup_time;
						dbprintf("now >= warmup_time (%llu), real_start = %llu\n",
							 now - warmup_time,
							 real_start);
					}
					sent_packets++;
				}
				sends++;
  			}
		}

		if (cooldown_time && now > cooldown_time) {
			/* start cooldown period */
			dbprintf("now > cooldown time (%lu), real_end = %llu\n",
				 now - cooldown_time, now);
			cooldown_time = 0;
			real_end = now;
			end_time = now + cooldown_period;
		}

		now = time_stamp();
	}

	/* now clag the important results into the results
	 * structure */

	dbprintf("real_start = %llu, real_end = %llu, start_time = %llu,"
		 "warmup_time = %llu, cooldown_period = %llu\n",
		 real_start, real_end, start_time, warmup_time, cooldown_period);

	result.size = size;
	result.transmitted_bytes = sent_packets * size;
	result.microseconds = tick_to_usec(real_end - real_start);
	result.bps_sent =
		(8 * sent_packets * size * US_PER_S) / result.microseconds;
	result.sends = sent_packets;
	result.eagains = send_eagains;
	result.badsends = broken_sends;

#ifdef DEBUG
	/* if debugging the client will figure out its results and print them */

	dbprintf("\n");

	dbprintf("Sent %llu packets in %llu microseconds\n", sent_packets, result.microseconds);
	dbprintf("EAGAIN - send %"PRId64
		 " | RETRYS send %"PRId64"\n",
		 send_eagains, broken_sends);

#endif
	return 0;
}

int discard_setup(char *hostname, int port, char *arg)
{
	int flag;
	int fd;

	dbprintf("discard setup begin (target %s [port %d]).\n", hostname, port);

	protocol = ETH_P_IPBENCH + 1;
	if (parse_args(arg))
		return -1;

	if ((fd = raw_setup_socket(hostname, port, ifname)) < 0)
		return -1;
        set_socket_options(fd, sockopts);
        microuptime_calibrate();
//	fcntl(fd, F_SETFL, O_NONBLOCK);
	return 0;
}



int discard_start(struct timeval *start)
{
	dbprintf("Discard test start.\n");
	gettimeofday(start, NULL);
	generate_load(fd);
	dbprintf("Discard test complete.\n");
	return 0;
}

int discard_stop(struct timeval *stop)
{
	dbprintf("Discard test stop.\n");
	gettimeofday(stop, NULL);
	close(fd);
	return 0;
}

int discard_marshal(void **data, size_t *size, double running_time)
{
	struct discard_result *tosend;


	tosend = malloc(sizeof *tosend);
	if (tosend == NULL)
		printf("Can't malloc %u bytes", sizeof *tosend);


	tosend->size    = htonll(result.size);
	tosend->microseconds    = htonll(result.microseconds);
	tosend->sends   = htonll(result.sends);
	tosend->bps_requested = htonll(bps);
	tosend->bps_sent = htonll(result.bps_sent);
	tosend->transmitted_bytes = htonll(result.transmitted_bytes);
	tosend->eagains = htonll(result.eagains);
	tosend->badsends = htonll(result.badsends);

	*data = tosend;
	*size = sizeof *tosend;

	return 0;
}

void discard_marshal_cleanup(void **data)
{
	free(*data);
}

/*
 * Run in ipbench
 */
int discard_unmarshal(void *input, size_t input_len, void **data,
		       size_t *data_len)
{
	struct discard_result *result = (struct discard_result *)(input);
	struct discard_result *theresult;

	dbprintf("Discard unmarshall arguments.\n");

	if (input_len != sizeof (struct discard_result)) {
		*data = NULL;
		*data_len = 0;
		dbprintf("discard_unmarshal: expected %d byte message, got %d\n",
			 sizeof (struct discard_result), input_len);
		return 1;
	}

	*data_len = sizeof(struct discard_result);
	theresult = *data = malloc(*data_len);
	if (*data == NULL)
		printf("Out of buffer space.\n");

	/* convert theresult back to something sensible for us */
	theresult->microseconds    = ntohll(result->microseconds);
	theresult->size    = ntohll(result->size);
	theresult->sends   = ntohll(result->sends);
	theresult->bps_requested = ntohll(result->bps_requested);
	theresult->bps_sent = ntohll(result->bps_sent);
	theresult->transmitted_bytes = ntohll(result->transmitted_bytes);
	theresult->badsends = ntohll(result->badsends);
	theresult->eagains = ntohll(result->eagains);

	return 0;
}

void discard_unmarshal_cleanup(void **data)
{
	free(*data);
	*data = NULL;
}

int discard_output(struct client_data *target_data, struct client_data data[], int nelem)
{

	int i = 0;
	uint64_t total_requested_throughput = 0;
	uint64_t total_sent_throughput = 0;
	uint64_t packet_size = 0;
	uint64_t totalbytes = 0;
	uint64_t microseconds = 0;
	struct discard_result *theresult;

	dbprintf("Discard begin output function\n");

	for (i = 0; i < nelem; i++)
	{
		theresult = (struct discard_result *)(data[i].data);

		if (!data[i].valid) {
			dbprintf("client %d: data.valid zero\n", i);
			return 1;
		}
		dbprintf("Client %d thoughput %"PRIu64"bps (%"PRIu64" bytes in %"PRIu64"usec)\n", i, theresult->bps_sent, theresult->transmitted_bytes, theresult->microseconds);
		total_requested_throughput += theresult->bps_requested;
		total_sent_throughput += theresult->bps_sent;
		totalbytes += theresult->transmitted_bytes;
		packet_size = theresult->size;
		microseconds = theresult->microseconds;
		if (theresult->badsends || theresult->eagains)
			dbprintf("Client %d: %"PRIu64" eagains, %"PRIu64" broken sends\n",
				 i, theresult->eagains,theresult->badsends);
	}

#if 1 // pretty print
 	dbprintf("\n-TOTAL-\n");
 	dbprintf("transferred %"PRId64" bytes in %"PRId64" microseconds\n",
 		 totalbytes, microseconds);
 		 dbprintf("achieved %"PRId64" bps\n", total_sent_throughput);
 	dbprintf("\n");
#endif
	printf("%lu,%lu,%lu",
	       total_requested_throughput, total_sent_throughput, packet_size);
	if (target_data != NULL) {
		printf(",%s", (char*)target_data->data);
	}
	printf("\n");

	return 0;
}

int discard_setup_controller(char *arg)
{
	if (arg) {
		dbprintf("Setting up controller (%s)\n", arg);
		/* a bit of a hack ... */
		if (parse_args(arg))
			return -1;
	}
	return 0;
}

struct ipbench_plugin ipbench_plugin = 
{
	.magic = "IPBENCH_PLUGIN",
	.name = "discard",
	.id = 0x05,
	.descr = "Discard Tests",
	.default_port = 7,
	.setup = &discard_setup,
	.setup_controller = &discard_setup_controller,
	.start = &discard_start,
	.stop = &discard_stop,
	.marshall = &discard_marshal,
	.marshall_cleanup = &discard_marshal_cleanup,
	.unmarshall = &discard_unmarshal,
	.unmarshall_cleanup = &discard_unmarshal_cleanup,
	.output = &discard_output,
};

