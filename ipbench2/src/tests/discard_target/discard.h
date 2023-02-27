struct timeval;
int discard_setup(char *hostname, int port, char *arg);
int discard_start(struct timeval *sp);
int discard_stop(struct timeval *sp);
int discard_marshal(void **data, size_t *size, double running_time);
void discard_marshal_cleanup(void **data);
int discard_unmarshal(void *input, size_t input_len, void **data,
		       size_t *data_len);
void discard_unmarshal_cleanup(void **data);
int discard_output(struct client_data *target_data, struct client_data data[], int nelem);


