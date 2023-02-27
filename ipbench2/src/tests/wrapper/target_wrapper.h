// define this as a test target plugin
#define IPBENCH_TEST_TARGET 1

#include "plugin.h"
#include "common.h"

extern char start_filename[];
extern char stop_filename[];
extern char output_filename[];

int target_wrap_setup(char* arg);
int target_wrap_setup_controller(char *arg);
int target_wrap_start(struct timeval *start);
int target_wrap_stop(struct timeval *stop);
int target_wrap_marshall(char **data, int *size, double running_time);
void target_wrap_marshall_cleanup(char **data);
int target_wrap_unmarshall(char *input, int input_len, char **data, int *data_len);
void target_wrap_unmarshall_cleanup(char **data);
int target_wrap_output(struct client_data *data);
