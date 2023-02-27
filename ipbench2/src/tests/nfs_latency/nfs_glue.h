int init_and_open(char *hostname, char *mountpoint, char *filename);
int generate_request();
int process_reply(uint64_t *timestamp);
void nfs_lat_cleanup(void);
