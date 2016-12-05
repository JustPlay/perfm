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

// GCC 4.3 now errors out on certain situations in C++ where a given name may refer to more than one type or function.
// WITHIN A SCOPE, A GIVEN NAME CAN REFER TO ONLY TYPE OR FUNCTION.
// For example, in the following code, the name foo changes meaning:
//
// class foo {};
// class bar {
// public:
//     void do_silly(foo) {}
//     void foo() {}
// };
//
// Note that on line 5, foo refers to class foo, whereas on line 6 and after, foo refers to void foo(). 
// This isn't allowed because it changes the meaning of foo. 
// The reason that this isn't allowed in C++ is because IF in the definition of bar we write foo(), 
// it is ambiguous whether we want to instantiate an object of type class foo or call this->foo().
//
// Note that this can also happen with two different types. The solution in either case is either to
// move one of the names such that it is not in scope, or to rename one of the names. Note that in the
// case with the function, it is usually easier to rename the function. If you are a library developer,
// please do not forget to bump the SONAME if you change the ABI.

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
     * @cpu    processor to monitor, -1 for any processor
     * @flags  flags used by perf_event_open(2), defaults to 0
     *
     * Return:
     *     file descriptor of the group leader, or -1 if error occurred 
     *
     * Description:
     *     A pid > 0 and cpu == -1 setting measures per-process and follows that process to whatever CPU 
     *     the process gets scheduled to. Per-process events can be created by any user.
     *
     *     A pid == -1 and cpu >= 0 setting is per-CPU and measures all processes on the specified CPU.
     *     Per-CPU events need the CAP_SYS_ADMIN capability or a /proc/sys/kernel/perf_event_paranoid value of less than 1.
     *
     *     Note that the combination of pid == -1 and cpu == -1 is not valid.
     *
     *     An event group is scheduled onto the CPU as a unit: it will only be put onto the CPU if all of the events in 
     *     the group can be put onto the CPU. This means that the values of the member events can be meaningfully 
     *     compared, added, divided (to get ratios), etc., with each other, since they have counted events for the 
     *     same set of executed instructions.
     */
    int open(const std::string &elist, pid_t pid, int cpu, unsigned long flags = 0UL);
    int open(const std::vector<std::string> &eargv, pid_t pid, int cpu, unsigned long flags = 0UL);

    int close();

    bool read();
    bool copy();
    
    // there exists three use case list below:
    // 1. start -> read -> stop -> start -> read -> stop ...
    // 2. start -> stop -> read -> start -> stop -> read ...
    // 3. start -> read -> read -> read -> read -> ...
    // we need to ensure this event group do counting all the time in period of [_tsc_prev, _tsc_curr]
    bool start() {
        _tsc_prev = _tsc_curr = read_tsc();
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_ENABLE, 0) == 0;
    }

    bool stop() {
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_DISABLE, 0) == 0;
    }

    bool reset() {
        return ioctl(leader()->perf_fd(), PERF_EVENT_IOC_RESET, 0) == 0;
    }

    size_t size() const {
        return _elist.size();
    }

    size_t nr_event() const {
        return _elist.size();
    }

    event::ptr_t leader() const {
        if (_elist.empty() || _grp >= _elist.size()) {
            return event::ptr_t();
        }

        return _elist[_grp];
    }

    void print() const;

    std::vector<event::ptr_t> elist() const {
        return _elist;
    }

    event::ptr_t fetch_event(size_t id) {
        return id < _elist.size() ? _elist[id] : nullptr;
    }

    uint64_t tsc_value() const {
        return _tsc_curr;
    }
    
    uint64_t tsc_delta() const {
        return _tsc_curr - _tsc_prev;
    }

private:
    std::vector<event::ptr_t> _elist; /* event list in this group
                                       * - an event group is scheduled onto the CPU as a unit, it will be put onto
                                       *   the CPU only if all of the events in the group can be put onto the CPU
                                       * - events in the same group are measured simultaneously
                                       */

    size_t _grp = 0;          /* group leader's subscipt in _elist; for now it should always be 0 */
    int _cpu = -1;            /* which CPU to monitor, -1 for any CPU */
    pid_t _pid;               /* which process to monitor, -1 for any process */
    unsigned long _flags = 0; /* flags used by perf_event_open(2) */

    uint64_t _tsc_prev;
    uint64_t _tsc_curr;
};

} /* namespace perfm */

#endif /* __PERFM_GROUP_HPP__ */
