#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <error.h>
#include <errno.h>

#include <inttypes.h>
#include <sys/ioctl.h>


#include <sys/socket.h>
#include <features.h>    /* for the glibc version number */
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#include <net/if.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif

#define ETH_P_IPBENCH 0x6008

#include <error.h>

int getifindexfromdevice(char *interface)
{
    int s;
    struct sockaddr_ll sa;
    struct ifreq ifr;
    
    memset(&sa, 0, sizeof(sa));
    memset(&ifr, 0, sizeof(ifr));
    
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if ( s == -1 )
    {
	    printf("Interface %s invalid?\n", interface);
	    exit(1);
    }

    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    assert(ioctl(s, SIOCGIFINDEX, &ifr) >= 0);

    close(s);

    printf("Found ifr_ifindex %d\n", ifr.ifr_ifindex);

    return(ifr.ifr_ifindex);
    
}


void memswap(void *pa, void *pb, size_t len)
{
	unsigned char *a, *b;
	for(a = pa, b = pb; len > 0; a++, b++, len--)
	{
		unsigned char t = *a;
		*a = *b;
		*b = t;
	}
}

int
main(int argc, char *argv[]){
	int s;
	int snd, rcv;
	struct sockaddr_ll my_addr;
	socklen_t addrlen;
	char *buffer;
	
	/* parse the options */
	if(argc!=2){
		printf("usage: echo interface\n");
		return -1;
	}

	/* allocate the buffer */
	buffer = malloc(ETH_FRAME_LEN);
	assert(buffer!=NULL);
	bzero(buffer, ETH_FRAME_LEN);
#ifdef DEBUG
	int i;
	unsigned char *data = buffer + 14;
#endif

	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IPBENCH));
	if (s == -1)
	{
		printf("Socket creation failed : %s\n", strerror(errno));
		printf("(Hint -- maybe you are not root or have capabilities to make raw sockets?)\n");
		exit(1);
	}

	bzero(&my_addr, sizeof(struct sockaddr_ll));
	
	my_addr.sll_family = AF_PACKET;
	my_addr.sll_protocol = htons(ETH_P_IPBENCH);
	my_addr.sll_ifindex = getifindexfromdevice(argv[1]);
	my_addr.sll_pkttype = PACKET_HOST;
	my_addr.sll_halen = ETH_ALEN;
	
	if (bind (s, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll)) == -1 )
		perror("bind");

	for(;;){
		addrlen = sizeof(struct sockaddr);
		rcv = recvfrom(s, buffer, ETH_FRAME_LEN, 0, NULL, NULL);
		assert(rcv >= 0);
		
		struct ethhdr *eh = (struct ethhdr *)buffer;
		
		while(rcv > 0){ 
#ifdef DEBUG		       
			printf("<-- %d | ", rcv);
			printf("DST:");
			for (i = 0 ; i < ETH_ALEN ; i++)
				printf("%02x:", eh->h_dest[i]);
			printf(" SRC:");
			for (i = 0; i < ETH_ALEN ; i++)
				printf("%02x:", eh->h_source[i]);	
			printf("[%04x]", eh->h_proto);
			printf(" DATA:");
			for ( i = 0 ; i < 8 ; i++ )
				printf("%02x", (uint8_t)data[i]);
			printf("\n");
#endif			
			/* now flip the mac addresses around and send it back */
			memswap(eh->h_dest, eh->h_source, ETH_ALEN);
			/* send the packet back to the right place */
			memcpy(my_addr.sll_addr, eh->h_dest, ETH_ALEN);
			
			snd = sendto(s, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&my_addr, sizeof(my_addr) );
			assert(snd >= 0); 
			rcv -= snd; 

#ifdef DEBUG
			printf("--> %d | ", snd);
			printf("DST:");
			for (i = 0 ; i < ETH_ALEN ; i++)
				printf("%02x:", eh->h_dest[i]);
			printf(" SRC:");
			for (i = 0; i < ETH_ALEN ; i++)
				printf("%02x:", eh->h_source[i]);	
			printf("[%04x] ", eh->h_proto);
			for ( i = 0 ; i < 8 ; i++ )
				printf("%02x", (uint8_t)data[i]);
			printf("\n");
#endif
		}
	}
}
