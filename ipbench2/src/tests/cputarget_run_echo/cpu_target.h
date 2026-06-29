#ifndef CPU_TARGET_H
#define CPU_TARGET_H

#define N_CPUS           64
#define CACHE_LINE_SIZE  128
#define BLOCK_SIZE       512

#include <sys/types.h>
#include <time.h>

/* Shared extern declarations from cpu_target.c */
extern long int        cpu_target_count_cpus(void);
extern double          cpu_target_average_cpu(void);
extern void            cpu_target_calibrate(void);
extern void            cpu_target_do_cyclesoak(void);
extern void            cpu_target_exit_handler(void);
extern int             cpu_target_parse_arg(char *arg);

/* Shared state - defined in cpu_target.c */
extern int             running;
extern long int        nr_cpus;
extern int             warmup_time;
extern int             cooldown_time;
extern double         *cpu_load;
extern long int        cpu_samples;
extern pid_t           cpu_target_kids[N_CPUS];

/* Calibration state - these are defined in cpu_target.c */
extern int             do_calibrate;
extern unsigned int    calibration_complete;

#endif /* CPU_TARGET_H */

