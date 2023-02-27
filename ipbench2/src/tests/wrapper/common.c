#define IPBENCH_TEST_CLIENT 1

#include "plugin.h"

/* globals */
char start_filename[PATH_MAX];
char stop_filename[PATH_MAX];
char output_filename[PATH_MAX];

static int check_file_exists(char *filename)
{
	char progname[PATH_MAX];
	int r,i;
	struct stat s;

	/* we may have arguments */
	strncpy(progname, filename, PATH_MAX - 1);
        progname[PATH_MAX - 1] = 0;

	for ( i=0; i<strlen(progname); i++ )
		if ( progname[i] == ' ' ) 
			progname[i] = '\0';

	r = stat(progname, &s);
	if (r == -1) {
		dbprintf("Can not stat %s (%s)\n", progname, strerror(errno));
		return 1;
	}
	/* check permssions or something? */
	return 0;
}

/* Parse commands from an argument of the form
 * cmd1=val1,cmd2=val2...
 */
int parse_arg(char *arg)
{
	char *p, cmd[200], *val;

	if (arg == NULL)
		return 0;
	
	while (next_token(&arg, cmd, " \t,"))
	{
		if ((p = strchr(cmd, '='))) {
			*p = '\0';
			val = p+1;
		} else
			val = NULL;
		
		/* test arguments */
		dbprintf("Got cmd %s val %s\n", cmd, val);
		if (!strcmp(cmd, "start")) {
			if ( check_file_exists(val) )
			{
				dbprintf("start_script failed\n");
				return -1;
			}
			strncpy(start_filename, val, PATH_MAX - 1);
                        start_filename[PATH_MAX - 1] = 0;
		}
		else if (!strcmp(cmd, "stop")) {
			if ( check_file_exists(val) )
			{
				dbprintf("stop_script failed\n");
				return -1;
			}
			strncpy(stop_filename, val, PATH_MAX - 1);
                        stop_filename[PATH_MAX - 1] = 0;
		}
		else if (!strcmp(cmd, "output")) {
			if ( check_file_exists(val) )
			{
				dbprintf("output failed\n");
				return -1;
			}
			strncpy(output_filename, val, PATH_MAX - 1);
                        output_filename[PATH_MAX - 1] = 0;
		}
		else {
			dbprintf("Invalid argument %s=%s.\n", cmd, val);
			return -1;
		}

	}
	return 0;
}
	
int			     
common_marshall (char **data, int *size, double running_time)
{
#define CHUNK_SIZE (100 * 1024)
	char *return_data = NULL, *new_return_data, chunk[CHUNK_SIZE];
	int p[2], len, rlen=0;
	pid_t pid;
	
	if (pipe(p)) {
		dbprintf("pipe: %s\n", strerror(errno));
		return -1;
	}
		

	pid = fork();
	
	if (pid == 0)
	{
		dbprintf("[target_wrap_marshall] in output child\n");
		/* close reading fd */
		close(p[0]);
		/* capture stdout */
		dup2(p[1],1);
		if (system(output_filename) == -1) {
			dbprintf("[target_wrap_marshall] %s failed\n", output_filename);
			exit(1);
		}
		close(p[1]);
		exit(0);
	}
	
	/* parent -- read from the pipe into return_data. */
	close(p[1]);
	while ( (len = read(p[0], chunk, CHUNK_SIZE)) )
	{
		dbprintf("[target_wrap_marshall] got %d bytes\n", len);
		if (return_data == NULL)
		{
			return_data = (char*)malloc(len);
			memcpy(return_data, chunk, len);
		}
		else
		{
			/* copy new data */
			new_return_data = (char*)malloc( rlen + len );
			memcpy(new_return_data, return_data, rlen);
			free(return_data);
			return_data = new_return_data;
			memcpy(&return_data[rlen], chunk, len);
		}
		rlen += len;
	}
	close(p[0]);

	*data = return_data;
	*size = rlen;
	return 0;
}
