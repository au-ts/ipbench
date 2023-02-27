// define this as a test client plugin
#define IPBENCH_TEST_CLIENT 1

#include "plugin.h"

/* function definitions for dummy test */
int dummy_setup(char *hostname, int port, char* arg);
int dummy_setup_controller(char* arg);
int dummy_start(struct timeval *start);
int dummy_stop(struct timeval *stop);
int dummy_marshall(char **data, int *size, double running_time);
void dummy_marshall_cleanup(char **data);
int dummy_unmarshall(char *input, int input_len, char **data, int *data_len);
void dummy_unmarshall_cleanup(char **data);
int dummy_output(struct client_data data[], int nelem);
