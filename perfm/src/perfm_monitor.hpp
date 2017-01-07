/**
 * perfm_monitor.hpp - interface for perfm monitor
 *
 */
#ifndef __PERFM_MONITOR_HPP__
#define __PERFM_MONITOR_HPP__

#include <vector>
#include <memory>
#include <cstring>

#include "perfm_config.hpp"
#include "perfm_group.hpp"

namespace perfm {

template<std::size_t N> class bitmap final {

public:
    bool is_set(long bit) const
    {
        if (bit < 0 || static_cast<size_t>(bit) >= N) {
            return false;
        }

        return !!(_bits_array[bit / _NR_BIT_LONG] & lshift(bit));
    }

    void do_set(long bit)
    {
        if (bit < 0 || static_cast<size_t>(bit) >= N) {
            return;
        }

        _bits_array[bit / _NR_BIT_LONG] |= lshift(bit);
    }

    void do_clr(long bit)
    {
        if (bit < 0 || static_cast<size_t>(bit) >= N) {
            return;
        }

        _bits_array[bit / _NR_BIT_LONG] &= ~lshift(bit);
    }

public:
    bitmap() { }
    ~bitmap() { }

    bitmap(const bitmap &) = delete;
    bitmap(bitmap &&) = delete;

private:
    static unsigned long lshift(unsigned long v) {
        return 1UL << (_NR_BIT_LONG - 1 - (v % _NR_BIT_LONG));
    }

private:
    unsigned long _bits_array[(N / _NR_BIT_LONG) + 1] = { 0 };

private:
    static constexpr std::size_t _NR_BIT_LONG = sizeof(unsigned long) << 3; 
};

class monitor {

public:
    using ptr_t = std::shared_ptr<monitor>;

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

    /*
     * rr - Round-robin scheduling
     *
     * @time_quantum: time slice (in seconds) for ...
     *
     * Description:
     *     rr will ...
     */
    void rr(double time_quantum = 0.1);
    int loop();

    void parse_cpu_list(const std::string &list);

    static unsigned long lshift(unsigned long v) {
        return 1UL << (NR_BIT_PER_LONG - 1 - (v % NR_BIT_PER_LONG));
    }

    bool is_set(int pos) const;
    void do_set(int pos);
    void do_clr(int pos);

    void print(size_t group, uint64_t tsc_cycles) const;

private:
    /* a set of "event group" associated to a processor or a socket
     * 
     * event groups on the same processor/socket will be scheduled under the "round-robin" scheduling policy
     *
     * events in the same group will be scheduled as a unit
     */
    using _pmu_dat_t = std::vector<group::ptr_t>;

    /* an event group, which will be scheduled as a unit
     * 
     * event group was represented by a vector which will contain a list of events
     * belong to this group. the first event (with sub-script 0) in this group will be
     * the group leader
     */
    using _e_group_t = std::vector<std::string>;

    unsigned long _cpu_list[NR_MAX_PROCESSOR / NR_BIT_PER_LONG];

    _pmu_dat_t *_cpu_data = nullptr;
    _pmu_dat_t *_skt_data = nullptr;
    _e_group_t *_ev_group = nullptr;

    size_t _nr_select_cpu = 0; /* # of selected cpus */
    size_t _nr_usable_cpu = 0; /* # of presented cpus */
};

inline bool monitor::is_set(int pos) const
{
    if (pos < 0 || static_cast<size_t>(pos) >= NR_MAX_PROCESSOR) {
        return false;
    }

    return !!(_cpu_list[pos / NR_BIT_PER_LONG] & lshift(pos));
}

inline void monitor::do_set(int pos)
{
    if (pos < 0 || static_cast<size_t>(pos) >= NR_MAX_PROCESSOR) {
        return;
    }

    _cpu_list[pos / NR_BIT_PER_LONG] |= lshift(pos);
}

inline void monitor::do_clr(int pos)
{
    if (pos < 0 || static_cast<size_t>(pos) >= NR_MAX_PROCESSOR) {
        return;
    }

    _cpu_list[pos / NR_BIT_PER_LONG] &= ~lshift(pos);
}

} /* namespace perfm */

#endif /* __PERFM_MONITOR_HPP__ */
