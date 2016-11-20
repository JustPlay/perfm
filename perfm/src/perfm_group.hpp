/**
 * perfm_group.hpp - interface for an event group which will be sched as a unit
 *
 */
#ifndef __PERFM_GROUP_HPP__
#define __PERFM_GROUP_HPP__

#include <vector>
#include <string>
#include <memory>

#include <sys/types.h>
#include <sys/ioctl.h>

#include "perfm_event.hpp"

namespace perfm {

class group {
public:
    using ptr_t = std::shared_ptr<group>;

public:
    static ptr_t alloc();

    virtual ~group() { }

private:
    group() = default;

public:
    /**
     * open - encode & open the event group for monitoring
     *
     * @elist  event group list, within the form: "event1,event2,event3,..."  
     * @pid    process/thread to minitor, -1 for any process/thread
     * @cpu    processor to monitor; -1 for any processor
     * @plm    privilege level mask string, used by libpfm4
     *
     * Return:
     *     file descriptor of the group leader, or -1 if error occurred 
     *
     * Description:
     */
    int open(const std::string &elist, pid_t pid, int cpu = -1, const std::string &plm = "uk");
    int open(const std::vector<std::string> &eargv, pid_t pid, int cpu = -1, const std::string &plm = "uk");

    int close();

    size_t read();
    size_t copy();
    
    int start() {
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_ENABLE, 0);
    }

    int stop() {
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_DISABLE, 0);
    }

    int reset() {
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_RESET, 0);
    }

    size_t size() const {
        return _elist.size();
    }

    event::ptr_t leader() const {
        if (_elist.empty() || _grp < 0 || static_cast<size_t>(_grp) >= _elist.size()) {
            return event::ptr_t();
        }

        return _elist[_grp];
    }

    void print() const;

private:
    std::vector<event::ptr_t> _elist; /* event list in this group
                                       * - an event group is scheduled onto the CPU as a unit, it will be put onto
                                       *   the CPU only if all of the events in the group can be put onto the CPU
                                       * - events in the same group are measured simultaneously
                                       */

    int _grp = 0;           /* group leader's subscipt in _elist; for now it should always be 0 */
    int _cpu = -1;          /* which CPU to monitor, -1 for any CPU */
    pid_t _pid;             /* which process to monitor, -1 for any process */
    unsigned long _flg = 0; /* flags used by perf_event_open() */
    unsigned long _plm = 0; /* privilege level mask, this value should NOT be zero */
};

} /* namespace perfm */

#endif /* __PERFM_GROUP_HPP__ */
