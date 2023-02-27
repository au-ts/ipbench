#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <inttypes.h>


#include "cpu_target_lukem.h"

#define PROFILE_TO_AVG 1000000
cycle_t profiles[PROFILE_TO_AVG];

volatile struct timer_buffer_t timer_buffer;

int calc = 1;

void sig_int(int arg) {

	calc = 0;

	usleep(500000);

	printf("%.1f\n",
	       (1.f - 
		((double)timer_buffer.idle/(double)timer_buffer.total)) 
	       * 100 );
	exit(0);

}

void *idle_thread(void *arg)
{
	uint64_t x0, x1, delta, total, idle;
	uint64_t cal_count = PROFILE_TO_AVG;
	
	idle = total = 0;
	x0 = get_cycles();

	while (calc) {
		uint64_t start = get_cycles();

		x1 = x0;
		x0 = get_cycles();
		
		delta = x0 - x1;
		total += delta;
		
		if (delta < PROFILE_CONTEXT_COST)
		{
			idle += delta;
		}
		
		timer_buffer.idle = idle;
		timer_buffer.total = total;

		uint64_t stop = get_cycles();
		profiles[cal_count] =  (stop - start);
		if (cal_count) 
			cal_count--;
		else {
			for (cal_count = PROFILE_TO_AVG; cal_count > 0 ; cal_count--)
				printf("%"PRId64"\n", profiles[cal_count]);
			exit(0);
		}
	       
	}
	return 0;
}


int main(int argc, char *argv[])
{

	pthread_t t;

	signal(SIGINT, sig_int);

	pthread_create(&t, NULL, idle_thread, NULL);
	
	calc = 1;

	pthread_join(t, NULL);

	exit(0);

}


