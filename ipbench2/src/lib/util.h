#ifndef _UTIL_H
#define _UTIL_H

/* Generic utility functions */

/*
   Modified from dbench version 2
   Copyright (C) Andrew Tridgell 1999

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#define BOOL int
#define True 1
#define False 0

int next_token(char **ptr,char *buff,char *sep);
int set_socket_options(int fd, char *options);
void dbprintf(const char *msg, ...);

/* microuptime stuff */
#include "microuptime.h"

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

#endif /*_UTIL_H*/
