#define _GNU_SOURCE
#define IPBENCH_TEST_TARGET
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "plugin.h"
#include "ipbench.h"
#include "cpu_target.h"

#define MAX_PATH_LEN (4096)
#define MAX_CMD_LEN (16384)  // Larger buffer for command line

static int fancy_output = 0;
static int num_threads = 1;
static int start_port = 5201;
static pid_t server_pid = 0;
static int num_samples;
static int do_calibrate;
static char exec_path[MAX_PATH_LEN] = "";
static char server_flags[MAX_PATH_LEN] = "";

static int launch_server(void)
{
        char cmd[MAX_CMD_LEN];
        int ret;

        if (strlen(exec_path) == 0) {
                dbprintf("No exec path specified\n");
                return -1;
        }

        server_pid = fork();

        if (server_pid < 0) {
                dbprintf("Fork failed for server: %s\n", strerror(errno));
                return -1;
        }

        if (server_pid == 0) {
                /* Child process - exec the echo server */
                ret = snprintf(cmd, sizeof(cmd), "%s -p %d -P %d %s",
                              exec_path, start_port, num_threads, server_flags);

                if (ret < 0 || (size_t)ret >= sizeof(cmd)) {
                        dbprintf("Command line truncated\n");
                        exit(EXIT_FAILURE);
                }

                dbprintf("Launching server: %s\n", cmd);
                execl("/bin/sh", "sh", "-c", cmd, NULL);

                /* If execl returns, it failed */
                dbprintf("execl failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }

        dbprintf("Launched server with PID %d (port %d, threads %d)\n",
                 server_pid, start_port, num_threads);

        /* Give server a moment to start */
        usleep(500000);

        return 0;
}

static void cleanup_server(void)
{
        int status;

        if (server_pid <= 0)
                return;

        dbprintf("Terminating server (PID %d)\n", server_pid);

        kill(server_pid, SIGTERM);

        usleep(100000);
        if (kill(server_pid, 0) == 0) {
                kill(server_pid, SIGKILL);
                waitpid(server_pid, &status, WNOHANG);
        }
        server_pid = 0;
}

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
                else if (!strcmp(cmd, "num_threads"))
                        num_threads = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "start_port"))
                        start_port = strtol(val, (char**)NULL, 10);
                else if (!strcmp(cmd, "exec_path")) {
                        strncpy(exec_path, val, sizeof(exec_path) - 1);
                        exec_path[sizeof(exec_path) - 1] = '\0';
                }
                else if (!strcmp(cmd, "server_flags")) {
                        strncpy(server_flags, val, sizeof(server_flags) - 1);
                        server_flags[sizeof(server_flags) - 1] = '\0';
                }

                if ((c = strtok_r(NULL, ",", &arg_ptr)) == NULL)
                        break;
                cmd_ptr = NULL;
                cmd = strtok_r(c, "=", &cmd_ptr);
                val = strtok_r(NULL, "=", &cmd_ptr);

        } while (1);

        return 0;
}

int cpu_target_controller_setup(char *arg)
{
        return 0;
}

int cpu_target_setup(char *arg)
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

        /* Launch the echo server if configured */
        if (strlen(exec_path) > 0) {
                if (launch_server() != 0) {
                        dbprintf("Failed to launch server\n");
                        return -1;
                }
        }

        return 0;
}

int cpu_target_start(struct timeval *start)
{
        gettimeofday(start, NULL);
        dbprintf("Starting.\n");
        cpu_target_do_cyclesoak();
        dbprintf("Complete.\n");
        cpu_target_exit_handler();
        return 0;
}

int cpu_target_stop(struct timeval *stop)
{
        gettimeofday(stop, NULL);
        running = 0;
        cleanup_server();
        dbprintf("Stopping.\n");
        return 0;
}

int cpu_target_marshall(char **data, int *size, double running_time)
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

void cpu_target_marshall_cleanup(char **data)
{
        free(cpu_load);
}

int cpu_target_unmarshall(char *input, int input_len, char **data,
                                  int *data_len)
{
        dbprintf("[cpu_target_unmarshall] start\n");
        char *buf = calloc(sizeof(char),input_len);
        *data = buf;
        memcpy(buf, input, input_len);
        *data_len = input_len;
        dbprintf("[cpu_target_unmarshall] cpu usage %s\n", (char*)(*data));
        return 0;
}

void cpu_target_unmarshall_cleanup(char **data)
{
        free(*data);
}

int cpu_target_output(struct client_data *data)
{
        if (fancy_output)
                printf("CPU USAGE: ");
        printf("%s\n", (char *)data->data);
        return 0;
}

struct ipbench_plugin ipbench_plugin =
{
        .magic = "IPBENCH_PLUGIN",
        .name = "cputarget_run_echo",
        .id = 0x21,
        .descr = "Launch echo target from host machine and record cpu usage",
        .default_port = 7,
        .type = IPBENCH_TARGET,
        .setup = &cpu_target_setup,
        .setup_controller = &cpu_target_controller_setup,
        .start = &cpu_target_start,
        .stop = &cpu_target_stop,
        .marshall = &cpu_target_marshall,
        .marshall_cleanup = &cpu_target_marshall_cleanup,
        .unmarshall = &cpu_target_unmarshall,
        .unmarshall_cleanup = &cpu_target_unmarshall_cleanup,
        .output = &cpu_target_output,
};

