#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

__attribute__((weak)) long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, 
                                           int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

//
// https://gcc.gnu.org/onlinedocs/gcc/Function-Attributes.html
//

//
// perf_event related configuration files in /proc/sys/kernel/
// - /proc/sys/kernel/perf_event_paranoid
//       The existence of the perf_event_paranoid file is the official method for
//       determining if a kernel supports perf_event_open().
//
// - /proc/sys/kernel/perf_event_max_sample_rate
//
// - /proc/sys/kernel/perf_event_max_stack
//
// - /proc/sys/kernel/perf_event_mlock_kb
//
// perf_event related configuration files in /sys/bus/event_source/devices/
//   Since Linux 2.6.34, the kernel supports having multiple PMUs available for monitoring.
//   Information on how to program these PMUs can be found under /sys/bus/event_source/devices/.
//   Each subdirectory corresponds to a different PMU.
//
// - /sys/bus/event_source/devices/*/type (since Linux 2.6.38)`
//       This contains an integer that can be used in the type field of perf_event_attr to 
//       indicate that you wish to use this PMU.
//
// - /sys/bus/event_source/devices/cpu/rdpmc (since Linux 3.4)
//
// - /sys/bus/event_source/devices/*/format/ (since Linux 3.4)
//       This subdirectory contains information on the architecture-specific subfields
//       available for programming the various config fields in the perf_event_attr struct.
//
// - /sys/bus/event_source/devices/*/events/ (since Linux 3.4)
//       This subdirectory contains files with predefined events.
//
// - /sys/bus/event_source/devices/*/uevent
//       This file is the standard kernel device interface for injecting hotplug events.
//
// - /sys/bus/event_source/devices/*/cpumask (since Linux 3.7)
//       The cpumask file contains a comma-separated list of integers that indicate a 
//       representative CPU number for each socket (package) on the motherboard.
//       This is needed when setting up uncore or northbridge events, as those PMUs
//       present socket-wide events.
