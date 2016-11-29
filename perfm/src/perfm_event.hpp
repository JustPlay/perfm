/**
 * perfm_event.hpp - interface for a *single* event/perf_event
 *
 */
#ifndef __PERFM_EVENT_HPP__
#define __PERFM_EVENT_HPP__

#include <cstdlib>
#include <cstring>
#include <cstdint>

#include <string>
#include <memory>
#include <tuple>
#include <new>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <perfmon/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

// -----------------------------------------------------------------------------------------------------
// | EventName(Intel)           | Cntr Type | EventName(intel)          | EventName(libpfm4)           |
// |---------------------------------------------------------------------------------------------------|
// | INST_RETIRED.ANY           | fixed (1) | INST_RETIRED.ANY_P        | INST_RETIRED:ANY_P           |
// |---------------------------------------------------------------------------------------------------|
// | CPU_CLK_UNHALTED.THREAD    | fixed (2) | CPU_CLK_UNHALTED.THREAD_P | CPU_CLK_UNHALTED:THREAD_P    |
// |---------------------------------------------------------------------------------------------------|
// | CPU_CLK_UNHALTED.REF_TSC   | fixed (3) | CPU_CLK_UNHALTED.REF_XCLK | CPU_CLK_UNHALTED:REF_XCLK    |
// |---------------------------------------------------------------------------------------------------|
// | 
// -----------------------------------------------------------------------------------------------------


/*
 * https://perf.wiki.kernel.org/index.php/Tutorial
 *
 * 1. If there are more events than counters, the kernel uses time 
 *    multiplexing (round-robin, switch frequency = HZ, generally 100 or 1000), to 
 *    give each event a chance to access the monitoring hardware.
 *    
 *    Multiplexing only applies to PMU events. With multiplexing, an event is 
 *    not measured all the time. At the end of the run, the tool scales the 
 *    count based on total time enabled vs time running. The actual formula is:
 *
 *    - final_count = raw_count * time_enabled / time_running
 *
 *    - TIME_RUNNING <= TIME_ENABLED
 *    - TIME_RUNNING != 0 
 *    - RAW_COUNT * TIME_ENABLED / TIME_RUNNING
 *
 *    This provides an __estimate__ of what the count would have been, had the
 *    event been measured during the entire run.
 *
 *    To avoid scaling (in the presence of only one active perf_event user), one
 *    can try and reduce the number of events.
 *
 *    PMU's generic counters can measure any events. 
 *          fixed counters can only measure one event. 
 *          some counters may be reserved for special purposes, such as a watchdog timer.
 *
 * 2. The read format *without* PERF_FORMAT_GROUP:
 *    struct {
 *        u64 val;
 *        u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *        u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 *    }
 *
 *    The read format *with* PERF_FORMAT_GROUP enabled:
 *    struct {
 *        u64 nr;
 *        u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *        u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 *        {
 *            u64 val;
 *            u64 id;  // PERF_FORMAT_ID 
 *        } cntr[nr];
 *    } // PERF_FORMAT_GROUP
 *
 * NOTE: perfm always enable PERF_FORMAT_TOTAL_TIME_ENABLED, PERF_FORMAT_TOTAL_TIME_RUNNING 
 *       perf_event_open(2)
 */

#ifdef  PEV_RDFMT_TIMEING
#undef  PEV_RDFMT_TIMEING
#endif
#define PEV_RDFMT_TIMEING (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

#ifdef  PEV_RDFMT_EVGROUP
#undef  PEV_RDFMT_EVGROUP
#endif
#define PEV_RDFMT_EVGROUP (PERF_FORMAT_GROUP)

namespace perfm {

class perf_event {

public:
    using ptr_t = std::shared_ptr<perf_event>;

public:
    static ptr_t alloc();
    
    virtual ~perf_event() { }

    int open();
    int close();

    int perf_fd() const {
        return this->_fd;
    }

    int cpu() const {
        return this->_cpu;
    }

    pid_t process() const {
        return this->_pid;
    }

    unsigned long mask() const {
        return this->_flags;
    }

    int leader() const {
        return this->_group_fd;
    }

    /**
     * attribute - get the attribute struct for this perf_event instance
     *
     * Return:
     *     return a pointer which point to the copy of @this->_hw
     *
     * Description:
     *     the returned pointer must be freed by the caller
     */
    struct perf_event_attr *attribute() const;

    void perf_fd(int fd) {
        this->_fd = fd;
    }

    void cpu(int cpu) {
        this->_cpu = cpu;
    } 

    void process(pid_t pid) {
        this->_pid = pid;
    }

    void mask(unsigned long flags) {
        this->_flags = flags;
    }

    void leader(int group_leader) {
        this->_group_fd = group_leader;
    } 

    void attribute(const struct perf_event_attr *hw) {
        memmove(&this->_hw, hw, sizeof(this->_hw));
    }

protected:
    perf_event() {
        memset(&this->_hw, 0, sizeof(this->_hw));
    }

private:
    /* file descriptor returned by perf_event_open() */
    int _fd = -1;

    /* parameters for perf_event_open() syscall */
    int _cpu = -1;
    int _group_fd = -1;
    pid_t _pid = -1;
    unsigned long _flags = 0UL;

    struct perf_event_attr _hw;
};

class event : public perf_event {

public:
    using ptr_t = std::shared_ptr<event>;
    using pmu_cntr_t = std::tuple<uint64_t, uint64_t, uint64_t>; /* 0: raw PMU count, 1: time_enabled, 2: time_running */

public:
    static ptr_t alloc();

    virtual ~event() { }

    using perf_event::open;

    /** 
     * open - encode & open the event for monitoring
     *
     * @evn  event name string  
     *
     * @pid  process/thread to minitor, -1 for any process/thread
     * @cpu  processor to monitor, -1 for any processor
     * @grp  group leader for this event
     * @flg  the flags argument for perf_event_open()
     *
     * @plm  privilege level mask, used by libpfm4
     *
     * Return:
     *     the new file descriptor, or -1 if an error occurred 
     *
     * Description:
     */
    int open(const std::string &evn, pid_t pid, int cpu = -1, int grp = -1, unsigned long flg = 0, unsigned long plm = PFM_PLM3 | PFM_PLM0);

    bool read();
    bool copy();

    uint64_t scale() const;
    uint64_t delta() const;
    double   ratio() const;

    int start() {
        return ::ioctl(this->perf_fd(), PERF_EVENT_IOC_ENABLE, 0);
    }

    int stop() {
        return ::ioctl(this->perf_fd(), PERF_EVENT_IOC_DISABLE, 0);
    }

    int reset() {
        return ::ioctl(this->perf_fd(), PERF_EVENT_IOC_RESET, 0);
    }

    int refresh() {
        return ::ioctl(this->perf_fd(), PERF_EVENT_IOC_REFRESH, 0);
    }

    void print() const;
    void prcfg() const;

    std::string name() const {
        return std::move(std::string(this->_nam));
    }

    unsigned long pl_mask() const {
        return this->_plm;
    }

    pmu_cntr_t pmu_cntr() const;

    void name(const std::string &nam) {
        this->_nam = nam;
    }

    void pl_mask(unsigned long plm) {
        this->_plm = plm;
    }

    void pmu_cntr(const pmu_cntr_t &val) {
       pmu_cntr(std::get<0>(val), std::get<1>(val), std::get<2>(val));
    }

    void pmu_cntr(uint64_t raw_val, uint64_t tim_ena, uint64_t tim_run);

private:
    uint64_t _pmu_vals[4] = { 0, 1, 1, 0 };   /* initialized with 0, 1, 1, 0 just to 
                                               * avoid the warning when do first read
                                               */

    #define raw_pmu_cntr() this->_pmu_vals[0] /* 0: raw PMU value */
    #define time_enabled() this->_pmu_vals[1] /* 1: time_enabled  */
    #define time_running() this->_pmu_vals[2] /* 2: time_running  */
    #define prev_pmu_val() this->_pmu_vals[3] /* 3: previous _scaled_ pmu value */

    std::string _nam;   /* event string name, used by libpfm4 */
    unsigned long _plm; /* privilege level mask, used by libpfm4 */
};

} /* namespace perfm */

#endif /* __PERFM_EVENT_HPP__ */
