/*
   Utility functions for IPBench
   Parts written by Ian Wienand <ianw@gelato.unsw.edu.au>

   Other parts : 
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


/* 
 * This is built into a shared library that all tests should probably
 * link against
 */

#include "ipbench.h"
#include "util.h"

/* set via the enable_debug() fn in the python library */
int do_debug;

void
dbprintf(const char *msg, ...)
{
	va_list va;
        time_t thetime;
        struct tm *t;

	if (do_debug) {
		va_start(va, msg);
		thetime = time(NULL);
		t = localtime(&thetime);
		fprintf (stderr, "[%.2d:%.2d.%.2d] ", t->tm_hour, t->tm_min, t->tm_sec);
		fprintf(stderr, "dbg: ");
		vfprintf(stderr, msg, va);
		va_end(va);
	}
	fflush(stderr);
}


/*
Based on a routine by GJC@VILLAGE.COM.
Extensively modified by Andrew.Tridgell@anu.edu.au
*/
int next_token(char **ptr,char *buff,char *sep)
{
	static char *last_ptr=NULL;
	char *s;
	BOOL quoted;

	if (!ptr) ptr = &last_ptr;
	if (!ptr) return(False);

	s = *ptr;

	/* default to simple separators */
	if (!sep) sep = " \t\n\r";

	/* find the first non sep char */
	while(*s && strchr(sep,*s)) s++;

	/* nothing left? */
	if (! *s) return(False);

	/* copy over the token */
	for (quoted = False; *s && (quoted || !strchr(sep,*s)); s++) {
		if (*s == '\"')
			quoted = !quoted;
		else
			*buff++ = *s;
	}

	*ptr = (*s) ? s+1 : s;
	*buff = 0;
	last_ptr = *ptr;

	return(True);
}


/* fns to set socket options */

enum SOCK_OPT_TYPES {OPT_BOOL,OPT_INT,OPT_ON};

struct
{
	char *name;
	int level;
	int option;
	int value;
	int opttype;
} socket_options[] = {
  {"SO_KEEPALIVE",      SOL_SOCKET,    SO_KEEPALIVE,    0,                 OPT_BOOL},
  {"SO_REUSEADDR",      SOL_SOCKET,    SO_REUSEADDR,    0,                 OPT_BOOL},
  {"SO_BROADCAST",      SOL_SOCKET,    SO_BROADCAST,    0,                 OPT_BOOL},
#ifdef TCP_NODELAY
  {"TCP_NODELAY",       IPPROTO_TCP,   TCP_NODELAY,     0,                 OPT_BOOL},
#endif
#ifdef IPTOS_LOWDELAY
  {"IPTOS_LOWDELAY",    IPPROTO_IP,    IP_TOS,          IPTOS_LOWDELAY,    OPT_ON},
#endif
#ifdef IPTOS_THROUGHPUT
  {"IPTOS_THROUGHPUT",  IPPROTO_IP,    IP_TOS,          IPTOS_THROUGHPUT,  OPT_ON},
#endif
#ifdef SO_SNDBUF
  {"SO_SNDBUF",         SOL_SOCKET,    SO_SNDBUF,       0,                 OPT_INT},
#endif
#ifdef SO_RCVBUF
  {"SO_RCVBUF",         SOL_SOCKET,    SO_RCVBUF,       0,                 OPT_INT},
#endif
#ifdef SO_SNDLOWAT
  {"SO_SNDLOWAT",       SOL_SOCKET,    SO_SNDLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_RCVLOWAT
  {"SO_RCVLOWAT",       SOL_SOCKET,    SO_RCVLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_SNDTIMEO
  {"SO_SNDTIMEO",       SOL_SOCKET,    SO_SNDTIMEO,     0,                 OPT_INT},
#endif
#ifdef SO_RCVTIMEO
  {"SO_RCVTIMEO",       SOL_SOCKET,    SO_RCVTIMEO,     0,                 OPT_INT},
#endif
  {NULL,0,0,0,0}};



/****************************************************************************
set user socket options
****************************************************************************/
int set_socket_options(int fd, char *options)
{
	char tok[200];

	while (next_token(&options, tok, ";")) {
		int ret=0,i;
		int value = 1;
		char *p;
		BOOL got_value = False;

		if ((p = strchr(tok,'='))) {
			*p = 0;
			value = atoi(p+1);
			got_value = True;
		}

		for (i = 0; socket_options[i].name; i++)
			if (strcasecmp(socket_options[i].name, tok)==0)
				break;

		if (!socket_options[i].name) {
			dbprintf("Unknown socket option %s\n",tok);
			continue;
		}

		if (got_value)
			dbprintf("Setting socket option %s [%d]\n",tok,value);
		else
			dbprintf("Setting socket option %s\n", tok);

		switch (socket_options[i].opttype)
		{
		case OPT_BOOL:
		case OPT_INT:
			ret = setsockopt(fd, socket_options[i].level,
					 socket_options[i].option, (char *)&value, sizeof(int));
			break;

		case OPT_ON:
			if (got_value)
				dbprintf("syntax error - %s does not take a value\n", tok);
			else
			{
				int on = socket_options[i].value;
				ret = setsockopt(fd,socket_options[i].level,
						 socket_options[i].option, (char *)&on, sizeof(int));
			}
			break;
		}
		if (ret != 0)
		{
			dbprintf("Failed to set socket option %s\n",tok);
			return ret;
		}
	}

	return 0;
}
