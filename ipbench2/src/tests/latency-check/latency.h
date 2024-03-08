/* latency header */
#define IPBENCH_TEST_CLIENT
#include "plugin.h"

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#ifdef CONFIG_RAW_LINUX
/* from man packet */
# include <features.h>    /* for the glibc version number */
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>     /* the L2 protocols */
#  include <net/if.h>
# else
#  include <asm/types.h>
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>   /* The L2 protocols */
# endif
#endif

/* According to RFC1700, this is unassigned, so it should be as good as any */
#define ETH_P_IPBENCH 0x6008
extern short protocol;

#ifndef ETH_FRAME_LEN
# define ETH_FRAME_LEN 1500
#endif
#ifndef IFNAMSIZ
# define IFNAMSIZ 100
#endif

/* usually 500000 */
#define SAMPLES 200000ULL

/* each packet is held in one of these */
struct packet_list_node {
	uint64_t packet_id;
	uint64_t send_time;
};

/* this struct holds all our important results */
struct latency_result {
	uint64_t transmitted_bytes;
	uint64_t microseconds;
	uint64_t bps;
	uint64_t bps_sent;
	uint64_t rtt_av;
	uint64_t rtt_min;
	uint64_t rtt_max;
	uint64_t rtt_std;
	uint64_t rtt_med;
	uint64_t sends;
	uint64_t recvs;
	uint64_t size;
	uint64_t bad_packets;
};

/* We marshall the result up into this structure */
struct marshalled_result {
	uint64_t time;
	uint64_t samples;
	uint64_t size;
	uint64_t sends;
	uint64_t recvs;
	uint64_t bad_packets;
	uint64_t throughput_requested;
	uint64_t throughput_achieved;
	uint64_t throughput_sent;
	uint64_t data[];
};


#define SOCKTYPE_TCP 0                  /* socktype setting flags                      */
#define SOCKTYPE_UDP 1
#define SOCKTYPE_RAW 2

/* so we can have multiple ways sending packets, we simply setup a
 * structure that gives us some functions to send and receive packets
 */

struct layer_opts
{
	/* the socket */
	int s;
	/* a function to setup the socket */
	int (*setup_socket)(char *hostname, int port, char *args);
	/* setup the packet */
	int (*setup_packet)(char** packet, int size);
	/* fill the packet with the second argument */
	int (*fill_packet)(void *packet, uint64_t seq);
	/* read the value in the packet into the second arg */
	int (*read_packet)(void *packet, uint64_t*seq);
	/* send the message (msg, size, flags) */
	int (*send_packet)(void *buf, int size, int flags);
	/* recv the packet (msg, size, flags) */
	int (*recv_packet)(void *buf, int size, int flags);
};

int raw_setup_socket(char *hostname, int port, char *args);
int raw_setup_packet(char ** packet, int size);
int raw_fill_packet(void *packet, uint64_t seq);
int raw_read_packet(void *packet, uint64_t*seq);
int raw_send_packet(void *buf, int size, int flags);
int raw_recv_packet(void *buf, int size, int flags);

/* note tcp & udp are really the same apart from the setup */
int tcp_setup_socket(char *hostname, int port, char *args);
int udp_setup_socket(char *hostname, int port, char *args);
int ip_setup_packet(char** packet, int size);
int ip_fill_packet(void *packet, uint64_t seq);
int ip_read_packet(void *packet, uint64_t*seq);
int ip_send_packet(void *buf, int size, int flags);
int ip_recv_packet(void *buf, int size, int flags);

/* these are the functions we implement */
int latency_setup(char *hostname, int port, char* arg);
int latency_setup_controller(char* arg);
int latency_start(struct timeval *start);
int latency_stop(struct timeval *stop);
int latency_marshall(char **data, int *size, double running_time);
void latency_marshall_cleanup(char **data);
int latency_unmarshall(char *input, int input_len, char **data, int *data_len);
void latency_unmarshall_cleanup(char **data);
int latency_output(struct client_data data[], int nelem);
