/* Latency CPU test - core measurement functions.
 *
 * The parts of this code that do anything interesting comes from
 * cyclesoak by Andrew Morton <akpm@zip.com.au>
 * http://www.zipworld.com.au/~akpm/linux/
 *
 * Bits stripped out and added by Ian Wienand
 * <ianw@gelato.unsw.edu.au>
 */

#define _GNU_SOURCE

#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>

#include "cpu_target.h"
#include "ipbench.h"

// hack: we can't include plugin.h here so we bring this in manually
extern void dbprintf (const char *, ...);
extern int do_debug;

static int              debug;
static int              do_calibrate = 0;
static int              calibrate_state = 0;
static unsigned int     calibration_complete = 0;
static struct timeval   alarm_time;
static unsigned long    calibrated_loops_per_sec[N_CPUS];
static unsigned long    counts_per_sec[N_CPUS];
static double           d_counts_per_sec[N_CPUS];
static char*            busyloop_buf;
static unsigned long    busyloop_size = 1000000;
static unsigned long*   busyloop_progress;
static long int         period_secs = 1;
static int              rw_ratio = 5;
static int              cacheline_size = 32;
static unsigned long    sum;
static unsigned long    twiddle = 1000;
static int              do_bonding = 1;

pid_t                   cpu_target_kids[N_CPUS];

long int                nr_cpus;
int                     warmup_time;
int                     cooldown_time;
double                 *cpu_load = NULL;
long int                cpu_samples = 0;
int                     running = 1;

static int              n_samples;
static long int         warmup_samples = 0;

#define CPS_FILE "counts_per_sec"
#define DB_KEY_SIZE     64
#define DB_DATA_SIZE    64

long int cpu_target_count_cpus(void)
{
        FILE *fp;
        long int n = -1UL;
        char nr_cpus_str[8];

        fp = popen("cat /proc/cpuinfo | grep processor | wc -l", "r");
        if (fp == NULL || fgets(nr_cpus_str, sizeof(nr_cpus_str)-1, fp) == NULL)
                dbprintf("Failed to count number of processors. Not performing bonding.\n" );
        else {
                n = strtol(nr_cpus_str, NULL, 10);
                dbprintf("%d cpus found\n", nr_cpus);
        }
        pclose(fp);

        if (n > N_CPUS) {
                dbprintf("Recompile with more than %d N_CPU --- too many for test\n", N_CPUS);
                return -1L;
        }
        return n;
}

double cpu_target_average_cpu(void)
{
        double sum = 0;
        int i;
        int tosum;

        if (cooldown_time) {
                if ((cpu_samples) < n_samples/2) {
                        dbprintf("Not enough samples to cover cooldown period!\n");
                        tosum = cpu_samples;
                } else
                        tosum = n_samples/2;
        }
        else
                tosum = cpu_samples;

        dbprintf("Averaging %d samples\n", tosum);

        for(i = 0; i < tosum; i++) {
                sum += cpu_load[i];
        }

        return sum / i;
}

static void busyloop(int instance)
{
        int idx = 0;
        int rw = 0;

        for (;;) {
                int thumb;

                rw++;
                if (rw == rw_ratio) {
                        busyloop_buf[idx]++;                    /* Dirty a cacheline */
                        rw = 0;
                } else {
                        sum += busyloop_buf[idx];
                }

                for (thumb = 0; thumb < twiddle; thumb++)
                        ;                               /* twiddle */

                busyloop_progress[instance * CACHE_LINE_SIZE]++;

                idx += cacheline_size;
                if (idx >= busyloop_size)
                        idx = 0;
        }
}

static void itimer(int sig)
{
        struct timeval tv;
        unsigned long delta;
        unsigned long long blp[N_CPUS];
        unsigned long long total_blp = 0;
        unsigned long long blp_snapshot[N_CPUS];
        static unsigned long long old_blp[N_CPUS];
        char print_out[1024];
        int i;

        gettimeofday(&tv, 0);
        delta = (tv.tv_sec - alarm_time.tv_sec) * 1000000;
        delta += tv.tv_usec - alarm_time.tv_usec;
        delta /= 1000;          /* Milliseconds */

        for (i = 0; i < nr_cpus; i++) {
                blp_snapshot[i] = busyloop_progress[i * CACHE_LINE_SIZE];
                blp[i] = (blp_snapshot[i] - old_blp[i]);
                if (debug)
                        dbprintf("CPU%d: delta=%lu, blp_snapshot=%llu, old_blp=%llu, diff=%llu ",
                                i, delta, blp_snapshot[i], old_blp[i], blp[i]);
                old_blp[i] = blp_snapshot[i];
                blp[i] *= 1000;
                blp[i] /= delta;
                total_blp += blp[i];
        }

        alarm_time = tv;

        if (do_calibrate) {
                if (calibrate_state++ == 3) {
                        for(i = 0; i < nr_cpus; i++) {
                                calibrated_loops_per_sec[i] = blp[i];
                        }
                        calibration_complete = 1;
                }
                snprintf(print_out, sizeof print_out,  "Calibrating -- total: %llu loops/sec", total_blp);
                if (do_bonding) {
                        for(i = 0; i < nr_cpus; i++) {
                            snprintf(print_out + strlen(print_out), sizeof print_out - strlen(print_out),
                                        " || CPU%d: %llu loops/sec", i, blp[i]);
                        }
                }
                dbprintf("%s\n", print_out);

        } else {
                double this_cpu_load[N_CPUS];
                double d_blp[N_CPUS];
                double total_cpu_load = 0;

                for (i = 0; i < nr_cpus; i++) {
                        d_blp[i] = blp[i];
                        this_cpu_load[i] = 1.0 - (d_blp[i] / d_counts_per_sec[i]);

                        if (this_cpu_load[i] < 0) this_cpu_load[i] = 0;
                        total_cpu_load += this_cpu_load[i] / nr_cpus;
                }

                if (warmup_samples * period_secs < warmup_time)
                {
                        if (cpu_samples % 5)
                                dbprintf("Warmup period:%5.1f%%\n", total_cpu_load * 100.0);
                        warmup_samples++;
                }
                else
                {
                        cpu_load[cpu_samples % n_samples] = total_cpu_load;
                        cpu_samples++;
                        if (cpu_samples % 5) {
                                sprintf(print_out, "[%d] System load:%5.1f%%", getpid(), total_cpu_load * 100.0);

                                if (do_bonding) {
                                        for (i = 0; i < nr_cpus; i++)
                                                sprintf(print_out + strlen(print_out), " || CPU%d: %5.1f%%", i, this_cpu_load[i] * 100.0);
                                }
                                dbprintf("%s\n", print_out);
                        }
                }
        }
}

static void prep_cyclesoak(void)
{
        struct itimerval it = {
                { period_secs, 0 },
                { 1, 0 },
        };
        FILE *f;
        char buf[80];
        int cpu;
        cpu_set_t cpu_set;
        int i;

        if (!do_calibrate) {
                f = fopen(CPS_FILE, "r");
                if (!f) {
                        fprintf(stderr, "Please run `cyclesoak -C' on an unloaded system\n");
                        exit(1);
                }

                for (i = 0; i < nr_cpus; i++) {
                        if (fgets(buf, sizeof(buf), f) == 0) {
                                fprintf(stderr, "Reading calibration file failed!\n");
                                exit(1);
                        }
                        counts_per_sec[i] = strtoul(buf, 0, 10);
                        if (counts_per_sec[i] == 0) {
                                fprintf(stderr, "Calibration implies 0 idle time. Likely a bug.\n");
                                exit(1);
                        }
                        d_counts_per_sec[i] = counts_per_sec[i];
                }
                fclose(f);
        }

        busyloop_buf = malloc(busyloop_size);
        if (busyloop_buf == 0) {
                fprintf(stderr, "busyloop_buf: no mem\n");
                exit(1);
        }
        busyloop_progress = mmap(0, nr_cpus * CACHE_LINE_SIZE * sizeof(long),
                                PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
        if (busyloop_progress == MAP_FAILED) {
                perror("mmap");
                exit(1);
        }
        memset(busyloop_progress, 0, nr_cpus * CACHE_LINE_SIZE);

        for (cpu = 0; cpu < nr_cpus; cpu++) {
                if ((cpu_target_kids[cpu]=fork()) == 0) {
                    struct sched_param sp;
                    sp.sched_priority = 0;

                    if (do_bonding) {
                        CPU_ZERO(&cpu_set);
                        CPU_SET(cpu, &cpu_set);
                        sched_setaffinity(cpu_target_kids[cpu], sizeof(cpu_set), &cpu_set);
                    }
                    if (sched_setscheduler(0, SCHED_IDLE, &sp)) {
                        perror("attempt to set to IDLE prio");
                        exit(1);
                    }
                    busyloop(cpu);
                }

        }

        signal(SIGALRM, itimer);
        gettimeofday(&alarm_time, 0);
        if (setitimer(ITIMER_REAL, &it, 0)) {
                perror("setitimer");
                exit(1);
        }
}

void cpu_target_calibrate(void)
{
        int i;

        prep_cyclesoak();

        for ( ; ; ) {
                sleep(10);
                if (calibration_complete) {
                        FILE *f = fopen(CPS_FILE, "w");
                        if (f == 0) {
                                fprintf(stderr, "error opening `%s' for writing\n", CPS_FILE);
                                perror("fopen");
                                exit(1);
                        }
                        for (i = 0; i < nr_cpus; i++) {
                                fprintf(f, "%lu\n", calibrated_loops_per_sec[i]);
                        }
                        fclose(f);
                        return;
                }
        }
}

void cpu_target_do_cyclesoak(void)
{
        prep_cyclesoak();
        while (running)
                sleep(2);
}

void cpu_target_exit_handler(void)
{
        int i;
        for (i=0; i < nr_cpus; i++){
                dbprintf("Killing %d.\n", cpu_target_kids[i]);
                kill(cpu_target_kids[i], SIGKILL);
        }
}

int cpu_target_parse_arg(char *arg)
{
        char *c, *cmd, *val;
        char *arg_ptr = NULL, *cmd_ptr = NULL;

        c = strtok_r(arg, ",", &arg_ptr);
        if (c == NULL) {
                dbprintf("Invalid argument %s.\n", arg);
                return -1;
        }
        cmd = strtok_r(c, "=", &cmd_ptr);
        val = strtok_r(NULL, "=", &cmd_ptr);

        do {
                dbprintf("Got cmd %s val %s\n", cmd, val);
                if (!strcmp(cmd, "cpus"))
                        nr_cpus = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "period"))
                        period_secs = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "warmup"))
                        warmup_time = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "cooldown"))
                        cooldown_time = strtol(val, (char**)NULL, 10);

                if ((c = strtok_r(NULL, ",", &arg_ptr)) == NULL)
                        break;
                cmd_ptr = NULL;
                cmd = strtok_r(c, "=", &cmd_ptr);
                val = strtok_r(NULL, "=", &cmd_ptr);

        } while (1);

        return 0;
}

