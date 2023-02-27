
#include "nfsstones.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/utsname.h>

/*
 * This program has been modified to extend the ipbench, benchmark
 * framework. The original code base has been kept and resturctured
 * into ipbench specific routines.
 * - http://parallel.ru/ftp/benchmarks/nfsstone/
 * - http://sourceforge.net/projects/ipbench/
 * main()->nfsstone_start()
 */

/*
 * This program takes two arguments.  The first one is the directory to
 * run the tests in.  The second one is the lock file name.  The program
 * will set itself up, then try to obtain the lock.  This will allow
 * the benchmark to sync up.  When it has the lock, it will release it
 * right away to allow someone else to get it.  This should happen fast
 * enough that the clients will start within a second or two of each other.
 *
 */

/*
 * Note: with the below settings, each client will use about 12.2 MB of
 * disk space on the server.  It is important to keep the disk space
 * per client above the amount of memory per client.  Since we are
 * trying to test the server speed, not the client cache speed, the
 * blocks per file should be adjusted to keep the cache overflowing.
 * Note: The above observation does not apply to suns not running
 * 	SunOS >= 4.0
 */
#define	TOP_DIRS	top_dirs /* see the declarations */
#define	BOT_DIRS	bot_dirs /* for defaults         */
#define	FILES_PER_DIR	1
#define FILE_CREATES	83
#define NREAD_LINK	583
#define	BLOCKS_PER_FILE	250
#define FILE_LOOKUPS	4167
#define BYTES_PER_BLOCK	8192
#define TOT_DIRS	(TOP_DIRS * BOT_DIRS)
#define TOT_FILES	(TOT_DIRS * FILES_PER_DIR)

/*
 * The number eleven comes from:
 * 1. write the file
 * 2. read sequentially
 * 3. read the file (not sequentially)
 * 4. read sequentially
 * 5. read sequentially
 * 6. read sequentially
 * 7. read the file (not sequentially)
 * 8. read sequentially
 * 9. read sequentially
 * 10. read sequentially
 * 11. read the file (not sequentially)
 */
#define TOT_FILEOPS	(TOT_FILES * 11 * BLOCKS_PER_FILE +             \
                         (FILE_LOOKUPS + FILE_CREATES + NREAD_LINK) * TOT_FILES)

/*
 * The number 2 comes from the create/delete of the directories
 */
#define TOTAL_OPS	(TOT_DIRS * 2 + TOT_FILES * 2 + TOT_FILEOPS)

#ifndef LINUX
char *strcpy(), *strcat(), *sprintf();
long lseek();
#endif

/* private routines */
/* global vars */
extern int errno;

extern struct ipbench_plugin ipbench_plugin;
struct nfsstone_results results;
char path[STR_LEN];
char lock[STR_LEN];
int lock_fd = 0;

/* need to be long since we use strtol */
long top_dirs = 1;
long bot_dirs = 3;

int do_child(char *dir);
int seq_write(int fd);
int seq_read(int fd);
int nseq_read(int fd);
/* end private routines */

double ustos(uint64_t usec)
{
	double tmp = usec/1000000.0;
	return tmp;
}

uint64_t stous(uint64_t sec)
{
	return sec*1000000;
}

void usage()
{
		fprintf(stderr, "Usage: %s <path_prefix> <lock_file_path> [top_dirs] [bot_dirs]\n",
			ipbench_plugin.name );
		fprintf(stderr, "\tpath_prefix:    path to network file system.\n");
		fprintf(stderr, "\tlock_file_path: path to a lockfile for each client to access, this\n");
		fprintf(stderr, "\t                does not have to be shared between clients.\n");
		fprintf(stderr, "\ttop_dirs:       the number of directories to create in the top level (default 1).\n");
		fprintf(stderr, "\tbot_dirs:       the number of directories to create under the top level (default 3).\n");
}

long double std_dev(struct client_data data[], uint64_t avg, int samples){
	uint64_t  tmp=0.0, std=0.0;
	int i;
	struct nfsstone_results *res;
	for(i=0;i<samples;i++){
		res=(struct nfsstone_results *)(data[i].data);
		tmp = avg - ntohll(res->usec);
		std += tmp * tmp;
	}
	std = std/samples;
	return sqrt(std);
}

int buf[BYTES_PER_BLOCK/sizeof(int)];

int nfsstone_setup_controller(char *arg)
{
	return 0;
}

int nfsstone_setup(char *hostname, int port, char* arg)
{
	char tmp[STR_LEN];
	struct stat stat_buf;
	bzero((char *)buf, BYTES_PER_BLOCK);
	char *tmp_top_dirs, *tmp_bot_dirs;
	char *err_buf; /* error buffer for strtol() */

	if(strncmp(version,"1.0", 256) == 0){
		fprintf(stderr, "/****************************************************/\n");
		fprintf(stderr, "/*====================WARNING=======================*/\n");
		fprintf(stderr, "/*                                                  */\n");
		fprintf(stderr, "/* This is an old version of ipbench/nfsstone using */\n");
		fprintf(stderr, "/* a non optimal sequential method. Your results    */\n");
		fprintf(stderr, "/* will be invalid wrt to the origianl nfsstone     */\n");
		fprintf(stderr, "/* benchmark that uses multiple processes to        */\n");
		fprintf(stderr, "/* generate the a work load.                        */\n");
		fprintf(stderr, "/*                                                  */\n");
		fprintf(stderr, "/* You will need to update to version 2 to report   */\n");
		fprintf(stderr, "/* accurate results.                                */\n");
		fprintf(stderr, "/****************************************************/\n");
	}
	if (arg == NULL) {
		usage();
		return FAIL;
	}
	strncpy(tmp, arg, STR_LEN - 1);
        tmp[STR_LEN - 1] = 0;

	if (hostname == NULL){
		fprintf(stderr, "nfsstone_setup called with hostname = NULL\n");
	}
	dbprintf("[%d][nfsstone_setup] called with: hostname[%s], port[%d], arg[%s]\n",
		__LINE__,hostname, port, tmp);

	strncpy(path,strtok(tmp, " \t\n"),STR_LEN - 1);
        path[STR_LEN - 1] = 0;
	strncpy(lock,strtok(NULL, " \t\n"),STR_LEN - 1);
        lock[STR_LEN - 1] = 0;

	if( (tmp_top_dirs=strtok(NULL," \t\n")) != NULL ){
		top_dirs=strtol(tmp_top_dirs,&err_buf,0);
		if(errno==EINVAL||errno==ERANGE){
			dbprintf("[%d][nfsstone_run]Error top_dir argument convertion, seen[%s]\n",*err_buf);
			perror("Top dir converstion error");
			top_dirs=1;	/* set defaults */
		}
	}

	if( (tmp_bot_dirs=strtok(NULL," \t\n")) != NULL ){
		bot_dirs=strtol(tmp_bot_dirs,&err_buf,0);
		if(errno==EINVAL||errno==ERANGE){
			dbprintf("[%d][nfsstone_run]Error bot_dir argument convertion, seen[%s]\n",*err_buf);
			perror("Bot dir converstion error");
			bot_dirs=3;	/* set defaults */
		}
	}

	dbprintf("[%d][nfsstone_setup]path[%s] lock [%s]\n\tTOP_DIRS[%ld], BOT_DIRS[%ld]\n",
		__LINE__,path, lock, top_dirs, bot_dirs);
	if(path[0] == '\0' || lock[0] == '\0'){
		fprintf(stderr, "Path or lock file is NULL.");
		fprintf(stderr, "Require arguments <path to nfs> <lockfile>\n");
		return FAIL;
	}
	if (stat(path, &stat_buf) != OK) {
		perror("path_prefix");
		return FAIL;
	}
	if ((stat_buf.st_mode & S_IFMT) != S_IFDIR ||
	    access(path, W_OK|X_OK) != OK) {
		(void)fprintf(stderr,
		    "Path_prefix '%s', is not a directory with write permission.\n",path);
		return FAIL;
	}

	if ((lock_fd = open(lock, O_RDWR|O_CREAT, 0600)) < OK) {
		perror("lock_file");
		return FAIL;
	}

	if (chdir(path) != OK) {
		dbprintf("[%d][nfsstone_setup]Cannot chdir to '%s'\n", __LINE__,path);
		perror("path_prefix");
		return FAIL;
	}

	/*
	 * Now we go get the lock to synchronise with the other clients
	 */

	if (lockf(lock_fd, F_LOCK, 0) != OK) {
		perror("lockf");
		return FAIL;
	}

	/*
	 * Now let someone else have the lock
	 */

	if (lockf(lock_fd, F_ULOCK, 0) != OK) {
		perror("lockf");
		return FAIL;
	}
	return OK;
}
int  nfsstone_stop(struct timeval *stop)
{
	struct stat stat_buf;
	int error=OK;

	gettimeofday(stop,NULL);

	if (stat(path, &stat_buf) != OK) {
		perror("path_prefix");
		error += FAIL;
	}
	if(close(lock_fd)!=OK){
		perror("Could not close lock_fd");
		error += FAIL;
	}
	else{
		 if( unlink(lock) != OK ){
		 	perror("Could not delete lock file");
			error += FAIL;
		 }
	}
	if ((stat_buf.st_mode & S_IFMT) == S_IFDIR) {
		fprintf(stderr,
		    "Clean up benchmark directory[%s].\n",path);
		if(rmdir(path) != OK){
			fprintf(stderr, "Could not remove benchmark tmp dir[%s]\n", path);
			error += FAIL;
		}
	}
	return error;
}
int  nfsstone_marshall(char **data, int *size, double running_time)
{
	*data = malloc(sizeof(results));

//	ASSERT(*data);

	memcpy(*data, &results, sizeof(results));
	*size = sizeof(results) + 1;

	return OK;
}
void nfsstone_marshall_cleanup(char **data)
{
	free(*data);
	dbprintf("[%d][nfsstone_marshall_cleanup]\n",__LINE__);
	return;
}
int  nfsstone_unmarshall(char *input, int input_len, char **data, int *data_len)
{
	char *res = malloc(input_len);
	dbprintf("[%d][nfsstone_unmarshall]\n",__LINE__);

	if (res == NULL){
		fprintf(stderr,"[%d][nfsstone_unmarshall]Received bad data oops.\n", __LINE__);
		return FAIL;
	}

	*data = res;
	memcpy(res, input, input_len);
	*data_len = input_len;

	return OK;
}
void nfsstone_unmarshall_cleanup(char **data)
{
	dbprintf("[%d][nfsstone_unmarshall_cleanup]\n",__LINE__);
	free(*data);
	return;
}

int  nfsstone_output(struct client_data data[], int nelem)
{
	dbprintf("[%d][nfsstone_output]\n",__LINE__);
	struct nfsstone_results *res;
	int i;
	uint64_t stones_equal[nelem];	/* make sure the work loads were equal */
	uint64_t avg_total_time=0;

	for (i=0; i < nelem; i++){
		res = (struct nfsstone_results *)(data[i].data);
		if (res != NULL) {
			int j;
			uint64_t c_stones, c_run_time;

			for (j=0; j<i; j++){
				/*
				 * here we check to see that the loads are equal
				 * I see no point in benchmarking unequal loads.
				 */
				if (stones_equal[j] != ntohll(res->total_stones)) {
					struct nfsstone_results *tmp=(struct nfsstone_results *)(data[j].data);
					printf("========= Invalid results =========\n");
					printf("%s Nfstones[%"PRIu64"] != %s Nfstones[%"PRIu64"] i[%d],j[%d]\n",
						res->host, res->total_stones, tmp->host, stones_equal[j], i,j);
				}
			}

			c_stones = ntohll(res->total_stones);
			c_run_time = ntohll(res->usec);
			stones_equal[i] = c_stones;
			avg_total_time += c_run_time;

			printf("Nfsstone results for: %s\n", res->host);
			printf("Total Nfsstones    Total Time Seconds    Nfsstone/Second\n");
			printf("%15"PRIu64"%22.5f%19.5f\n",
				c_stones, ustos(c_run_time), c_stones/ustos(c_run_time) );
		} else {
			printf("nfsstone client data results are NULL\n");
		}
	}
	avg_total_time = avg_total_time/(double)nelem;
	printf("\nTotal benchmark calculation\n");
	printf("   Average Time for %d clients\n", nelem);
	printf("%20.5f\n", ustos(avg_total_time));
	printf("   Std. Dev.\n");
	printf( "%20.5f\n", ustos(std_dev(data,avg_total_time,nelem)) );

	return OK;
}

int nfsstone_start(struct timeval *start)
{
	struct timeval end_time;
	int error = 0;

	error = nfsstone_run(start, &end_time);
	dbprintf("[%d][nfsstone_start] returning error[%d]\n", __LINE__, error);
	return error;
}

/*
 * This level of indirection is because we fork.
 * Ipbench only expects ONE return value not a whole bunch so just
 * return once from here after all children or errors are accounted for.
 */
int nfsstone_run(struct timeval *start, struct timeval *end_time)
{

	struct timezone dummy;
	char hostname[BUFSIZ - 12];

	int i, j, num = 0, status;
	pid_t  pid=0;
	char s[BUFSIZ], t[BUFSIZ], u[BUFSIZ];
	int error = OK;

	/* Do not include getting host name in
	 * timming calcs, this would be wrong!!!
	 */
	if( (gethostname(hostname, sizeof hostname)) < OK){
	 	dbprintf("[%d][nfsstone_run]'gethostname' sys.call failed.\n",
			__LINE__);
	 	perror("Could no get hostname");
		error += FAIL;
	 }

	 dbprintf("Using hostname[%s]\n", hostname);

	/*
	 * Start Timing
	 */
	if (gettimeofday(start, &dummy) == -1) {
		perror("gettimeofday");
		error += FAIL;
	}

	for (i = 0; i < TOP_DIRS; i++) {
		/* dont even think of starting if already in error */
		if(error)
			break;

		(void)sprintf(u, "%8d-%s", ++num, hostname);
		dbprintf("[%d][nfsstone_run]Makeing directory '%s'\n", __LINE__, u);
		if (mkdir(u, 0777) != OK) {
			dbprintf("[%d][nfsstone_run]Cannot make directory '%s'\n", __LINE__, u);
			perror("mkdir");
			return FAIL;
		}
		for (j = 0; j < BOT_DIRS; j++) {
			(void)sprintf(t, "%8d", ++num);
			(void)strcpy(s, u);
			(void)strcat(s, "/");
			(void)strcat(s, t);
			dbprintf("[%d][nfsstone_run]Makeing directory '%s'\n", __LINE__, s);
			if (mkdir(s, 0777) != OK) {
				perror("mkdir: subdir");
			}
			else{
				if( (pid = fork()) == OK ){
					if( (error = do_child(s)) != OK ){
						dbprintf("[%d][nfstone_run]do_child returned error\n", __LINE__);
						break;
					}
				}
				else if(pid == -1){
					perror("fork for do_child() failed");
					return FAIL;
				}
			}
		}
	}

	while( (pid=wait(&status)) != -1)
		if (WEXITSTATUS(status) != OK)
			error = 1;

	dbprintf("[%d][nfsstone_run]Removing benchmark dir tree\n", __LINE__);
	num = 0;
	for (i = 0; i < TOP_DIRS; i++) {
		(void)sprintf(u, "%8d-%s", ++num, hostname);
		for (j = 0; j < BOT_DIRS; j++) {
			(void)sprintf(t, "%8d", ++num);
			(void)strcpy(s, u);
			(void)strcat(s, "/");
			(void)strcat(s, t);
			dbprintf("[%d][nfsstone_run]Removing dir '%s'\n",__LINE__,s);
			if (rmdir(s) != OK)
				perror("rmdir: subdir");
		}
		if (rmdir(u) != OK)
			perror("rmdir");
	}
	fprintf(stderr, "********************* WARNING ********************\n");
	fprintf(stderr, "* Calling unlink() in nfsstone_run, this should  *\n");
	fprintf(stderr, "* be moved to nfsstone_stop, here for testing :( *\n");
	fprintf(stderr, "**************************************************\n");
	unlink(lock);

	if(error){
		dbprintf("[%d][nfsstone_run] in error returning code [%d]\n", error);
		return error;
	}

	if (gettimeofday(end_time, &dummy) == -1) {
		perror("gettimeofday");
		error += FAIL;
	}

	memset(&results,'\0',sizeof(results));
	memcpy(results.host, hostname, STR_LEN - 1);
        results.host[STR_LEN - 1] = 0;
	results.total_stones = htonll((long)TOTAL_OPS);
	results.usec = htonll((stous(end_time->tv_sec - start->tv_sec) +
                              (end_time->tv_usec - start->tv_usec)));
	dbprintf("usec[%"PRIu64"]\n", results.usec);
	return error;
}

int do_child(dir)
	char *dir;
{
	int i, fd, cfd;
	long j;
	char s[BUFSIZ];
	char b[BUFSIZ];

	dbprintf("[%d][nfsstone do_child]\n", __LINE__);

	if (chdir(dir) != OK) {
		dbprintf("Could not chdir(%s)\n", dir);
		perror("Subdirectory");
		exit(-FAIL);
	}
	for (i = 0; i < FILES_PER_DIR; i++) {
		(void)sprintf(s, "%8d", i);
		if ((fd = open(s, O_RDWR|O_CREAT|O_TRUNC, 0666)) < OK) {
			perror("open");
			exit(-FAIL);
		}

		if(seq_write(fd)!=OK)
			exit(-FAIL);
		if(seq_read(fd)!=OK)
			exit(-FAIL);

		/*
		 * Now lets through in some readlink calls
		 */

		if (symlink("/aaa/bbb/ccc/ddd/eee/fff", "Test_link") != OK) {
			perror("symlink");
			if (errno != EEXIST)
				exit(-FAIL);
		}

		for (j = 0; j < NREAD_LINK; j++) {
			if (readlink("Test_link", b, BUFSIZ) == -1) {
				perror("readlink");
				exit(-FAIL);
			}
		}


		if(nseq_read(fd)!=OK)
			exit(-FAIL);

		/*
		 * We put the unlink out here because a create retransmit
		 * could get us.
		 */
		(void)unlink("Test_link");
		/*
		 * Now lets throw in create requests
		 */

		for (j = 0; j < FILE_CREATES; j++) {
			if ((cfd = open("test_create", O_RDWR|O_CREAT|O_TRUNC,
				0666)) < 0) {
				perror("open");
				exit(-FAIL);
			}
			/*
			 * Force the create to get out of cache
			 */
			(void)fsync(cfd);
			(void)close(cfd);
		}

		if(seq_read(fd)!=OK)
			exit(-FAIL);
		/*
		 * We put the unlink out here because a create retransmit
		 * could get us.
		 */

		(void)unlink("test_create");
		if(seq_read(fd)!=OK)
			exit(-FAIL);

		/*
		 * Now we look up non existent files
		 */
		for (j = 0; j < FILE_LOOKUPS; j++) {
			char sb[BUFSIZ];
			int pp = getpid();

			(void)sprintf(sb, "xx%ld%d%d", j,pp,i);
			(void)access(sb, F_OK);
		}

		if(seq_read(fd)!=OK)
			exit(-FAIL);
		if(nseq_read(fd)!=OK)
			exit(-FAIL);
		if(seq_read(fd)!=OK)
			exit(-FAIL);
		if(seq_read(fd)!=OK)
			exit(-FAIL);
		if(seq_read(fd)!=OK)
			exit(-FAIL);
		if(nseq_read(fd)!=OK)
			exit(-FAIL);

		(void)close(fd);

		/*
		 * Now we rename the file
		 */
		if (rename(s, "Test_of_rename") != OK) {
			perror("rename");
			exit(-FAIL);
		}

		/*
		 * Finally get rid of the file
		 */
		if (unlink("Test_of_rename") != OK) {
			perror("unlink");

			/*
			 * Busy servers cause the client to retransmit the
			 * request, giving the ENOENT since the file was
			 * removed by the first request, so we ignore this.
			 * (Maybe we should penalize 1 NFS stone/sec ??)
			 */
			if (errno != ENOENT)
				exit(-FAIL);
		}
		sync();
	}
	exit(OK);
}

/*
 * Write a file descriptor sequentially
 */
int seq_write(fd)
	int fd;
{
	long j;

	/*
	 * Write the file sequentially
	 */

	if (lseek(fd, 0L, L_SET) == -1) {
		perror("lseek");
		return FAIL;
	}

	for (j = 0; j < BLOCKS_PER_FILE; j++) {
		buf[0] = j; /* Minimal sanity check */
		if (write(fd,(char *)buf, BYTES_PER_BLOCK) != BYTES_PER_BLOCK) {
			perror("write");
			return FAIL;
		}
	}

	(void)fsync(fd);
	return OK;
}

int seq_read(fd)
	int fd;
{
	long j;

	/*
	 * Read sequentially
	 */
	if (lseek(fd, 0L, L_SET) == -1) {
		perror("lseek");
		return FAIL;
	}

	for (j = 0; j < BLOCKS_PER_FILE; j++) {
		if (read(fd, (char *)buf, BYTES_PER_BLOCK) != BYTES_PER_BLOCK) {
			perror("read");
			return FAIL;
		}

		if (buf[0] != j) {
			(void)fprintf(stderr, "Data corruption error\n");
			return FAIL;
		}
	}
	return OK;
}

int nseq_read(fd)
	int fd;
{
	long j;

	/*
	 * Read back the file not in order.
	 */
	for (j = 0; j < BLOCKS_PER_FILE/2; j++) {
		if (lseek(fd, j * BYTES_PER_BLOCK, L_SET) == -1) {
			perror("lseek");
			return FAIL;
		}

		if (read(fd, (char *)buf, BYTES_PER_BLOCK) != BYTES_PER_BLOCK) {
			perror("read");
			return FAIL;
		}

		if (buf[0] != j) {
			(void)fprintf(stderr, "Data corruption error\n");
			return FAIL;
		}

		if (lseek(fd, (BLOCKS_PER_FILE - (j + 1)) *
			BYTES_PER_BLOCK, L_SET) == -1) {
			perror("lseek");
			return FAIL;
		}

		if (read(fd, (char *)buf, BYTES_PER_BLOCK) != BYTES_PER_BLOCK) {
			perror("read");
			return FAIL;
		}

		if (buf[0] != (BLOCKS_PER_FILE - (j + 1))) {
			(void)fprintf(stderr, "Data corruption error\n");
			return FAIL;
		}
	}
	return OK;
}
