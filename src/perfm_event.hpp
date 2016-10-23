#ifndef __PERFM_EVENT_HPP__
#define __PERFM_EVENT_HPP__

#include <string>
#include <memory>
#include <tuple>
#include <cstring>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdint>

#include <perfmon/pfmlib_perf_event.h>

namespace perfm {

class group_t;
class event_t {
    friend class group_t;

public:
    using ptr_t     = std::shared_ptr<event_t>;
    using pmu_val_t = std::tuple<uint64_t, uint64_t, uint64_t>; /* 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING */

public:
    static ptr_t creat() {
        return ptr_t(new event_t);
    }

    virtual ~event_t() {
        
    }

    pmu_val_t get_pmu_val() const {
        return std::make_tuple(pmu_val_curr[0], pmu_val_curr[1], pmu_val_curr[2]);
    }

    void set_pmu_val(uint64_t pmu_cntr_val, uint64_t time_enabled, uint64_t time_running) {
        pmu_val_prev[0] = pmu_val_curr[0];
        pmu_val_prev[1] = pmu_val_curr[1];
        pmu_val_prev[2] = pmu_val_curr[2];

        pmu_val_curr[0] = pmu_cntr_val;
        pmu_val_curr[1] = time_enabled;
        pmu_val_curr[2] = time_running;
    }

    void set_pmu_val(const pmu_val_t &pmu_cntr_val) {
        set_pmu_val(std::get<0>(pmu_cntr_val), std::get<1>(pmu_cntr_val), std::get<2>(pmu_cntr_val));
    }

private:
    event_t() {
        memset(&this->pea, 0, sizeof(this->pea));
    }

public:
    /** 
     * ev_open - Encode & Open the event for monitoring
     *
     * @evn  Event name string  
     * @pid  Process/thread to minitor, -1 for any process/thread
     * @cpu  Processor to monitor; -1 for any processor
     * @plm  Privilege level mask; used by libpfm4
     * @flg  The flags argument for perf_event_open()
     *
     * Return:
     *     the new file descriptor, or -1 if an error occurred 
     *
     * Description:
     */
    int ev_open(const std::string &evn, pid_t pid, int grp = -1, int cpu = -1, unsigned long plm = PFM_PLM3 | PFM_PLM0, unsigned long flg = 0);

    /** 
     * ev_open - Call perf_event_open() to set up performance monitoring
     *
     * Return:
     *     the new file descriptor, or -1 if an error occurred 
     *
     * Description:
     *     this func must be called after 'this' had been fully inited, 
     *     e.g. called by 
     *            event_t::ev_open(const std::string &, ...)
     *          OR
     *            group_t::gr_open(...)
     */
    int ev_open();
    
    int ev_close() {
        int ret  = ::close(this->fd);
        this->fd = -1;

        return ret;
    }

    bool ev_read();
    bool ev_copy();

    void ev_print() const;

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
    uint64_t pmu_val_curr[3]; /* 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING */
    uint64_t pmu_val_prev[3]; /* 0: RAW PMU COUNT, 1: TIME_ENABLED, 2: TIME_RUNNING */ 

    struct perf_event_attr pea;

    int fd  = -1; /* file descriptor corresponds to the event that is
                   * measured; the ret val of perf_event_open()
                   */

    int grp = -1; /* the event group leader's fd; -1 for group leader
                   * arg for perf_event_open()
                   */

    int cpu = -1; /* which CPU to monitor; -1 for any CPU
                   * arg for perf_event_open()
                   */

    pid_t pid;    /* which process to monitor; -1 for any process
                   * arg for perf_event_open()
                   */

    unsigned long flg; /* arg for perf_event_open() */
    unsigned long plm;

    std::string nam; /* event string name */
};

} /* namespace perfm */

#endif /* __PERFM_EVENT_HPP__ */
