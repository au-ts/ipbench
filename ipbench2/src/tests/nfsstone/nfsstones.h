#ifndef _NFSSTONES_H
#define _NFSSTONES_H

// define this as a test client plugin
#define IPBENCH_TEST_CLIENT 1
#define STR_LEN		256

#include "plugin.h"

/* threaded version */
static const char version[]="2.0";

/* function definitions for nfsstone test */
int  nfsstone_setup(char *hostname, int port, char* arg);
int  nfsstone_setup_controller(char *arg);
int  nfsstone_start(struct timeval *start);
int  nfsstone_stop(struct timeval *stop);
int  nfsstone_marshall(char **data, int *size, double running_time);
void nfsstone_marshall_cleanup(char **data);
int  nfsstone_unmarshall(char *input, int input_len, char **data, int *data_len);
void nfsstone_unmarshall_cleanup(char **data);
int  nfsstone_output(struct client_data data[], int nelem);
int nfsstone_run(struct timeval *start, struct timeval *end_time);

struct nfsstone_results{
	char host[STR_LEN];
	uint64_t total_stones;
	uint64_t usec;
};

/* Message used for benchmark status */
#define OK    0
#define FAIL  1
#define ERROR 1

#endif /* _NFSSTONES_H */
