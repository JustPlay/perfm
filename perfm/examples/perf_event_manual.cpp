#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <sys/ioctl.h>
#include <asm/unistd.h>

#include <perfmon/perf_event.h>  /* <linux/perf_event.h> */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/syscall.h>

#if 0 /* use perf_event_open() provided in <perfmon/perf_event.h> */

static long perf_event_open(struct perf_event_attr *pea, pid_t pid, int cpu, int grp_fd, unsigned long flags)
{
    long ret = 0;

    if (!pea.size) {
        pea->size = sizeof(struct perf_event_attr);
    }

    ret = syscall(__NR_perf_event_open, pea, pid, cpu, grp_fd, flags);

    return ret;
}

#endif

int main(int argc, char **argv)
{
    struct perf_event_attr event;
    unsigned long long counter[] = {0, 0, 0};
    int perf_fd;

    if (access("/proc/sys/kernel/perf_event_paranoid", F_OK) != 0) {
        fprintf(stderr, "perf_event_open() is not supported!\n");
        exit(EXIT_FAILURE);
    }

    memset(&event, 0, sizeof(struct perf_event_attr));

    event.type   = PERF_TYPE_HARDWARE;
    event.config = PERF_COUNT_HW_INSTRUCTIONS;
    event.size   = sizeof(struct perf_event_attr);

    event.disabled       = 1;
    event.exclude_kernel = 1;
    event.exclude_hv     = 1;
    event.read_format    = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    perf_fd = perf_event_open(&event, 0, -1, -1, 0);
    if (perf_fd == -1) {
        fprintf(stderr, "perf_event_open() failed\n");
        exit(EXIT_FAILURE);
    }

    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);

    fprintf(stdout, "Measuring instruction count for this printf\n");

    ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);

    read(perf_fd, counter, sizeof(counter));

    fprintf(stdout, "PMU_RAW:           %lld\n"
                    "PMU_TIME_ENABLED:  %lld\n"
                    "PMU_TIME_RUNNGING: %lld\n", 
            counter[0], counter[1], counter[2]);

    close(perf_fd);

    return 0;
}
