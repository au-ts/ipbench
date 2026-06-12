#define _GNU_SOURCE
#define IPBENCH_TEST_TARGET
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include "plugin.h"
#include "ipbench.h"
#include "cpu_target.h"

/* iperf3 plugin-specific state */
static int fancy_output = 0;
static int num_iperf3_servers = 0;
static int iperf3_start_port = 5201;
static char iperf3_server_flags[256] = "";
static pid_t iperf3_pids[32];
static int num_samples;
static int do_calibrate;

/* launch iperf3 servers */
static int launch_iperf3_servers(void)
{
        int i;
        char cmd[512];

        if (num_iperf3_servers < 1 || num_iperf3_servers > 32) {
                dbprintf("Invalid number of iperf3 servers: %d (must be 1-32)\n",
                         num_iperf3_servers);
                return -1;
        }

        for (i = 0; i < num_iperf3_servers; i++) {
                iperf3_pids[i] = fork();

                if (iperf3_pids[i] < 0) {
                        dbprintf("Fork failed for iperf3 server %d: %s\n",
                                 i, strerror(errno));
                        return -1;
                }

                if (iperf3_pids[i] == 0) {
                        int port = iperf3_start_port + i;

                        snprintf(cmd, sizeof(cmd), "iperf3 -s -p %d %s", port,
                                 iperf3_server_flags);

                        dbprintf("Launching iperf3 server on port %d\n", port);
                        execl("/bin/sh", "sh", "-c", cmd, NULL);

                        dbprintf("execl failed for iperf3: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                dbprintf("Launched iperf3 server %d with PID %d (port %d)\n",
                         i, iperf3_pids[i], iperf3_start_port + i);
        }

        usleep(500000);

        return 0;
}

/* cleanup iperf3 servers */
static void cleanup_iperf3_servers(void)
{
        int i;
        int status;

        if (num_iperf3_servers < 1)
                return;

        for (i = 0; i < num_iperf3_servers; i++) {
                if (iperf3_pids[i] > 0) {
                        dbprintf("Terminating iperf3 server %d (PID %d)\n",
                                 i, iperf3_pids[i]);
                        kill(iperf3_pids[i], SIGTERM);

                        usleep(100000);
                        if (kill(iperf3_pids[i], 0) == 0) {
                                kill(iperf3_pids[i], SIGKILL);
                                waitpid(iperf3_pids[i], &status, WNOHANG);
                        }
                        iperf3_pids[i] = 0;
                }
        }
}

/* Parse iperf3-specific arguments (calls core parser first) */
static int parse_arg(char *arg)
{
        char *c, *cmd, *val;
        char *arg_ptr = NULL, *cmd_ptr = NULL;

        if (cpu_target_parse_arg(arg))
                return -1;

        if (strlen(arg) == 0)
                return 0;

        c = strtok_r(arg, ",", &arg_ptr);
        if (c == NULL)
                return 0;
        cmd = strtok_r(c, "=", &cmd_ptr);
        val = strtok_r(NULL, "=", &cmd_ptr);

        do {
                dbprintf("Got cmd %s val %s\n", cmd, val);

                if (!strcmp(cmd, "fancy_output"))
                        fancy_output = 1;
                else if (!strcmp(cmd, "num_iperf3_servers"))
                        num_iperf3_servers = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "start_port"))
                        iperf3_start_port = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "server_flags")) {
                        strncpy(iperf3_server_flags, val, sizeof(iperf3_server_flags) - 1);
                        iperf3_server_flags[sizeof(iperf3_server_flags) - 1] = '\0';
                }

                if ((c = strtok_r(NULL, ",", &arg_ptr)) == NULL)
                        break;
                cmd_ptr = NULL;
                cmd = strtok_r(c, "=", &cmd_ptr);
                val = strtok_r(NULL, "=", &cmd_ptr);

        } while (1);

        return 0;
}

int iperf3_cpu_target_controller_setup(char *arg)
{
        return 0;
}

int iperf3_cpu_target_setup(char *arg)
{
        struct itimerval it = {{0,0},{0,0},};

        dbprintf("Setup test.\n");

        if (strlen(arg) != 0)
                if (parse_arg(arg))
                        return -1;

        nr_cpus = cpu_target_count_cpus();
        if (nr_cpus < 0)
                return -1;

        if (cooldown_time)
                num_samples = (2 * ((cooldown_time + 1)));
        else
                num_samples = 3600;
        dbprintf("Averaging (up to) %d samples\n", num_samples);
        cpu_load = malloc(num_samples * sizeof(double));

        do_calibrate = 1;
        cpu_target_calibrate();
        do_calibrate = 0;
        dbprintf("calibrated OK\n");
        cpu_target_exit_handler();

        cpu_samples = 0;
        if (setitimer(ITIMER_REAL, &it, 0)) {
                perror("setitimer");
                exit(1);
        }

        if (num_iperf3_servers > 0) {
                if (launch_iperf3_servers() != 0) {
                        dbprintf("Failed to launch iperf3 servers\n");
                        return -1;
                }
        }

        return 0;
}

int iperf3_cpu_target_start(struct timeval *start)
{
        gettimeofday(start, NULL);
        dbprintf("Starting.\n");
        cpu_target_do_cyclesoak();
        dbprintf("Complete.\n");
        cpu_target_exit_handler();
        return 0;
}

int iperf3_cpu_target_stop(struct timeval *stop)
{
        gettimeofday(stop, NULL);
        running = 0;
        cleanup_iperf3_servers();
        dbprintf("Stopping.\n");
        return 0;
}

int iperf3_cpu_target_marshall(char **data, int *size, double running_time)
{
        static char buf[BUFSIZ];
        char *bp;

        double av = cpu_target_average_cpu();

        snprintf(buf, BUFSIZ, "%.1f", av*100);
        bp = buf;
        dbprintf("Average CPU time is %5.1f%%.\n", av*100);
        for (int i = 0; i < nr_cpus; i++) {
                int len;
                bp = strdup(bp);
                snprintf(buf, BUFSIZ, ",CPU%d: %.1f", i, cpu_load[i]*100);
                len = strlen(bp) + strlen(buf) + 1;
                bp = realloc(bp, len);
                strncat(bp, buf, len);
        }

        *data = bp;
        *size = strlen(bp) + 1;

        return 0;
}

void iperf3_cpu_target_marshall_cleanup(char **data)
{
        free(cpu_load);
}

int iperf3_cpu_target_unmarshall(char *input, int input_len, char **data,
                                  int *data_len)
{
        dbprintf("[iperf3_cpu_target_unmarshall] start\n");
        char *buf = calloc(sizeof(char),input_len);
        *data = buf;
        memcpy(buf, input, input_len);
        *data_len = input_len;
        dbprintf("[iperf3_cpu_target_unmarshall] cpu usage %s\n", (char*)(*data));
        return 0;
}

void iperf3_cpu_target_unmarshall_cleanup(char **data)
{
        free(*data);
}

int iperf3_cpu_target_output(struct client_data *data)
{
        if (fancy_output)
                printf("CPU USAGE: ");
        printf("%s\n", (char *)data->data);
        return 0;
}

struct ipbench_plugin ipbench_plugin =
{
        .magic = "IPBENCH_PLUGIN",
        .name = "iperf3_cpu_target",
        .id = 0x21,
        .descr = "Measure CPU usage with concurrent iperf3 servers",
        .default_port = 7,
        .type = IPBENCH_TARGET,
        .setup = &iperf3_cpu_target_setup,
        .setup_controller = &iperf3_cpu_target_controller_setup,
        .start = &iperf3_cpu_target_start,
        .stop = &iperf3_cpu_target_stop,
        .marshall = &iperf3_cpu_target_marshall,
        .marshall_cleanup = &iperf3_cpu_target_marshall_cleanup,
        .unmarshall = &iperf3_cpu_target_unmarshall,
        .unmarshall_cleanup = &iperf3_cpu_target_unmarshall_cleanup,
        .output = &iperf3_cpu_target_output,
};

