/*
 * ipbench
 * A benchmark for testing IP implementations
 * See COPYING for license terms
 * (C) 2003 DISY/Gelato@UNSW
 */

/*
 * General include file
 */

#ifndef _IPBENCH_H
#define _IPBENCH_H

#include "config.h"
#ifdef HAVE_GETOPT_H
    #include <getopt.h>
#else
    #include "getopt.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <signal.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>

#endif /* _IPBENCH_H */
