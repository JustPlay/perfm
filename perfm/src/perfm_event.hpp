/**
 * perfm_event.hpp - interface for a _single_ event/perf_event
 *
 */
#ifndef __PERFM_EVENT_HPP__
#define __PERFM_EVENT_HPP__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

#include "linux/perf_event.h"

/*
 * https://perf.wiki.kernel.org/index.php/Tutorial
 *
 * 1. If there are more events than counters, the kernel uses time 
 *    multiplexing (round-robin, switch frequency = HZ, generally 100 or 1000), to 
 *    give each event a chance to access the monitoring hardware.
 *    
 *    Multiplexing only applies to PMU events. With multiplexing, an event is 
 *    not measured all the time. At the end of the run, we need scale the 
 *    count based on total time enabled vs time running. The actual formula is:
 *
 *    - final_count = raw_count * time_enabled / time_running
 *
 *    - time_running <= time_enabled
 *    - time_running != 0 
 *    - raw_count * time_enabled / time_running
 *
 *    This provides an __estimate__ of what the count would have been, had the
 *    event been measured during the entire run. So, we should avoid multiplexing
 *    is possible
 *
 *    To avoid scaling (in the presence of only one active perf_event user), one
 *    can try and reduce the number of events.
 *
 *    PMU's generic counters can measure any events. 
 *          fixed counters can only measure one event (not programable). 
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
 * NOTE: perfm always enable PERF_FORMAT_TOTAL_TIME_ENABLED & PERF_FORMAT_TOTAL_TIME_RUNNING 
 *       for more info please refer to perf_event_open(2)
 */

#ifdef  RDFMT_TIMEING
#undef  RDFMT_TIMEING
#endif
#define RDFMT_TIMEING (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

#ifdef  RDFMT_EVGROUP
#undef  RDFMT_EVGROUP
#endif
#define RDFMT_EVGROUP (PERF_FORMAT_GROUP)

namespace perfm {

/**
 * descriptor - the descriptor for a single perf_event
 *
 * Description:
 *     this class contain arguments & the returned fd for perf_event_open(2), and someother things
 *
 * TODO:
 *     we need more ...
 */
class descriptor {

public:
    using ptr_t = std::shared_ptr<descriptor>;

public:
    static ptr_t alloc();
    
    virtual ~descriptor() { }

    bool open();
    bool close();

    int fd() const {
        return _fd;
    }

    int cpu() const {
        return _cpu;
    }

    pid_t process() const {
        return _pid;
    }

    unsigned long flag() const {
        return _flags;
    }

    int leader() const {
        return _group_fd;
    }

    /**
     * attr - get the attribute struct for this perf_event instance
     *
     * Return:
     *     return a pointer which point to the copy of @_hw
     *
     * Description:
     *     the returned pointer must be freed by the caller
     */
    struct perf_event_attr *attr() const;

    void fd(int fd) {
        _fd = fd;
    }

    void cpu(int cpu) {
        _cpu = cpu;
    } 

    void process(pid_t pid) {
        _pid = pid;
    }

    void flag(unsigned long flags) {
        _flags = flags;
    }

    void leader(int group_leader) {
        _group_fd = group_leader;
    } 

    void attr(const struct perf_event_attr *hw);

protected:
    descriptor () {
        memset(&_hw, 0, sizeof(_hw));
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

    /* FIXME: some otherthings unused for now */
    struct perf_event_mmap_page *_page = nullptr;
};

/**
 * event - TODO
 *
 */
class event : public descriptor {

public:
    using ptr_t  = std::shared_ptr<event>;
    using cntr_t = std::tuple<uint64_t, uint64_t, uint64_t>; /* 0: raw pmu count, 1: time_enabled, 2: time_running */

public:
    static ptr_t alloc();

    virtual ~event() { }

    using descriptor::open;

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

    bool start() {
        return ::ioctl(this->fd(), PERF_EVENT_IOC_ENABLE, 0);
    }

    bool stop() {
        return ::ioctl(this->fd(), PERF_EVENT_IOC_DISABLE, 0);
    }

    bool reset() {
        return ::ioctl(this->fd(), PERF_EVENT_IOC_RESET, 0);
    }

    bool refresh() {
        return ::ioctl(this->fd(), PERF_EVENT_IOC_REFRESH, 0);
    }

    void print() const;
    void prcfg() const;

    std::string name() const {
        return std::move(std::string(this->_nam));
    }

    unsigned long pl_mask() const {
        return this->_plm;
    }

    cntr_t pmu_cntr() const;

    void name(const std::string &nam) {
        this->_nam = nam;
    }

    void pl_mask(unsigned long plm) {
        this->_plm = plm;
    }

    void pmu_cntr(const cntr_t &val) {
       pmu_cntr(std::get<0>(val), std::get<1>(val), std::get<2>(val));
    }

    void pmu_cntr(uint64_t raw_val, uint64_t tim_ena, uint64_t tim_run);

private:
    uint64_t _pmu_vals[4] = { 0, 1, 1, 0 };   /* initialized with 0, 1, 1, 0 just to 
                                               * avoid the warning when do first read/scale
                                               */

    #define _m_raw_pmu() this->_pmu_vals[0]  /* 0: raw pmu value */
    #define _m_enabled() this->_pmu_vals[1]  /* 1: time_enabled  */
    #define _m_running() this->_pmu_vals[2]  /* 2: time_running  */
    #define _m_initial() this->_pmu_vals[3]  /* 3: previous or initial _scaled_ pmu value */

    std::string _nam;   /* event string name, used by libpfm4 */
    unsigned long _plm; /* privilege level mask, used by libpfm4 */
};

inline bool event::start()
{
    int ret = ::ioctl(this->fd(), PERF_EVENT_IOC_ENABLE, 0);
#ifndef NDEBUG
    if (ret == -1) {
        perfm_warn("failed to start perf_event %d\n", this->fd());
    }
#endif
    return !!ret;
}

inline bool event::stop()
{
    int ret = ::ioctl(this->fd(), PERF_EVENT_IOC_DISABLE, 0);
#ifndef NDEBUG
    if (ret == -1) {
        perfm_warn("failed to stop perf_event %d\n", this->fd());
    }
#endif
    return !!ret;
}

inline bool event::reset()
{
    int ret = ::ioctl(this->fd(), PERF_EVENT_IOC_RESET, 0);
#ifndef NDEBUG
    if (ret == -1) {
        perfm_warn("failed to reset perf_event %d\n", this->fd());
    }
#endif
    return !!ret;
}

inline bool event::refresh()
{
    int ret = ::ioctl(this->fd(), PERF_EVENT_IOC_REFRESH, 0);
#ifndef NDEBUG
    if (ret == -1) {
        perfm_warn("failed to refresh perf_event %d\n", this->fd());
    }
#endif
    return !!ret;
}

} /* namespace perfm */

#endif /* __PERFM_EVENT_HPP__ */
