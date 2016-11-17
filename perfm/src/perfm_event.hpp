/**
 * perfm_event.hpp - interface for a single event (perf_event)
 *
 */
#ifndef __PERFM_EVENT_HPP__
#define __PERFM_EVENT_HPP__

#include <cstdio>
#include <cstdlib>
#include <string>
#include <memory>
#include <tuple>
#include <cstring>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdint>

#include <perfmon/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

/*
 * Request timing information because event or/and PMU may be multiplexed
 * and thus it may not count all the time.
 * 
 * The scaling information will be used to scale the raw count
 * as if the event had run all along. The scale rules are list bellow:
 *
 * - TIME_RUNNING <= TIME_ENABLED
 * - TIME_RUNNING != 0 
 * - RAW_COUNT * TIME_ENABLED / TIME_RUNNING
 *
 * The read format *without* PERF_FORMAT_GROUP:
 * struct {
 *     u64 nr;
 *     u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *     u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 * }
 *
 * The read format *with* PERF_FORMAT_GROUP enabled:
 * struct {
 *     u64 nr;
 *     u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *     u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 *     {
 *         u64 value;
 *         u64 id;  // PERF_FORMAT_ID 
 *     } cntr[nr];
 * } // PERF_FORMAT_GROUP
 *
 * NOTE: perfm always enable PERF_FORMAT_TOTAL_TIME_ENABLED, PERF_FORMAT_TOTAL_TIME_RUNNING 
 *       perf_event_open(2)
 */

#ifdef  PEV_RDFMT_TIMEING
#undef  PEV_RDFMT_TIMEING
#endif
#define PEV_RDFMT_TIMEING (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

#define PEV_RDFMT_EVGROUP (PERF_FORMAT_GROUP)

namespace perfm {

class evgrp_t;
class event_t {
    friend class evgrp_t;

public:
    using ptr_t      = std::shared_ptr<event_t>;
    using pmu_cntr_t = std::tuple<uint64_t, uint64_t, uint64_t>; // 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING

public:
    static ptr_t creat() {
        return ptr_t(new event_t);
    }

    virtual ~event_t() {
        
    }

    pmu_cntr_t get_pmu_cntr() const {
        return std::make_tuple(pmu_curr[0], pmu_curr[1], pmu_curr[2]);
    }

    void set_pmu_cntr(uint64_t pmu_cntr_val, uint64_t time_enabled, uint64_t time_running) {
        pmu_prev[0] = pmu_curr[0];
        pmu_prev[1] = pmu_curr[1];
        pmu_prev[2] = pmu_curr[2];

        pmu_curr[0] = pmu_cntr_val;
        pmu_curr[1] = time_enabled;
        pmu_curr[2] = time_running;
    }

    void set_pmu_cntr(const pmu_cntr_t &pmu_cntr) {
        set_pmu_cntr(std::get<0>(pmu_cntr), std::get<1>(pmu_cntr), std::get<2>(pmu_cntr));
    }

private:
    event_t() {
        memset(&this->pea, 0, sizeof(this->pea));
    }

public:
    /** 
     * ev_open - encode & open the event for monitoring
     *
     * @evn  event name string  
     * @pid  process/thread to minitor, -1 for any process/thread
     * @grp  group leader for this event
     * @cpu  processor to monitor, -1 for any processor
     * @plm  privilege level mask, used by libpfm4
     * @flg  the flags argument for perf_event_open()
     *
     * Return:
     *     the new file descriptor, or -1 if an error occurred 
     *
     * Description:
     */
    int ev_open(const std::string &evn, pid_t pid, int grp = -1, int cpu = -1, unsigned long plm = PFM_PLM3 | PFM_PLM0, unsigned long flg = 0);

    /** 
     * ev_open - call perf_event_open() to set up performance monitoring
     *
     * Return:
     *     the new file descriptor, or -1 if an error occurred 
     *
     * Description:
     *     this func must be called after 'this' had been fully inited, 
     *     e.g. called by 
     *            event_t::ev_open(const std::string &, ...)
     *          OR
     *            evgrp_t::gr_open(...)
     */
    int ev_open();
    int ev_close();

    bool ev_read();
    bool ev_copy();

    uint64_t ev_scale() const;
    uint64_t ev_delta() const;

    void ev_print() const;
    void ev_prcfg() const;

    std::string ev_nam() const {
        return std::move(std::string(this->nam));
    }

    int ev_fd() const {
        return this->fd;
    }

    int ev_cpu() const {
        return this->cpu;
    }

    pid_t ev_pid() const {
        return this->pid;
    }

    int ev_grp() const {
        return this->grp;
    }

    int ev_start() {
        return ::ioctl(this->fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    int ev_stop() {
        return ::ioctl(this->fd, PERF_EVENT_IOC_DISABLE, 0);
    }

    int ev_reset() {
        return ::ioctl(this->fd, PERF_EVENT_IOC_RESET, 0);
    }

    int ev_refresh() {
        return ::ioctl(this->fd, PERF_EVENT_IOC_REFRESH, 0);
    }

private:
    uint64_t pmu_curr[3]; // 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING
    uint64_t pmu_prev[3]; // 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING 

    struct perf_event_attr pea;

    int fd  = -1; // file descriptor corresponds to the event that is measured,
                  // the ret val of perf_event_open(2)

    int grp = -1; // the event group leader's fd, -1 for group leader,
                  // arg for perf_event_open(2)

    int cpu = -1; // which CPU to monitor, -1 for any CPU,
                  // arg for perf_event_open(2)

    pid_t pid;    // which process to monitor, -1 for any process,
                  // arg for perf_event_open(2)

    unsigned long flg; // arg for perf_event_open(2)

    std::string nam;   // event string name, used by libpfm4
    unsigned long plm; // privilege level mask, used by libpfm4
};

void ev2perf(const std::string &evn, FILE *fp = stdout);

} /* namespace perfm */

#endif /* __PERFM_EVENT_HPP__ */
