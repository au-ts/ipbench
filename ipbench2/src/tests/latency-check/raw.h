extern int raw_setup_socket(char *hostname, int port, char *args);
extern int raw_setup_packet(char ** packet, int size);
extern int raw_fill_packet(void *packet, uint64_t seq);
extern int raw_read_packet(void *packet, uint64_t*seq);
extern int raw_send_packet(void *buf, int size, int flags);
extern int raw_recv_packet(void *buf, int size, int flags);
