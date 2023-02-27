// define this as a test target plugin
#define IPBENCH_TEST_TARGET 1

#include "plugin.h"

int dummy_target_setup(char* arg);
int dummy_target_setup_controller(char* arg);
int dummy_target_start(struct timeval *start);
int dummy_target_stop(struct timeval *stop);
int dummy_target_marshall(char **data, int *size, double running_time);
void dummy_target_marshall_cleanup(char **data);
int dummy_target_unmarshall(char *input, int input_len, char **data, int *data_len);
void dummy_target_unmarshall_cleanup(char **data);
int dummy_target_output(struct client_data *data);
