/* 
   Copyright (C) by Andrew Tridgell <tridge@samba.org> 1999, 2001
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   
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

/* TODO: We could try allowing for different flavours of synchronous
   operation: data sync and so on.  Linux apparently doesn't make any
   distinction, however, and for practical purposes it probably
   doesn't matter.  On NFSv4 it might be interesting, since the client
   can choose what kind it wants for each OPEN operation. */

#include "dbench.h"

int sync_open = 0, sync_dirs = 0;
char *tcp_options = TCP_OPTIONS;

static int num_children = 1;

static struct child_struct *children;

static void sigcont(int sig)
{
}

static void sig_alarm(int sig)
{
	uint64_t total = 0;
	int total_lines = 0;
	int running = 0;
	int i;
	int nprocs = children[0].nprocs;

	for (i=0;i<nprocs;i++) {
		total += children[i].bytes_in + children[i].bytes_out;
		total_lines += children[i].line;
		if (!children[i].done) running++;
	}
	/* yeah, I'm doing stdio in a signal handler. So sue me. */	
	vbprintf("%4d  %8d  %"PRId64" MB/sec    \r",
	       running,
	       total_lines / nprocs, (total / BITS_PER_MB) / (end_timer()/US_PER_S));
	signal(SIGALRM, sig_alarm);
	alarm(PRINT_FREQ);
}

/* this creates the specified number of child processes and runs fn()
   in all of them */
static int create_procs(int nprocs, void (*fn)(struct child_struct * ))
{
  int i;
	int synccount;

	signal(SIGCONT, sigcont);

	start_timer();

	synccount = 0;

	children = shm_setup(sizeof(struct child_struct)*nprocs);
	if (!children) {
		printf("Failed to setup shared memory\n");
		return 1;
	}

	memset(children, 0, sizeof(*children)*nprocs);

	for (i=0;i<nprocs;i++) {
		children[i].id = i;
		children[i].nprocs = nprocs;
	}
	
	/* the shared memory is setup and the children are forked off
	 * here.  they go to sleep (pause()) and are woken up by
	 * start_procs() when it it time to start the test
	 */
	for (i=0;i<nprocs;i++) {
		if (fork() == 0) {
			setbuffer(stdout, NULL, 0);
			nb_setup(&children[i]);
			children[i].status = getpid();
			pause();
			fn(&children[i]);
			_exit(0);
		}
	}


	/* wait up to 30 seconds for children to register */
	do {
		synccount = 0;
		for (i=0;i<nprocs;i++) {
			if (children[i].status) synccount++;
		}
		if (synccount == nprocs) break;
		sleep(1);
	} while ((end_timer()/US_PER_S) < 30);

	if (synccount != nprocs) {
		dbprintf("FAILED TO START %d CLIENTS (started %d)\n", nprocs, synccount);
		return -1;
	}

	return 0;
}

static int start_procs()
{
  int i, status;

	start_timer();
	kill(0, SIGCONT);

	signal(SIGALRM, sig_alarm);
	alarm(PRINT_FREQ);

	dbprintf("%d clients started\n", num_children);

	for (i=0;i<num_children;) {
		if (waitpid(0, &status, 0) == -1) continue;
		if (WEXITSTATUS(status) != 0) {
			dbprintf("Child failed with status %d\n",
			       WEXITSTATUS(status));
			exit(1);
		}
		i++;
	}

	alarm(0);
	sig_alarm(0);

	return 0;
}


/**** IAN CODE ****/

/* Parse commands from an argument of the form
 * cmd1=val1,cmd2=val2...
 */

static int num_children;
static uint64_t total_bytes, total_time;
extern char *client_filename;
extern char *server;

static int parse_arg(char *arg)
{
	char *p, cmd[200], *val;

	if (arg == NULL)
		return 0;

	while (next_token(&arg, cmd, " \t,"))
	  {
	    if ((p = strchr(cmd, '=')))
	      {
		*p = '\0';
		val = p+1;
	      }
	    else
	      val = NULL;
	    
	    /* test arguments */
	    dbprintf("Got cmd %s val %s\n", cmd, val);
	    if (!strcmp(cmd, "client_filename")) 
	      {
		client_filename = malloc(strlen(val)+1);
		strcpy(client_filename, val);
	      }
	    else if (!strcmp(cmd, "num_children"))
	      {
		num_children = strtoll(val, (char**)NULL, 10);
		if ( num_children < 0 )
		  {
		    dbprintf("num_children < 0!\n");
		    return -1;
		  }
	      }
	    else if (!strcmp(cmd, "sockopts"))
	      {
		tcp_options = malloc(strlen(val)+1);
		strcpy(tcp_options, val);
	      }
	    else {
	      vbprintf("Invalid argument %s=%s.\n", cmd, val);
	      return 1;	/* something is wrong */
	    }
	  }
	return 0;
}


int _tbench_setup(char *hostname, int port, char *args)
{

  dbprintf("Setting up tbench test (server %s, port %d, args %s)\n", 
	   hostname, port, args);

  server = malloc(strlen(hostname)+1);
  strcpy(server, hostname);
  server = hostname;

  parse_arg(args);

  if (create_procs(num_children, child_run))
  {
    dbprintf("Error setting up children\n");
    return -1;
  }

  return 0;

}

/* 
 * start the test by signalling to all our children to wake up.
 * the sigarlm will run every second to print us a status output 
 * and check what is still running
 */
int _tbench_start(struct timeval *start)
{
  int i;

  dbprintf("Starting tbench test\n");
  
  gettimeofday (start, NULL);
  
  start_procs();

  for (i=0;i<num_children;i++) {
    total_bytes += children[i].bytes_in + children[i].bytes_out;
  }
  
  total_time = end_timer();
  
  dbprintf("Total bytes : %"PRId64" | Total time %"PRId64"\n", total_bytes, total_time);
  dbprintf("Throughput %"PRId64" MB/sec (%d procs)\n", 
	 (total_bytes/BITS_PER_MB) / (total_time/US_PER_S), num_children);
  return 0;
}

int _tbench_get_throughput(struct result_struct *r)
{
	r->time = total_time;
        r->total_bytes = total_bytes;

	return 0;
}
