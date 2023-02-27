#ifndef _BSWAP_H
#define _BSWAP_H
/*
 * Byte order helper routines
 */

#if __BYTE_ORDER == __LITTLE_ENDIAN
# ifndef __bswap_64
#  define htonll(x) (((uint64_t)(ntohl((uint32_t)((x << 32) >> 32))) << 32) | \
		   (uint64_t)ntohl(((uint32_t)(x >> 32))))
#  define ntohll(x) htonll(x)
# else
#  define htonll(x) __bswap_64(x)
#  define ntohll(x) __bswap_64(x)
# endif /*__bswap_64*/
#else
# define htonll(x) (x)
# define ntohll(x) (x)
#endif /*__BYTE_ORDER == __LITTLE_ENDIAN */

#endif /*_BSWAP_H*/
