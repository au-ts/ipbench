#include "dbench.h"

/* the tbench test */

int
tbench_setup (char *hostname, int port, char *arg)
{
  return _tbench_setup(hostname, port, arg);
}

int
tbench_start (struct timeval *start)
{
  gettimeofday(start, NULL);
  return _tbench_start(start);
}

int
tbench_stop (struct timeval *stop)
{
  gettimeofday (stop, NULL);
  dbprintf("Tbench stop\n");
  return 0;
}

int
tbench_marshall (void **data, size_t * size, double running_time)
{
  struct result_struct r;
  _tbench_get_throughput(&r) == 0;
  *data = malloc(sizeof(struct result_struct));
  if ( *data == NULL )
      errprintf(1, "Can't malloc (%s)\n", strerror(errno));
  memcpy(*data, &r, sizeof(struct result_struct));
  *size = sizeof(struct result_struct);
  return 0;
}

void
tbench_marshall_cleanup(void **data)
{
  free(*data);
}

int
tbench_unmarshall (void *input, size_t input_len, void **data,
		  size_t * data_len)
{
  *data = malloc(sizeof(struct result_struct));
  if (*data == NULL)
    errprintf(1,"Can't malloc (%s)\n", strerror(errno));
  memcpy(*data, input, sizeof(struct result_struct));
  *data_len = sizeof(struct result_struct);
  return 0;
}

void
tbench_unmarshall_cleanup(void **data)
{
  free(*data);
}


int
tbench_output (struct client_data *target_data, struct client_data data[], int nelem)
{
  int i=0;
  uint64_t total_throughput = 0;
  struct result_struct *r;

  for(i = 0 ; i < nelem ; i++)
    {
      r = (struct result_struct*)(data[i].data);
      printf("Client %d ran %"PRId64" usec transferring %"PRId64" bytes\n -->Throughput is %"PRId64"\n", 
	     i, r->time, r->total_bytes, (r->total_bytes/BITS_PER_MB) / (r->time/US_PER_S));
      total_throughput += (r->total_bytes/BITS_PER_MB) / (r->time/US_PER_S);
    }

  printf("\n\n*** Total throughput %"PRId64"\n", total_throughput);

  return 0;
}


/* plugin header */
struct ipbench_plugin ipbench_plugin = 
{
	.magic = "IPBENCH_PLUGIN",
	.name = "tbench",
	.id = 0x3,
	.descr = "tbench test",
	.default_port = 7,
	.setup = &tbench_setup,
	.start = &tbench_start,
	.stop = &tbench_stop,
	.marshall = &tbench_marshall,
	.marshall_cleanup = &tbench_marshall_cleanup,
	.unmarshall = &tbench_unmarshall,
	.unmarshall_cleanup = &tbench_unmarshall_cleanup,
	.output = &tbench_output,
	//XXX
	.target_code = NULL,
};
