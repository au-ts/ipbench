// define this as a test client plugin
#define IPBENCH_TEST_CLIENT 1

#include "plugin.h"

/* function definitions for dummy test */
int nfs_latency_setup(char *hostname, int port, char* arg);
int nfs_latency_setup_controller(char* arg);
int nfs_latency_start(struct timeval *start);
int nfs_latency_stop(struct timeval *stop);
int nfs_latency_marshall(char **data, int *size, double running_time);
void nfs_latency_marshall_cleanup(char **data);
int nfs_latency_unmarshall(char *input, int input_len, char **data, int *data_len);
void nfs_latency_unmarshall_cleanup(char **data);
int nfs_latency_output(struct client_data data[], int nelem);
