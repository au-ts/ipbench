// define this as a test client plugin
#define IPBENCH_TEST_CLIENT 1

#include "plugin.h"
#include "common.h"

extern char start_filename[];
extern char stop_filename[];
extern char output_filename[];

/* function definitions for dummy test */
int client_wrap_setup(char *hostname, int port, char* arg);
int client_wrap_setup_controller(char *arg);
int client_wrap_start(struct timeval *start);
int client_wrap_stop(struct timeval *stop);
int client_wrap_marshall(char **data, int *size, double running_time);
void client_wrap_marshall_cleanup(char **data);
int client_wrap_unmarshall(char *input, int input_len, char **data, int *data_len);
void client_wrap_unmarshall_cleanup(char **data);
int client_wrap_output(struct client_data data[], int nelem);
