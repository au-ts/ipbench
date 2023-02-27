#include <pthread.h>

/* You need to profile to get some reasonable number here.  This
 * should be fine for IA64 */
#define PROFILE_CONTEXT_COST 300

#ifdef __ia64__
#define get_cycles() ia64_get_itc()
#elif __alpha__
#define get_cycles() alpha_get_cycles()
#elif __i386__
#define get_cycles() i386_get_cycles()
#elif __amd64__
#define get_cycles() amd64_get_cycles()
#elif __aarch64__
#define get_cycles() aarch64_get_cycles()
#else
#error get_cycles() not written for your architecture
#endif

#ifdef __ia64__
typedef unsigned long cycle_t;

static __inline__ cycle_t
ia64_get_itc (void)
{
        unsigned long result;

        __asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory");

        return result;
}
#endif

#ifdef __alpha__
typedef uint32_t cycle_t;

static inline cycle_t
get_cycles(void)
{
	cycle_t ret;
	asm volatile("rpcc %0": "=r" (ret));
	return ret;
}
#endif

#ifdef __i386__
typedef uint64_t cycle_t;

static inline cycle_t 
i386_get_cycles(void)
{
	cycle_t result;
	__asm__ __volatile__("rdtsc" : "=A" (result));
	return result;
}
#endif

#ifdef __amd64__
typedef uint64_t cycle_t;

static inline cycle_t
amd64_get_cycles(void)
{
        cycle_t result;
        unsigned int a,d;
        asm volatile("rdtsc" : "=a" (a), "=d" (d));
        result = ((unsigned long)a) | (((unsigned long)d)<<32);
        return result;
}
#endif

#ifdef __aarch64__
typedef uint64_t cycle_t;
static inline cycle_t
aarch64_get_cycles(void)
{
	uint64_t tsc;
	asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));
	return tsc;
}
#endif

/* Timer data is held in a struct like this */
struct timer_buffer_t
{
	cycle_t idle;
	cycle_t total;
};
