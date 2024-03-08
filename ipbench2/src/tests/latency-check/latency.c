/*
 * Latency Test for ipbench
 * Luke Macpherson <lukem@cse.unsw.edu.au>
 * Worked into ipbench by Ian Wienand <ianw@gelato.unsw.edu.au>
 * This version expects the echo server to update the 2nd word in the packet,
 * and checks it gets what it expects.
 * See COPYING for license terms
 * (C) 2003 DISY/Gelato@UNSW
 * (C) 2004 NICTA
 */
#include "latency.h"

/*
 * we define a number of different "layers" for either tcp/udp or raw
 * (and possibly others in the future, I guess).  tcp/udp share most
 * of their functions.
 */

struct layer_opts *layer;
struct layer_opts tcp_layer =
{
	.setup_socket = tcp_setup_socket,
	.setup_packet = ip_setup_packet,
	.fill_packet = ip_fill_packet,
	.read_packet = ip_read_packet,
	.send_packet = ip_send_packet,
	.recv_packet = ip_recv_packet,
};

struct layer_opts udp_layer =
{
	.setup_socket = udp_setup_socket,
	.setup_packet = ip_setup_packet,
	.fill_packet = ip_fill_packet,
	.read_packet = ip_read_packet,
	.send_packet = ip_send_packet,
	.recv_packet = ip_recv_packet,
};

#ifdef CONFIG_RAW_LINUX
struct layer_opts raw_layer =
{
	.setup_socket = raw_setup_socket,
	.setup_packet = raw_setup_packet,
	.fill_packet = raw_fill_packet,
	.read_packet = raw_read_packet,
	.send_packet = raw_send_packet,
	.recv_packet = raw_recv_packet,
};
#endif

static struct latency_result result;

/* Globals and their defaults */
static int fd;
static uint64_t samples = SAMPLES;      /* Samples required. A sample is one 
					 * packet that has made a round trip
					 */
static uint64_t bps=10000000;		/* Throughput to attempt            */
static uint64_t size=64;		/* Size of chunks to send           */
static uint64_t warmup=10*US_PER_S;	/* Warmup for 10 seconds 	    */
static uint64_t cooldown=10*US_PER_S;   /* Cooldown for 10 seconds 	    */
static uint64_t drop_threshold=1;       /* Threshold before dropping a packet with UDP */
static uint64_t socktype=SOCKTYPE_TCP;  /* By default use TCP               */

static uint64_t *rtt_client_sample;     /* an array of size 'samples' to 
					 *  hold our results 
					 */
static uint64_t *rtt_sample;            /* ptr to current sample in
					 * rtt_client_sample
					 */

short protocol = ETH_P_IPBENCH;
static char ifname[IFNAMSIZ];
char sockopts[200];
static char *dump_dir, *dump_prefix, dump_filename[MAXPATHLEN];

static struct packet_list_node *packet_list = NULL;
static uint64_t packet_list_length = 0;
static uint64_t packet_list_head = 0;

/* We print output slightly differently if running with the CPU target */
static int with_cpu_target = 0;

static inline uint64_t
packet_list_add(uint64_t packet_id, uint64_t send_time)
{
	uint64_t drops;

	packet_list_head = (packet_list_head + 1) % packet_list_length;

	drops = packet_list[packet_list_head].packet_id != 0;

	packet_list[packet_list_head].packet_id = packet_id;
	packet_list[packet_list_head].send_time = send_time;

	return drops;
}

static inline uint64_t
packet_list_del(uint64_t packet_id)
{
	uint64_t offset, index, r;

	offset = packet_list[packet_list_head].packet_id - packet_id;

	/* packet is too old */
	if (offset >= packet_list_length)
		return 0;

	if (packet_list_head >= offset) {
		index = packet_list_head - offset;
	} else {
		index = packet_list_length - (offset - packet_list_head);
	}

	assert(index < packet_list_length);

	if (packet_list[index].packet_id == packet_id) {
		r = packet_list[index].send_time;
		packet_list[index].packet_id = 0;
		packet_list[index].send_time = 0;
	} else {
		r = 0;
	}

	return r;
}

static int compare_rtt(const void *x, const void *y)
{
	const uint64_t *rtt_x, *rtt_y;

	rtt_x = x;
	rtt_y = y;

	if (*rtt_x < *rtt_y) {
		return -1;
	} 
	if (*rtt_x > *rtt_y) {
		return 1;
	}

	return 0;
}

/* All these functions take an 'nsamples' argument, this is so
 * on output we can reuse them by simply passing the number of
 * clients as the nsamples.
 */
static void sort_rtt(int nsamples)
{
	qsort(rtt_sample, (nsamples*samples), sizeof(uint64_t), compare_rtt);
}

static uint64_t med_rtt(int nsamples)
{
	sort_rtt(nsamples);
	return rtt_sample[(nsamples*samples) / 2];
}

static uint64_t avg_rtt(int nsamples)
{
	uint64_t total = 0;
	int i;
	for (i = 0; i < (nsamples*samples); i++) {
		total += rtt_sample[i];
	}

	return (total / (nsamples*samples));
}

static uint64_t dev_rtt(int nsamples)
{
	uint64_t sum, avg, tmp;
	int i;

	avg = avg_rtt(nsamples);

	sum = 0;

	for (i = 0; i < (nsamples*samples); i++) {
		tmp = avg - rtt_sample[i];
		sum += tmp * tmp;
	}

	return (sum / (nsamples*samples));	/* warning, we need the sqrt of this! */
}

static uint64_t min_rtt(int nsamples)
{
	uint64_t min;
	int i;

	min = rtt_sample[0];

	for (i = 1; i < (nsamples*samples); i++) {
		if (rtt_sample[i] < min) {
			min = rtt_sample[i];
		}
	}

	return min;
}

static uint64_t max_rtt(int nsamples)
{
	uint64_t max;
	int i;

	max = rtt_sample[0];

	for (i = 1; i < (nsamples*samples); i++) {
		if (rtt_sample[i] > max) {
			max = rtt_sample[i];
		}
	}

	return max;
}


static int
measure_latency(int sock, uint64_t bps, uint64_t size,
                uint64_t warmup, uint64_t cooldown)
{
	uint64_t sid, rid, predicted_sends, sends, recvs, tosend=0, torecv=0;
	uint64_t sent_packets, received_packets, drops;
	/* start_time < warmup_time < cooldown_time < end_time */
	uint64_t start_time, warmup_time, cooldown_time, end_time;
	uint64_t now, rtt, send_time;
	uint64_t late_packets=0;         /* Missing packets */
	uint64_t dropped_packets=0;      /* Dropped packets */
	uint64_t bad_packets = 0;        /* packets that haven't been updated */
	char *sbuf=NULL, *rbuf=NULL;
	double send_rate;

	uint64_t send_eagains=0, recv_eagains=0, broken_sends=0, broken_recvs=0;

	int r, s;
	/* initialisation */
	layer->setup_packet(&sbuf,size);
	layer->setup_packet(&rbuf,size);
	assert(sbuf != NULL && rbuf != NULL);

	sid = rid = 0;

	sent_packets = received_packets = 0; /* used for actual benchmark time */
	sends = recvs = 0; /* used for rate control */

	/* work out the send interval we should be using */
	start_time = time_stamp();
	warmup_time = start_time + usec_to_tick(warmup);
	cooldown_time = end_time = 0; /* calculated later */
	cooldown = usec_to_tick(cooldown);
	send_rate = (double)bps / (8.0 * US_PER_S * size * tick_rate);

	dbprintf("Calculating send rate (8.0 * %d * %d * %g)\n", US_PER_S, size, tick_rate);

	dbprintf("About to start sending.  send_rate is %g, start_time is %llu, warmup_time %llu, cooldown %llu\n",
		 send_rate, start_time, warmup_time, cooldown);

	/* start sending */

	/* XXX: you will have outstanding requests from the warmup
	 * period ... sends will be less than recvs.
	 */

	while((now = time_stamp()) && (end_time == 0 || now < end_time)) {

		predicted_sends = (now - start_time) * send_rate;

		if (received_packets >= samples && cooldown_time == 0){
			/* start cooldown period */
			cooldown_time = now;
			end_time = cooldown_time + cooldown;
		}

		if (torecv == 0){
			torecv = size;
		}

		do {
			assert(torecv >= 0 && torecv <= size);
			r = layer->recv_packet(&rbuf[size-torecv], torecv, MSG_WAITALL);
			if (r < 0) {
				if (errno != EAGAIN) 
				{
					dbprintf("recv error: %s\n", strerror(errno));
					return -1;
				}
				recv_eagains++;
				break;
			}
			torecv -= r;
			if (torecv != 0) broken_recvs++;
		} while (torecv > 0);

		if (torecv == 0){
			bad_packets += layer->read_packet(rbuf, &rid);
			send_time = packet_list_del(rid);

			/* if we are measuring... */
			if (now >= warmup_time && cooldown_time == 0){
				if(send_time > 0){
					rtt = now - send_time;
					rtt_sample[received_packets] = rtt;
					received_packets++;
				} else {
					late_packets++;
				}
			}
			recvs++;
		}

		if (predicted_sends > sends) {
			if (tosend == 0){
				layer->fill_packet(sbuf, sid);
				/**((uint64_t *) sbuf) = sid;*/
				tosend = size;
			}
			do {
				s = layer->send_packet(&sbuf[size-tosend], tosend, 0);
				if (s < 0) {
					if (errno != EAGAIN){
						printf("ERRNO IS %d\n",errno);
						perror("send");
						assert(!"unhandled send error");
					}
					send_eagains++;
					break;
				}
				tosend -= s;
				if (tosend != 0) broken_sends++;
			} while (tosend > 0);
			/* dbprintf("sent sid %"PRId64"\n", sid); */

			if (tosend == 0){
				drops = packet_list_add(sid, now);
				sid++;
				/* if we are measuring */
				if (now >= warmup_time && cooldown_time == 0){
					dropped_packets += drops;
					sent_packets++;
				}
				sends++;
  			}
		}
	}

        close(layer->s);
	/* now clag the important results into the results
	 * structure */


	result.size = size;
	result.transmitted_bytes = received_packets * size;
	result.microseconds = tick_to_usec(cooldown_time - warmup_time);
	result.bps =
		(8 * received_packets * size * US_PER_S) / result.microseconds;
	result.bps_sent =
		(8 * sent_packets * size * US_PER_S) / result.microseconds;
	result.sends = sent_packets;
	result.recvs = received_packets;

#ifdef DEBUG
	/* if debugging the client will figure out its results and print them */
	result.rtt_min = tick_to_usec(min_rtt(1));
	result.rtt_av = tick_to_usec(avg_rtt(1));
	result.rtt_max = tick_to_usec(max_rtt(1));
	result.rtt_med = tick_to_usec(med_rtt(1));
	result.rtt_std = tick_to_usec(dev_rtt(1));

	dbprintf("\n");
	dbprintf("transferred %"PRId64" bytes in %"PRId64" microseconds\n",
		 result.transmitted_bytes, result.microseconds);
	dbprintf("%"PRId64" recvs %"PRId64" sends (%"PRId64" samples)\n",
		 received_packets, sent_packets, samples);
	dbprintf("%"PRId64" late packets.\n", late_packets);
	dbprintf("%"PRId64" dropped packets.\n", dropped_packets);
	dbprintf("achieved %"PRId64" bps\n", result.bps);
	dbprintf("min rtt was %"PRId64" us\n", result.rtt_min);
	dbprintf("average rtt was %"PRId64" us\n", result.rtt_av);
	dbprintf("max rtt was %"PRId64" us\n", result.rtt_max);
	dbprintf("rtt std dev was %"PRId64" us^2\n", result.rtt_std);
	dbprintf("median rtt was %"PRId64" us\n", result.rtt_med);
	dbprintf("%"PRId64" sends, %"PRId64" receives\n", result.sends, result.recvs);

	dbprintf("EAGAIN - send %"PRId64" recv %"PRId64
		 " | RETRYS send %"PRId64" recv %"PRId64"\n",
		 send_eagains, recv_eagains, broken_sends, broken_recvs);

#endif
	return 0;
}


/* Parse commands from an argument of the form
 * cmd1=val1,cmd2=val2...
 */
static int parse_arg(char *arg)
{
	char *p, cmd[200], *val;

	while (next_token(&arg, cmd, " \t,"))
	  {
		  if ((p = strchr(cmd, '='))) {
			  *p = '\0';
			  val = p+1;
		  } else
			  val = NULL;

		  /* test arguments */
		  dbprintf("Got cmd %s val %s\n", cmd, val);
		  if (!strcmp(cmd, "bps"))
			  bps = strtoll(val, (char**)NULL, 10);
		  else if (!strcmp(cmd, "Mbps"))
			  bps = strtoll(val, (char**)NULL, 10) * 1000000;
		  else if (!strcmp(cmd, "samples"))
			  samples = strtoll(val, (char**)NULL, 10);
		  else if (!strcmp(cmd, "size"))
			  size = strtoll(val, (char**)NULL, 10);
		  else if (!strcmp(cmd, "warmup"))
			  warmup = strtoll(val, (char**)NULL, 10) * US_PER_S;
		  else if (!strcmp(cmd, "cooldown"))
			  cooldown = strtoll(val, (char**)NULL, 10) * US_PER_S;
		  else if (!strcmp(cmd, "socktype")) {
			  if (!strcmp(val, "udp"))
				  socktype = SOCKTYPE_UDP;
			  else if (!strcmp(val, "raw"))
				  socktype = SOCKTYPE_RAW;
		  }
		  else if (!strcmp(cmd, "iface")) {
			  bzero(ifname, IFNAMSIZ);
			  strncpy(ifname, val, IFNAMSIZ - 1);
		  }
		  else if (!strcmp(cmd, "drop")) {
			  drop_threshold = strtoll(val, (char**)NULL, 10);
		  }
		  else if (!strcmp(cmd, "sockopts"))
		  {
			  strncpy(sockopts, val, 199);
                          sockopts[199] = 0;
		  }
		  else if (!strcmp(cmd, "with_cpu_target"))
		  {
			  with_cpu_target = 1;
		  }
		  else if (!strcmp(cmd, "dump_dir")) {
			  struct stat s;
			  if (stat (val, &s) == -1 || ! S_ISDIR (s.st_mode)) {
				  dbprintf("Invalid dump directory (%s)!\n", strerror(errno));
				  return -1;
			  }
			  dump_dir = malloc(strlen(val)+1);
			  strcpy(dump_dir, val);
		  }
		  else if (!strcmp(cmd, "dump_prefix")) {
			  dump_prefix = malloc(strlen(val)+1);
			  strcpy(dump_prefix, val);
		  }
		  else {
			  dbprintf("Invlid argument %s=%s.\n", cmd, val);
			  return -1;
		  }
	  }

	/* only support ethernet sized frames with RAW */
	if (socktype == SOCKTYPE_RAW && size > ETH_FRAME_LEN)
		size = ETH_FRAME_LEN;

	return 0;
}


int latency_setup(char *hostname, int port, char *arg)
{
	dbprintf("Latency setup begin (target %s [port %d]).\n", hostname, port);
	if (strlen(arg) != 0)
		if (parse_arg(arg))
			return -1;

	/* 
	 * number of packets sent in a second. For testing people want
	 * to send packets very slowly ... 
	 */
	packet_list_length = ((bps / 8) / size);
	if (packet_list_length < 1)
		packet_list_length = 1;
	/* mutiplied by the number of packets we can have outstanding */
	packet_list_length *= drop_threshold;

	packet_list = malloc(packet_list_length * sizeof(struct packet_list_node));
	if (packet_list == NULL) {
		dbprintf("Can't malloc %"PRId64" bytes for packet_list.\n", packet_list_length);
		return -1;
	}
	bzero(packet_list, packet_list_length * sizeof(struct packet_list_node));

	/* allocate the sample list */
	rtt_client_sample = malloc(sizeof(uint64_t) * samples);
	if (rtt_client_sample == NULL) {
		dbprintf("Can't malloc %"PRId64" bytes for rtt_client_sample\n", samples * 8);
		return -1;
	}
	bzero(rtt_client_sample, sizeof(uint64_t)*samples);

	/* set the current sample as the first one */
	rtt_sample = rtt_client_sample;

	dbprintf("testing at %"PRId64"bps, %"PRId64" byte chunks, %"PRId64" samples\n", bps, size, samples);

	microuptime_calibrate();

	switch(socktype) {
	case SOCKTYPE_TCP:
		layer = &tcp_layer;
		break;

	case SOCKTYPE_UDP:
		layer = &udp_layer;
		break;

#ifdef CONFIG_RAW_LINUX
	case SOCKTYPE_RAW:
		layer = &raw_layer;
		break;
#endif
	default:
		dbprintf("Invalid socket type (%d)?\n", socktype);
		return -1;
	}

	layer->s = layer->setup_socket(hostname, port, ifname);
	assert( layer->s != -1 );

	fcntl(layer->s, F_SETFL, O_NONBLOCK);

        if (sockopts[0])
            set_socket_options(layer->s, sockopts);

	dbprintf("Latency setup done.\n");

	return 0;
}

int latency_start(struct timeval *start)
{
	int ret;
	dbprintf("Latency test start.\n");
	gettimeofday(start, NULL);
	ret = measure_latency(fd, bps, size, warmup, cooldown);
	dbprintf("Latency test complete.\n");
	return ret;
}

int latency_stop(struct timeval *stop)
{
	dbprintf("Latency test stop.\n");
	gettimeofday(stop, NULL);
	close(fd);
	return 0;
}

int latency_marshall(char **data, int *size, double running_time)
{
        uint64_t i;
	struct marshalled_result *tosend;

	dbprintf("Latency marshall arguments.\n");

	tosend = malloc(sizeof(struct marshalled_result) + sizeof(uint64_t) * samples);
	if (tosend == NULL)
	{
		dbprintf("Can't malloc %d bytes\n", sizeof(struct marshalled_result) + sizeof(uint64_t) * samples);
		return -1;
	}

	uint64_t *rtt_data = (uint64_t*)&tosend->data;

	memcpy(rtt_data, rtt_sample, samples * sizeof(uint64_t));

	for (i = 0 ; i < samples ; i++ )
		rtt_data[i] = htonll(tick_to_usec(rtt_data[i]));

	tosend->size    = htonll(result.size);
	tosend->time    = htonll(result.microseconds);
	tosend->samples = htonll(samples);
	tosend->sends   = htonll(result.sends);
	tosend->recvs   = htonll(result.recvs);
	tosend->throughput_requested = htonll(bps);
	tosend->throughput_achieved  = htonll(result.bps);
	tosend->throughput_sent = htonll(result.bps_sent);
	tosend->bad_packets = htonll(result.bad_packets);

	*data = (char *)tosend;
	*size = sizeof(struct marshalled_result) + (sizeof(uint64_t) * samples);

	return 0;
}

void latency_marshall_cleanup(char **data)
{
	free(*data);
}

/*
 * Run in ipbench
 */
int latency_unmarshall(char *input, int input_len, char **data,
		       int *data_len)
{
	dbprintf("Latency unmarshall arguments.\n");
	int i;

	/* convert theresult back to something sensible for us */
	struct marshalled_result *theresult = (struct marshalled_result *)(input);
	theresult->time    = ntohll(theresult->time);
	theresult->samples = ntohll(theresult->samples);
	theresult->size    = ntohll(theresult->size);
	theresult->sends   = ntohll(theresult->sends);
	theresult->recvs   = ntohll(theresult->recvs);
	theresult->throughput_requested = ntohll(theresult->throughput_requested);
	theresult->throughput_achieved  = ntohll(theresult->throughput_achieved);
	theresult->throughput_sent = ntohll(theresult->throughput_sent);
	theresult->bad_packets = ntohll(theresult->bad_packets);

	dbprintf("Unmarshalling %"PRId64" samples\n", theresult->samples);

	uint64_t *rtt_data = (uint64_t *)&theresult->data;

	for (i=0; i < theresult->samples; i++)
		rtt_data[i] = ntohll(rtt_data[i]);

	*data_len = sizeof(struct marshalled_result) + (sizeof(uint64_t) * theresult->samples);
	*data = malloc(*data_len);
	if (*data == NULL)
	{
		dbprintf("Out of buffer space.\n");
		return -1;
	}

	memcpy(*data, input, input_len);

	/* finally, setup the global variable "samples" */
	samples = theresult->samples;

	return 0;
}

void latency_unmarshall_cleanup(char **data)
{
	free(*data);
}

int latency_output(struct client_data data[], int nelem)
{

	int i = 0;
	uint64_t total_requested_throughput = 0;
	uint64_t total_achieved_throughput = 0;
	uint64_t total_sent_throughput = 0;
	uint64_t packet_size = 0;
	struct marshalled_result *theresult;
	uint64_t *rtt_totals = malloc(samples * sizeof(uint64_t) * nelem);

	if (rtt_totals == NULL)
	{
		dbprintf("Can't malloc enough room for samples.\n");
		return -1;
	}

	dbprintf("Latency begin output function\n");

	for (i = 0; i < nelem; i++)
	{
		theresult = (struct marshalled_result *)(data[i].data);

		memcpy(&rtt_totals[i*samples], &theresult->data, (sizeof(uint64_t)*samples));

		total_requested_throughput += theresult->throughput_requested;
		total_achieved_throughput  += theresult->throughput_achieved;
		total_sent_throughput += theresult->throughput_sent;
		packet_size = theresult->size;
#ifdef DEBUG
		/* when debug turned on show the individual results for each client */
		rtt_sample = (uint64_t*)&theresult->data;

		result.transmitted_bytes = theresult->recvs * theresult->size;
		result.microseconds = theresult->time;
		result.bps =
			(8 * theresult->recvs * theresult->size * US_PER_S) / result.microseconds;
		result.rtt_min = min_rtt(1);
		result.rtt_av = avg_rtt(1);
		result.rtt_max = max_rtt(1);
		result.rtt_med = med_rtt(1);
		result.rtt_std = dev_rtt(1);

		dbprintf("** Client %d\n **", i);
		dbprintf("transferred %"PRId64" bytes in %"PRId64" microseconds\n",
			 result.transmitted_bytes, result.microseconds);
		dbprintf("%"PRId64" sends %"PRId64" recvs\n", theresult->sends, theresult->recvs);
		dbprintf("achieved %"PRId64" bps\n", result.bps);
		dbprintf("min rtt was %"PRId64" us\n", result.rtt_min);
		dbprintf("average rtt was %"PRId64" us\n", result.rtt_av);
		dbprintf("max rtt was %"PRId64" us\n", result.rtt_max);
		dbprintf("rtt std dev was %"PRId64" us^2\n", result.rtt_std);
		dbprintf("median rtt was %"PRId64" us\n", result.rtt_med);
		dbprintf("\n");
#endif

		if (dump_dir != NULL)
		{
			FILE *f;
			uint64_t s;
			snprintf(dump_filename, MAXPATHLEN, "%s/%s_%d.dmp", dump_dir, dump_prefix, i);
			dbprintf("Dumping client data into %s\n", dump_filename);
			f = fopen(dump_filename, "w");
			if (fd == -1) {
				dbprintf("Can't create dump file (%s)\n", strerror(errno));
				continue;
			}
			for (s = 0 ; s < samples ; s++)
				fprintf(f, "%"PRId64"\n", rtt_totals[s]);
			fclose(f);
		}
	}

	/* all the below functions rely on rtt_sample */
	rtt_sample = rtt_totals;

	result.rtt_min = min_rtt(nelem);
	result.rtt_av = avg_rtt(nelem);
	result.rtt_max = max_rtt(nelem);
	result.rtt_med = med_rtt(nelem);
	result.rtt_std = dev_rtt(nelem);

#if 0 // pretty print
 	dbprintf("\n-TOTAL-\n");
 	dbprintf("transferred %"PRId64" bytes in %"PRId64" microseconds\n",
 		 result.transmitted_bytes, result.microseconds);
 		 dbprintf("achieved %"PRId64" bps\n", result.bps);
 	dbprintf("min rtt was %"PRId64" us\n", result.rtt_min);
 	dbprintf("average rtt was %"PRId64" us\n", result.rtt_av);
 	dbprintf("max rtt was %"PRId64" us\n", result.rtt_max);
 	dbprintf("rtt std dev was %"PRId64" us^2\n", result.rtt_std);
 	dbprintf("median rtt was %"PRId64" us\n", result.rtt_med);
	dbprintf("Got %"PRId64" bad packets\n", result.bad_packets);
 	dbprintf("\n");
#endif
	printf("%"PRId64",%"PRId64",%"PRId64",%"PRId64",%"PRId64",%"PRId64",%"PRId64",%.2f,%"PRId64",%"PRId64,
	       total_requested_throughput, total_achieved_throughput, total_sent_throughput, packet_size,
	       result.rtt_min, result.rtt_av, result.rtt_max, sqrt(result.rtt_std), result.rtt_med,
	       result.bad_packets);
	/* when running with cpu_target test, we want the cpu usage argument as the last one */
	if (with_cpu_target)
		printf(",");
	else
		printf("\n");

	free(rtt_totals);

	return 0;
}

int latency_setup_controller(char *arg)
{
	if (arg) {
		dbprintf("Setting up controller (%s)\n", arg);
		/* a bit of a hack ... */
		if (parse_arg(arg))
			return -1;
	}
	return 0;
}

struct ipbench_plugin ipbench_plugin = 
{
	.magic = "IPBENCH_PLUGIN",
	.name = "latency",
	.id = 0x2,
	.descr = "Latency Tests",
	.default_port = 7,
	.setup = &latency_setup,
	.setup_controller = &latency_setup_controller,
	.start = &latency_start,
	.stop = &latency_stop,
	.marshall = &latency_marshall,
	.marshall_cleanup = &latency_marshall_cleanup,
	.unmarshall = &latency_unmarshall,
	.unmarshall_cleanup = &latency_unmarshall_cleanup,
	.output = &latency_output,
};

