/* Wrappers for ipbench API
 *  Ian Wienand <ianw@gelato.unsw.edu.au>
 *  (C) 2004 
 *  Released under the GPL
 *
 * This file, and the related ipbench.i swing input file, describe the
 * shunt library that interfaces between test plugins (shared objects)
 * and the python based daemons.
 *
 */

#include <dlfcn.h>
#include <dirent.h>
#include <signal.h>

/* from python */
#include "osdefs.h"
#include "plugin.h"
#include "except.h"

struct ipbench_plugin *ipbench_plugin;
#ifdef IPBENCH_TEST_CLIENT
static struct client_data *client_data;
#elif defined(IPBENCH_TEST_TARGET)
static struct client_data target_data;
#else
# error "IPBENCH_TEST_CLIENT or IPBENCH_TEST_TARGET must be #defined"
#endif

#ifdef IPBENCH_TEST_CLIENT
/* number of clients */
static int nclients;
#endif
/* number of seconds between start() and stop() timestamping */
static double run_secs;
static struct timeval timer_start;

int enable_debug(void)
{
	do_debug = 1;
	return 0;
}

int get_default_port(void)
{
	return ipbench_plugin->default_port;
}

/* could probably be smarter, but we just find those things that end
 * in '.so' */
static int filter(const struct dirent *d)
{
	if ( strcmp(&d->d_name[strlen(d->d_name)-3] , ".so" ))
		return 0;
	return 1;
}

#ifdef IPBENCH_TEST_CLIENT
int setup_controller(int totalclients, char *args)
{
	/* allocate memory for the client_data array */
	nclients = totalclients;

	if ((client_data = malloc(nclients * sizeof(struct client_data))) == NULL)
		return ipbench_error(ipbench_RuntimeError, "nclients not set!");

	ipbench_plugin->setup_controller(args);

	return 0;
}
#elif defined IPBENCH_TEST_TARGET
int setup_controller(char *args)
{
	ipbench_plugin->setup_controller(args);
	return 0;
}
#endif

int load_plugin(const char *plugin_name)
{
	int n;
        struct dirent **namelist;
        char file[MAXPATHLEN];
        void *handle;
        struct ipbench_plugin *p;
	char *path = IPBENCH_PLUGIN_DIR;  /* defined on the command line */

        dbprintf("Finding plugins in %s\n", path);
        /* recurse through looking for plugins */
        n = scandir(path, &namelist, filter, NULL);
        if (n < 0)
                perror("scandir");
        else if ( n > MAX_PLUGINS )
		return -1;
        else {
                while(n--)
                {
                        snprintf(file, MAXPATHLEN, "%s/%s", path, namelist[n]->d_name);
                        dbprintf("Checking %s against %s ...\n", file, plugin_name);

                        handle = dlopen(file, RTLD_NOW);
                        if (!handle)
                        {
                                dbprintf("%s\n", dlerror());
                                goto out;
                        }

                        /* every shared object should have an ipbench_plugin header */
			dbprintf("Checking for header ...\n");
                        p = (struct ipbench_plugin *)dlsym(handle, "ipbench_plugin");
                        if (!p)
                        {
                                dbprintf("%s does not appear to be an ipbench plugin\n", file);
                                printf("%s\n", dlerror());
                                goto out;
                        }

			dbprintf("Checking magic number ...\n");
                        if (strcmp("IPBENCH_PLUGIN", p->magic)) {
                                dbprintf("Plugin does not contain IPBENCH header\n");
                                goto out;
                        }

			dbprintf("Checking test name |%s|%s|...\n", plugin_name, p->name);
			if ( (strlen(plugin_name) != strlen(p->name)) || 
			     (strcmp(plugin_name, p->name) != 0))
			{
				dbprintf("%s does not match %s\n", plugin_name, p->name);
				dlclose(handle);
				goto out;
			}

			dbprintf("Test loaded\n");
			ipbench_plugin = p;
			free(namelist[n]);
			break;
		out:
                        free(namelist[n]);
                }
                free(namelist);
        }

	if (ipbench_plugin == NULL)
		return ipbench_error(ipbench_RuntimeError, "Can not find plugin");
        return 0;
}

int start(void)
{
	int sts = 0;

	sts = ipbench_plugin->start(&timer_start);
	if (sts)
		return ipbench_error(ipbench_RuntimeError, "start failed");

	return 0;
}

int stop(void)
{
	int sts;
	struct timeval timer_stop, timer_diff;

	sts = ipbench_plugin->stop(&timer_stop);
	if (sts)
		return ipbench_error(ipbench_RuntimeError, "stop failed");

	timersub(&timer_stop, &timer_start, &timer_diff);
	run_secs = timer_diff.tv_sec + timer_diff.tv_usec * 1e-6;

	return 0;
}

#ifdef IPBENCH_TEST_CLIENT
int setup(char *hostname, int port, char *clientargs)
{
	int ret;

	dbprintf("[setup] : %s | %d | %s\n", hostname, port, clientargs);

	ret = ipbench_plugin->setup(hostname, port, clientargs);

	if (ret == -1)
		return ipbench_error(ipbench_RuntimeError, "Setup failed");
	else
		return 0;
}
#elif defined IPBENCH_TEST_TARGET

/* we need to catch the signal from C code, since python will not
 * deliver signals unless it is between opcodes, and by running
 * start() and not returning, we never give python a chance!
 */
void sigusr1_handler(int sig)
{
	dbprintf("--- GOT SIGUSR1 ---\n");
	stop();
}

int setup(char *clientargs)
{
	int ret;

	dbprintf("[setup] : %s\n", clientargs);

	signal(SIGUSR1, sigusr1_handler);

	ret = ipbench_plugin->setup(clientargs);

	if (ret == -1)
		return ipbench_error(ipbench_RuntimeError, "Setup failed");
        return 0;
}
#endif

/* This returns its data in marshalled_data, the size in marshall_data_size.
 * This is accessed from python as a vector (return value, data)
 */
int marshall(char **marshalled_data, int *marshalled_data_size)
{
	int sts;
	char *ret_data;
	sts = ipbench_plugin->marshall(&ret_data, marshalled_data_size, run_secs);
	dbprintf("[marshall] plugin marshall return code %d [len %d]\n", 
		 sts, *marshalled_data_size);
	
	*marshalled_data = ret_data;

	if (sts != 0) {
		dbprintf("[marshall] data flagged as invalid\n");
		return 1;
	}

	return 0;
}


#ifdef IPBENCH_TEST_CLIENT
/* for client test, we are passed the clientid of the data */
int unmarshall(int clientid, char *data, int len, int valid)
{
	int sts;
	char *ret_data;
	dbprintf("[unmarshall] data for client %d \"%s\" [len %d] [%s]\n", 
		 clientid, data, len, valid ? "invalid" : "valid");
	
	sts = ipbench_plugin->unmarshall(data, len, &ret_data,
					 &client_data[clientid].size);
	client_data[clientid].data = ret_data;
	client_data[clientid].valid = valid;
	client_data[clientid].type = IPBENCH_CLIENT;
	return sts;
}
#elif defined IPBENCH_TEST_TARGET
/* for target test, we just put it straight into target_data */
int unmarshall(char *data, int len, int valid)
{
	int sts;
	char *ret_data;
	dbprintf("[unmarshall] data for target \"%s\" [len %d] [%s]\n", 
		 data, len, valid ? "invalid" : "valid");
	sts = ipbench_plugin->unmarshall(data, len, &ret_data, &target_data.size);
	target_data.data = ret_data;
	target_data.valid = valid;
	target_data.type = IPBENCH_TARGET;
	return sts;
}
#endif

int output(void)
{
	int sts;
#ifdef IPBENCH_TEST_CLIENT
	sts = ipbench_plugin->output(client_data, nclients);
#elif defined IPBENCH_TEST_TARGET
	sts = ipbench_plugin->output(&target_data);
#endif
	return sts;

}
