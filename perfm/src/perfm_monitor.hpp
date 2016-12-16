/**
 * perfm_monitor.hpp - interface for perfm monitor
 *
 */
#ifndef __PERFM_MONITOR_HPP__
#define __PERFM_MONITOR_HPP__

#include <vector>
#include <memory>
#include <cstring>

#include "perfm_group.hpp"

namespace perfm {

class monitor {

public:
    using ptr_t = std::shared_ptr<monitor>;

public:
    static constexpr size_t nr_max_cpus = 512;
    static constexpr size_t nr_bit_long = sizeof(unsigned long) << 3;

public:
    static ptr_t alloc();

    ~monitor();

    void init();
    void open();
    void close();

    void start();
    void stop();

private:
    monitor() {
        memset(_cpu_list, 0, sizeof(_cpu_list));
    }

    void rr(double = 0.1);
    int loop();

    void parse_cpu_list(const std::string &list);

    static unsigned long lshift(unsigned long v) {
        return 1UL << (nr_bit_long - 1 - (v % nr_bit_long));
    }

    bool is_set(int cpu) const {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_max_cpus) {
            return false;
        }

        return !!(_cpu_list[cpu / nr_bit_long] & lshift(cpu));
    }

    void do_set(int cpu) {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_max_cpus) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] |= lshift(cpu);
    }

    void do_clr(int cpu) {
        if (cpu < 0 || static_cast<size_t>(cpu) >= nr_max_cpus) {
            return;
        }

        _cpu_list[cpu / nr_bit_long] &= ~lshift(cpu);
    }

    void print(size_t group, uint64_t tsc_cycles) const;

private:
    using _cpu_data_t = std::vector<group::ptr_t>;
    using _ev_group_t = std::vector<std::string>;

    unsigned long _cpu_list[nr_max_cpus / nr_bit_long];
    _cpu_data_t  *_cpu_data = nullptr;
    _ev_group_t  *_ev_group = nullptr;

    size_t _nr_select_cpu = 0;
    size_t _nr_total_cpu  = 0;
};

} /* namespace perfm */

#endif /* __PERFM_MONITOR_HPP__ */
