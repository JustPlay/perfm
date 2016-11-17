/**
 * perfm_evgrp.hpp - interface for an event group which will be sched as a unit
 *
 */
#ifndef __PERFM_EVGRP_HPP__
#define __PERFM_EVGRP_HPP__

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <memory>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_event.hpp"

namespace perfm {

class evgrp_t {
public:
    using ptr_t = std::shared_ptr<evgrp_t>;

public:
    static ptr_t creat() {
        return ptr_t(new evgrp_t);
    }

    virtual ~evgrp_t() {
    
    }

private:
    evgrp_t() = default;

public:
    /**
     * gr_open - Encode & Open the event group for monitoring
     *
     * @ev_list  Event group list, within the form: "event1,event2,event3,..."  
     * @pid      Process/thread to minitor, -1 for any process/thread
     * @cpu      Processor to monitor; -1 for any processor
     * @plm      Privilege level mask string, used by libpfm4
     *
     * Return:
     *     file descriptor of the group leader, or -1 if error occurred 
     *
     * Description:
     */
    int gr_open(const std::string &ev_list, pid_t pid, int cpu = -1, const std::string &plm = "uk");
    int gr_open(const std::vector<std::string> &ev_argv, pid_t pid, int cpu = -1, const std::string &plm = "uk");

    int gr_close() {
        for (auto &evp : ev_list) {
            evp->ev_close();
        }

        return 0; /* FIXME (error handling) */
    }

    size_t gr_read();
    size_t gr_copy();
    
    int gr_start() {
        return ioctl(gr_leader()->ev_fd(), PERF_EVENT_IOC_ENABLE, 0);
    }

    int gr_stop() {
        return ioctl(gr_leader()->ev_fd(), PERF_EVENT_IOC_DISABLE, 0);
    }

    int gr_reset() {
        return ioctl(gr_leader()->ev_fd(), PERF_EVENT_IOC_RESET, 0);
    }

    size_t gr_size() const {
        return ev_list.size();
    }

    event_t::ptr_t gr_leader() const {
        if (ev_list.empty() || grp < 0 || static_cast<unsigned long>(grp) >= ev_list.size()) {
            return event_t::ptr_t();
        }

        return ev_list[grp];
    }

    void gr_print() const;

private:
    std::vector<event_t::ptr_t> ev_list; /* event list in this group
                                          * - An event group is scheduled onto the CPU as a unit, it will be put onto
                                          *   the CPU only if all of the events in the group can be put onto the CPU
                                          * - Events in the same group are measured simultaneously
                                          */

    int grp = 0;           /* group leader's subscipt in ev_list; for now it should always be 0 */
    int cpu = -1;          /* which CPU to monitor, -1 for any CPU */
    pid_t pid;             /* which process to monitor, -1 for any process */
    unsigned long flg = 0; /* flags used by perf_event_open() */
    unsigned long plm = 0; /* privilege level mask, this value should NOT be zero */
};

} /* namespace perfm */

#endif /* __PERFM_EVGRP_HPP__ */
