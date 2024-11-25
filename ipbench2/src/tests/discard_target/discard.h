struct timeval;
int discard_setup(char *hostname, int port, char *arg);
int discard_start(struct timeval *sp);
int discard_stop(struct timeval *sp);
int discard_marshal(char **data, int *size, double running_time);
void discard_marshal_cleanup(char **data);
int discard_unmarshal(char *input, int input_len, char **data,
		       int *data_len);
void discard_unmarshal_cleanup(char **data);
int discard_output(struct client_data *target_data, int);


