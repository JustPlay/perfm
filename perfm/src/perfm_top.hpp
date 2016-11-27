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
    unsigned long lshift(unsigned long val) {
        return 1UL << (nr_bit_long - 1 - (val % nr_bit_long));
    }

    bool is_set(int cpu) {
        if (cpu < 0 || cpu >= nr_cpus_max) {
            return false;
        }

        return !!(_cpu_list[cpu / nr_bit_long] & lshift(cpu));
    }

    void do_set(int cpu) {
        if (cpu < 0 || cpu >= nr_cpus_max) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] |= lshift(cpu);
    }

    void do_clr(int cpu) {
        if (cpu < 0 || cpu >= nr_cpus_max) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] &= ~lshift(cpu);
    }

    void print(int, double, double, double) const;

    //
    // @cpulist must with the form: 1,2-4,6,8,9-10
    //
    void parse_cpu_list(const std::string &cpulist);

private:
    const std::string _ev_list = "PERF_COUNT_HW_CPU_CYCLES:U,PERF_COUNT_HW_CPU_CYCLES:K";

    unsigned long _cpu_list[nr_cpus_max / nr_bit_long];

    using cpu_data_t = std::tuple<int, int, group::ptr_t>;

    cpu_data_t *_cpu_data = nullptr;
    size_t _nr_select_cpu = 0;
    size_t _nr_total_cpu  = 0;

    #define cpu_id(cpu) std::get<0>(_cpu_data[(cpu)]) // processor id
    #define cpu_fr(cpu) std::get<1>(_cpu_data[(cpu)]) // processor frequency (from /proc/cpuinfo, MHz)
    #define cpu_ev(cpu) std::get<2>(_cpu_data[(cpu)]) // event group binded to this processor

    struct termios _termios;
    int _term_row = 25;
    int _term_col = 80;
};

} /* namespace perfm */
