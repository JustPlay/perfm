/**
 * perfm_monitor.hpp - interface for perfm monitor
 *
 */
#ifndef __PERFM_MONITOR_HPP__
#define __PERFM_MONITOR_HPP__

#include <vector>
#include <memory>

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

    ~monitor() { }

    void open();
    void close();

    void start();
    void stop();

private:
    monitor() {
        memset(_cpu_list, 0, sizeof(_cpu_list));
    }

    void rr();
    int loop();

    void parse_cpu_list(const std::string &list);

    unsigned long lshift(unsigned long val) const {
        return 1UL << (nr_bit_long - 1 - (val % nr_bit_long));
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

private:
    std::vector<group::ptr_t> _glist; /* event group list */

    unsigned long _cpu_list[nr_max_cpus / nr_bit_long];

    size_t _nr_select_cpu = 0;
    size_t _nr_total_cpu  = 0;

    using cpu_data_t = std::tuple<int, group::ptr_t>;

    cpu_data_t *_cpu_data = nullptr;

    #define cpu_num(cpu) std::get<0>(_cpu_data[(cpu)]) // processor's id
    #define cpu_pmu(cpu) std::get<1>(_cpu_data[(cpu)]) // processor's PMU event
};

} /* namespace perfm */

#endif /* __PERFM_MONITOR_HPP__ */
