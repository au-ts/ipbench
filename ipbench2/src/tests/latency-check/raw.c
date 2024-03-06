#include "latency.h"

#ifdef CONFIG_RAW_LINUX

static uint8_t dest_mac[6];
static uint8_t src_mac[6];
static struct sockaddr_ll lladdr;

/* the socket */
static int s;

static int getifindexfromdevice(char *interface)
{
	int tmps;
	struct sockaddr_ll sa;
	struct ifreq ifr;

	memset(&sa, 0, sizeof(sa));
	memset(&ifr, 0, sizeof(ifr));

	if ((tmps = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		dbprintf("Socket creation error.\n");
		return -1;
	}

	strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = 0;
	if (ioctl(tmps, SIOCGIFINDEX, &ifr) < 0) 
	{
		dbprintf("Error getting interface number %s : %s.\n", 
					 interface, strerror(errno));
		return -1;
	}

	close(tmps);

	dbprintf("Found ifr_ifindex %d\n", ifr.ifr_ifindex);

	return(ifr.ifr_ifindex);
}


static int getmacaddrfromdevice(char *interface, uint8_t *mac_addr)
{
	int tmps;
	struct sockaddr_ll sa;
	struct ifreq ifr;

	memset(&sa, 0, sizeof(sa));
	memset(&ifr, 0, sizeof(ifr));

	if ((tmps = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		dbprintf("Socket creation error.\n");
		return -1;
	}

	strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (ioctl(tmps, SIOCGIFHWADDR, &ifr) < 0) {
		dbprintf("Error getting our hardware address (%s).\n", 
			 interface);
		return -1;
	}

	close(tmps);

	memcpy(mac_addr, ifr.ifr_addr.sa_data, 6);


#ifdef DEBUG
	dbprintf("using %s as the source interface\n", interface);
	for(s = 0 ; s< 6; s++) {
		dbprintf("src_mac[%d] = %x\n", s, 
			 (uint8_t)ifr.ifr_addr.sa_data[s]);
		return -1;
	}
#endif

	return 0;
}


int raw_setup_socket(char *hostname, int port, char *args)
{
	/* parse a MAC address since we are socktype RAW */
	char *next;
	int i = 0;
	unsigned int num;
	next = strtok(hostname, ":");
	while(next)
	{
		sscanf(next, "%x", &num);
		if (num < 0 || num > 0xff) {
			dbprintf("Invalid MAC address!\n");
			return -1;
		}
		dest_mac[i] = (uint8_t)num;
		dbprintf("dest_mac[%d] = 0x%x\n", i, dest_mac[i]);
		if (i > 6) {
			dbprintf("Too many octets in MAC address!\n");
			return -1;
		}
		i++;
		next = strtok(NULL, ":");
	}
	if (i != 6) {
		dbprintf("That doesn't look like a valid MAC address!\n");
		return -1;
	}

	/* setup the sockaddr_ll */
	bzero(&lladdr, sizeof(lladdr));

	lladdr.sll_family = AF_PACKET;
	lladdr.sll_protocol = htons(protocol);
	lladdr.sll_ifindex = getifindexfromdevice(args);
	lladdr.sll_pkttype = PACKET_OTHERHOST;
	lladdr.sll_halen = ETH_ALEN;
	memcpy(lladdr.sll_addr, dest_mac, 6);

	dbprintf("Socket type is RAW, using interface %s.\n", args);
	getmacaddrfromdevice(args, src_mac);

	s = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (s == -1) {
		dbprintf("Can not create socket (%s).\n", strerror(errno));
		return -1;
	}

	/* bind so we only receive the packets we want */
	if (bind(s, (struct sockaddr *)&lladdr, sizeof(struct sockaddr_ll)) == -1) {
		dbprintf("Can not bind to ethernet card : %s\n", strerror(errno));
		return -1;
	}

	return s;
}

/* Packet socket based sending functions */
int raw_setup_packet(char **buf, int size)
{
	/*
	 *   an ethernet packet looks like this ...
	 * | Dest MAC | Src  MAC | Packet Type | User Data   | FCS      |
	 * | 6 octets | 6 octets | 2 octets    | 46-1500 oct | 4 octets |
	 *
	 * src_mac and dest_mac have been setup for us previously
	 */

	struct ethhdr *hdr;

	if (size > ETH_FRAME_LEN)
		return 1;

	*buf = malloc(ETH_FRAME_LEN);
	if (buf == NULL) 
	{
		dbprintf("Can't allocate new packet (%s)\n", strerror(errno));
		return -1;
	}

	hdr = (struct ethhdr *)(*buf);

	memcpy(hdr->h_dest, dest_mac, ETH_ALEN);
	memcpy(hdr->h_source, src_mac, ETH_ALEN);
	hdr->h_proto = htons(protocol);

	return 0;
}

int raw_fill_packet(void *buf, uint64_t seq)
{
	uint8_t *data = (uint8_t*)buf + 14;
	*((uint64_t*)data) = seq;
	return 0;
}

int raw_read_packet(void *buf, uint64_t *seq)
{
	uint8_t *data = (uint8_t*)buf + 14;
	*seq = *((uint64_t*)(data));
	return 0;
}

int raw_recv_packet(void *buf, int size, int flags)
{
	return recvfrom(s, buf, size, 0, NULL, NULL);
}

int raw_send_packet(void *buf, int size, int flags)
{
	return sendto(s, buf, size, 0, (struct sockaddr*)(&lladdr), sizeof(lladdr));
}
#endif /*CONFIG_RAW_LINUX*/

#ifdef CONFIG_RAW_BPF
/* just dummies -- we don't support this */
int raw_setup_socket(char *hostname, int port, char *args) { return 0; }
int raw_setup_packet(char **buf, int size) { return 0; }
int raw_fill_packet(void *buf, uint64_t seq) { return 0; }
int raw_read_packet(void *buf, uint64_t *seq) { return 0; }
int raw_recv_packet(void *buf, int size, int flags) { return 0; }
int raw_send_packet(void *buf, int size, int flags) { return 0; }
#endif /*CONFIG_RAW_BPF*/
