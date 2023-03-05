/*
 * Generic `Get a timestamp' code.
 * Based on the code from `realfeel3'
 */

#ifndef MICROUPTIME_H
#define MICROUPTIME_H

#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#ifndef US_PER_S
#define US_PER_S (1000000)
#endif

typedef uint64_t clk_t;

extern double tick_rate;

static inline clk_t tick_to_usec(clk_t ticks)
{
	return (clk_t)(ticks / tick_rate);
}

static inline clk_t usec_to_tick(clk_t usec)
{
	return (clk_t)(usec * tick_rate);
}

static inline clk_t real_time(void)
{
	clk_t r;
	struct timeval t = { 0, 0 };

	assert(gettimeofday(&t, NULL) == 0);

	r = (uint64_t)t.tv_sec * US_PER_S;
	r += t.tv_usec;

	return r;
}

#if defined(__ia64__)
static inline clk_t time_stamp(void)
{
  clk_t result;
  __asm__ __volatile__("mov %0=ar.itc;;" : "=r"(result) :: "memory");
  return result;
}
#elif defined(__i386__)
static inline clk_t time_stamp(void)
{
  clk_t result;
   __asm__ __volatile__("rdtsc" : "=A" (result));
  return result;
}
#elif defined(__amd64__)
static inline clk_t time_stamp(void)
{
  clk_t result;
  unsigned int a,d;
  asm volatile("rdtsc" : "=a" (a), "=d" (d));
  result = ((unsigned long)a) | (((unsigned long)d)<<32);
  return result;
}

#elif defined(__aarch64__)
static inline clk_t time_stamp(void)
{
  clk_t result;
  asm volatile ("mrs %0, cntvct_el0" : "=r" (result));
  return result;
}
#else
static inline clk_t time_stamp(void)
{
	return real_time();
}

#endif

void microuptime_calibrate(void);

#endif /* MICROUPTIME_H */
