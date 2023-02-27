#define _GNU_SOURCE
#include <features.h>

#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define NTHREADS 1000
#define THREAD_STACK_SIZE (4*PTHREAD_STACK_MIN)
#define JOIN_STACK_SIZE (PTHREAD_STACK_MIN)

#define dbprintf printf

__thread int sockfd;

struct sockaddr_in serv_addr;

typedef struct threads_struct {
	pthread_t td;
	pthread_t jointd;
	pthread_attr_t attr;
	pthread_attr_t joinattr;
	void *sp;
	void *joinsp;
	char recv[BUFSIZ];
	int flag;
	pthread_mutex_t join;
	pthread_cond_t joining;
} threads_struct_t;

threads_struct_t threads[NTHREADS];

/* keep track of the threads currently inflight */
typedef struct inflight_struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
} inflight_struct_t;

inflight_struct_t inflight = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.cond = PTHREAD_COND_INITIALIZER,
	.count = 0,
};

/* loop through our thread list and find any usable threads */
static inline int find_free_thread(void)
{
	int i;
	for(i=0 ; i < NTHREADS ; i++)
		if (threads[i].flag == -1)
			return i;
	return -1;
}

/* this thread sits and when woken, pthread_joins the new thread.
 * when it knows the thread is complete, it will flag that the new
 * threads stack is free to be reused.
 */
void *joiner(void *arg) 
{
	threads_struct_t *t = (threads_struct_t*)arg;

	while (1) {
		/* we sit and wait for the cond variable to wake us up */
		pthread_mutex_lock(&t->join);
		pthread_cond_wait(&t->joining, &t->join);
		
		dbprintf("[%d] joining\n", t->flag);

		int slot = t->flag;

		/* once woken, join with the master thread id */
		pthread_join(t->td, NULL);
		
		dbprintf("[%d] done\n", t->flag);

		/* when it is complete, the stack can be reused */
		inflight.count--;
		threads[t->flag].flag = -1;
		pthread_mutex_unlock(&t->join);

		/* if the main thread is busy (i.e. we don't get the
		 * lock) then we know that the main thread will be
		 * looping and notice our flag is reset and we can be
		 * reused.  On the other hand, if we get the lock, the
		 * main process must be sleeping, so we wake it up.
		 * Either way, we are OK!
		 */

		if (pthread_mutex_trylock(&inflight.mutex) != EBUSY) {
			pthread_cond_signal(&inflight.cond);
			pthread_mutex_unlock(&inflight.mutex);
		}	

		dbprintf("[%d] signal reusable\n", slot);
	}
}


/* this thread connects up to the client to be flooded */
void *thread(void *arg)
{
	int t = (int)arg, r;
	dbprintf("[%d] socket\n",t);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "error opening socket: %s\n", 
			strerror(errno));
		exit(1);
	}

	dbprintf("[%d] connect\n",t);
	r = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (r < 0) {
		fprintf(stderr, "error connecting socket: %s\n",
			strerror(errno));
		exit(1);
	}

	dbprintf("[%d] send GET\n",t);
	send(sockfd, "GET /\r", strlen("GET /\r"),0);
	
	dbprintf("[%d] recv start\n",t);
	r = recv(sockfd, threads[t].recv, BUFSIZ,0);

	dbprintf("[%d] recv %d bytes\n",t,r);

	dbprintf("[%d] close\n",t);
	close(sockfd);

	dbprintf("[%d] lock\n", t);

	dbprintf("[%d] exit\n", t);
	return 0;
}


/* the main thread. */
int main(int argc, char *argv[])
{
	struct hostent *server;
	int i;

	if ( argc < 3 ) {
		printf("usage: spawner hostname port\n");
		exit(1);
	}

	for(i=0; i < NTHREADS; i++) {

		threads[i].flag = -1; //flag each thread as available

		/* we need our own stacks as the thread library
		 * doesn't handle creating and destroying hundreds or
		 * thousands of threads repeatadly.
		 */
		posix_memalign (&threads[i].sp, getpagesize(), THREAD_STACK_SIZE);
		posix_memalign (&threads[i].joinsp, getpagesize(), JOIN_STACK_SIZE);
		if (threads[i].sp == NULL || threads[i].joinsp == NULL) {
			fprintf(stderr, "error creating stacks : %s\n", strerror(errno));
			exit(1);
		}
		pthread_attr_init(&threads[i].attr);
		if (pthread_attr_setstack(&threads[i].attr, threads[i].sp, THREAD_STACK_SIZE) != 0) {
			fprintf(stderr,"error creating stack for thread %d : %s\n", i,
				strerror(errno));
			exit(1);
		}
		pthread_attr_init(&threads[i].joinattr);
		if (pthread_attr_setstack(&threads[i].joinattr, threads[i].joinsp, JOIN_STACK_SIZE) != 0) {
			fprintf(stderr,"error creating stack for thread %d : %s\n", i,
				strerror(errno));
			exit(1);
		}
		
		/* create the joiner thread for each thread slot */
		if (pthread_create(&threads[i].jointd, &threads[i].joinattr, joiner, (void*)&threads[i]) != 0) {
			fprintf(stderr,"error creating joiner thread %d : %s\n", i, strerror(errno));
			exit(1);
		}			
	}
		

	dbprintf("server is %s\n", argv[1]);
	server = gethostbyname(argv[1]);
	if (server == NULL) {
		fprintf(stderr, "can't resolve %s : %s\n", 
			argv[1], strerror(errno));
		exit(1);
	}

	serv_addr.sin_family = AF_INET;
	bcopy(server->h_addr, &serv_addr.sin_addr.s_addr, 
	      server->h_length);
	serv_addr.sin_port = htons(80);
	
	while (1) 
	{
		/* attempt to keep a large number of threads in flight */
		pthread_mutex_lock(&inflight.mutex);
		printf("%d threads in flight\n", inflight.count);
		while ( inflight.count < NTHREADS ) {
			int t = find_free_thread();
			if (t == -1) {
				fprintf(stderr, "No free slot?\n");
				exit(1);
			}
				
			dbprintf("Starting thread slot %d, stack %p\n",t, threads[t].sp);

			pthread_mutex_lock(&threads[t].join);
			if (pthread_create(&threads[t].td, &threads[t].attr, thread, (void *)t) != 0) {
				fprintf(stderr,"error creating thread %d : %s\n", t, strerror(errno));
				exit(1);
			}
			threads[t].flag = t;
			inflight.count++;
			/* signal to the joiner thread that it should
			 * join with our new thread (it knows to join
			 * to threads[t].td)
			 */
			pthread_cond_signal(&threads[t].joining);
			pthread_mutex_unlock(&threads[t].join);

		}
		dbprintf("about to wait\n");
		/* If we are here, then all our threads are running
		 * and we have nothing to do.  sit and wait for a
		 * signal from someone that we can start more
		 * threads */
		pthread_cond_wait(&inflight.cond, &inflight.mutex);
		dbprintf("ok, back to running more threads\n");

		pthread_mutex_unlock(&inflight.mutex);
	}

}
