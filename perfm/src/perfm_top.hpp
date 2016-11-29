/**
 * perfm_top.hpp - TODO
 *
 */
#ifndef __PERFM_TOP_HPP__
#define __PERFM_TOP_HPP__
#endif

#include "perfm_group.hpp"

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <tuple>

#include <unistd.h>
#include <termios.h>
#include <curses.h>

// how TSC (frequency) was computed in intel's emon: (processor should has 'constant_tsc' flag in /proc/cpuinfo)
// 1 - TSC for a hyperthread/smt is the core/smt's maximum frequency (no turbo), this is a __constant__ value
// 2 - TSC for a core is the summary of each smt's TSC belong to this core
// 3 - TSC for a socket is the summary of each core's TSC belong to this socket
// 4 - TSC for a system is the summary of each socket's TSC belong to this system
//
// TSC (frequency) = Cycles per second = Hz

// in intel's emon:
// 1 - metric_CPU utilization% = CPU_CLK_UNHALTED.REF_TSC / TSC (frequency)
// 2 - metric_CPU utilization% in kernel mode = CPU_CLK_UNHALTED.REF_TSC:SUP / TSC (frequency)
// 3 - CPU_CLK_UNHALTED.REF_TSC : reference cycles when the core is not in halt state.
//
// in libpfm4 CPU_CLK_UNHALTED is an alias of CPU_CLK_THREAD_UNHALTED (EventCode: 0x3c), so
// may be we should use CPU_CLK_THREAD_UNHALTED:REF_XCLK for libpfm4

// Most modern CPUs can change the CPU frequency for various reasons. To save power, CPUs can reduce the CPU 
// frequency. To improve performance, some CPUs boost the frequency above the Time Stamp Counter (TSC) frequency. 
//
// It is useful to know the frequency at which the CPU is running. Two of the fixed counters measure clockticks:
// - CPU_CLK_UNHALTED.REF_TSC : incremented at the same frequency as the TSC (TSC frequency doesn't vary) (fixed cntr 3)
// - CPU_CLK_UNHALTED.THREAD  : incremented at the frequency at which the CPU is running (fixed cntr 2)
// - INST_RETIRED.ANY         : instructions retired from execution (fixed cntr 1)
// Average frequency = TSC_frequency * (CPU_CLK_UNHALTED.THREAD / CPU_CLK_UNHALTED.REF_TSC)
// e.g. in intel's emon:
// metric_CPU operating frequency (in GHz) = CPU_CLK_UNHALTED.THREAD / CPU_CLK_UNHALTED.REF_TSC * system.tsc_freq / 1000000000

// The Intel Nehalem processor is a quad core, dual thread processor. It includes two types of PMU: core and uncore. 
// The uncore PMU measures events at the socket level and is therefore disconnected from any of the four cores. 
// The core PMU implements Intel architectural perfmon version 3 (so does Broadwell-EP) with four generic counters and three fixed counters.
// The uncore has eight generic counters and one fixed counter.

namespace perfm {

class top {

public:
    using ptr_t = std::shared_ptr<top>;

public:
    static constexpr size_t nr_cpus_max = 512;
    static constexpr size_t nr_bit_long = sizeof(unsigned long) << 3;

    static ptr_t alloc();

    virtual ~top() {
        if (_cpu_data) {
            delete [] _cpu_data;
        }
    }

    void open();

    void close();

    void init();
    void fini();

    void loop();

private:
    unsigned long lshift(unsigned long val) const {
        return 1UL << (nr_bit_long - 1 - (val % nr_bit_long));
    }

    bool is_set(int cpu) const {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_cpus_max) {
            return false;
        }

        return !!(_cpu_list[cpu / nr_bit_long] & lshift(cpu));
    }

    void do_set(int cpu) {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_cpus_max) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] |= lshift(cpu);
    }

    void do_clr(int cpu) {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_cpus_max) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] &= ~lshift(cpu);
    }

    void print(double) const;
    void print(int, double, double, double, double) const;

    //
    // @cpulist must with the form: 1,2-4,6,8,9-10
    //
    void parse_cpu_list(const std::string &cpulist);

private:
    const std::string _ev_list = "PERF_COUNT_HW_CPU_CYCLES:U,PERF_COUNT_HW_CPU_CYCLES:K";
    #define U_CYCLE_EID 0  // subscript for event(usr cycle) in cpu's event group
    #define K_CYCLE_EID 1  // subscript for event(sys cycle) in cpu's event group
    #define N_EVENT_MAX 2

    unsigned long _cpu_list[nr_cpus_max / nr_bit_long];

    using cpu_data_t = std::tuple<int, int, group::ptr_t>;

    cpu_data_t *_cpu_data = nullptr;
    size_t _nr_select_cpu = 0;
    size_t _nr_total_cpu  = 0;

    #define cpu_num(cpu) std::get<0>(_cpu_data[(cpu)]) // processor's id
    #define cpu_mhz(cpu) std::get<1>(_cpu_data[(cpu)]) // processor's frequency in MHz (from /proc/cpuinfo)
    #define cpu_pmu(cpu) std::get<2>(_cpu_data[(cpu)]) // processor's PMU event (pointer to the ev-group binded to it)

    struct termios _termios;
    int _term_row = 25;
    int _term_col = 80;
};

} /* namespace perfm */
