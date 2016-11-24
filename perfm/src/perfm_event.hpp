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

/*
 * 1. Request timing information because event or/and pmu may be multiplexed and 
 *    thus it may not count all the time.
 * 
 *    The scaling information will be used to scale the raw count as if the event 
 *    had run all along.
 *
 *    The scale rules are list bellow:
 *    - TIME_RUNNING <= TIME_ENABLED
 *    - TIME_RUNNING != 0 
 *    - RAW_COUNT * TIME_ENABLED / TIME_RUNNING
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
    using pmu_cntr_t = std::tuple<uint64_t, uint64_t, uint64_t>; /* 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING */

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

    pmu_cntr_t pmu_cntr() const {
        return std::make_tuple(_pmu_curr[0], _pmu_curr[1], _pmu_curr[2]);
    }

    void name(const std::string &nam) {
        this->_nam = nam;
    }

    void pl_mask(unsigned long plm) {
        this->_plm = plm;
    }

    void pmu_cntr(const pmu_cntr_t &pmu) {
       pmu_cntr(std::get<0>(pmu), std::get<1>(pmu), std::get<2>(pmu));
    }

    void pmu_cntr(uint64_t pmu, uint64_t time_enabled, uint64_t time_running) {
        _pmu_prev[0] = _pmu_curr[0];
        _pmu_prev[1] = _pmu_curr[1];
        _pmu_prev[2] = _pmu_curr[2];

        _pmu_curr[0] = pmu;
        _pmu_curr[1] = time_enabled;
        _pmu_curr[2] = time_running;
    }

private:
    uint64_t _pmu_curr[3]; // 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING
    uint64_t _pmu_prev[3]; // 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING 

    std::string _nam;   /* event string name, used by libpfm4 */
    unsigned long _plm; /* privilege level mask, used by libpfm4 */
};

} /* namespace perfm */

#endif /* __PERFM_EVENT_HPP__ */
