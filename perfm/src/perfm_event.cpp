#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

namespace {

/* 
 * the below definition must be consistent with `perf_type_id` defined in <linux/perf_event.h> 
 */

#define PERF_MAX PERF_TYPE_MAX

const char *pmu_type_list[PERF_MAX] = {
    "PERF_HARDWARE",
    "PERF_SOFTWARE",
    "PERF_TRACEPOINT",
    "PERF_HW_CACHE",
    "PERF_RAW",
    "PERF_BREAKPOINT",
};

} /* namespace */

namespace perfm {

descriptor::ptr_t descriptor::alloc()
{
    descriptor *d = nullptr;
    
    try {
        d = new descriptor;
    } catch (const std::bad_alloc &) {
        d = nullptr; 
    }

    return ptr_t(d);
}

bool descriptor::open(const struct perf_event_attr *hw, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    if (!hw || (pid == -1 && cpu == -1)) {
        return false;
    }

    this->cpu(cpu);
    this->process(pid);
    this->leader(group_fd);    
    this->attr(hw);
    this->flag(flags);

    return open();
}

bool descriptor::open()
{
    _fd = ::perf_event_open(&_hw, _pid, _cpu, _group_fd, _flags);

    if (_fd == -1) {
        perfm_warn("perf_event_open() %s\n", strerror_r(errno, NULL, 0));
    }

    return _fd != -1;
}

bool descriptor::close()
{
    if (_fd == -1) {
        return true;
    }

    int err = ::close(_fd);
    if (!err) {
        _fd = -1;
    } else {
        perfm_warn("failed to close perf_event %d\n", _fd);
    }

    return !err;
}

struct perf_event_attr *descriptor::attr() const
{
    struct perf_event_attr *hw_copy = nullptr;
    try {
        hw_copy = new perf_event_attr;
    } catch (const std::bad_alloc &) {
        hw_copy = nullptr;
    }
    
    if (hw_copy) {
        memmove(hw_copy, &_hw, sizeof(struct perf_event_attr));
    }

    return hw_copy;
}

void descripter::attr(const struct perf_event_attr *hw)
{
    if (!hw) {
        perfm_warn("argument invalid, attr unchanged\n");
    }

    memmove(&_hw, hw, sizeof(struct perf_event_attr));
}

event::ptr_t event::alloc()
{
    event *e = nullptr;

    try {
        e = new event;
    } catch (const std::bad_alloc &) {
        e = nullptr;
    }
    
    return ptr_t(e);
}

int event::open(const std::string &evn, pid_t pid, int cpu, int grp, unsigned long flg, unsigned long plm)
{
    _plm = plm;
    _nam = evn;

    /* 
     * FIXME:
     *    remove libpfm4
     */
    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));

    struct perf_event_attr hw;
    memset(&hw, 0, sizeof(hw));

    arg.attr = &hw;
    arg.size = sizeof(arg);

    pfm_err_t ret = pfm_get_os_event_encoding(this->name().c_str(), this->pl_mask(), PFM_OS_PERF_EVENT, &arg);
    if (ret != PFM_SUCCESS) {
        perfm_warn("%s %s\n", this->name().c_str(), pfm_strerror(ret));
        return -1;
    }

    // disabled = 1 for group leader, 0 for group member
    hw.disabled = -1 == this->leader() ? 1 : 0;

    // include timing information for scaling
    if (perfm_options.rdfmt_timeing) {
        hw.read_format |= RDFMT_TIMEING;
    }
    
    // PERF_FORMAT_GROUP
    if (perfm_options.rdfmt_evgroup && this->leader() == -1) {
        hw.read_format |= RDFMT_EVGROUP;
    }

    // count events of child tasks as well as the task specified
    if (perfm_options.incl_children) {
        hw.inherit = 1;
    } 

    /* TODO (we need more setting) */

    return open(hw, pid, cpu, grp, flg);
}

bool event::read()
{
    /*
     * FIXME:
     *     how to handle the previous pmu value
     */
    bool err = false;

    _m_initial() = scale();

    ssize_t nr = ::read(fd(), _pmu_vals, 3 * sizeof(uint64_t));
    if (nr != 3 * sizeof(uint64_t)) {
        perfm_warn("read pmu counters failed %s\n", strerror_r(errno, NULL, 0));
        err = true;
    }

    return !err;
}

double event::ratio() const
{
    if (_m_running() > _m_enabled()) {
        perfm_warn("time_running (%zu) > time_enabled (%zu)\n", _m_running(), _m_enabled());
    }

    if (_m_enabled() == 0) {
        return 0;
    } 

    return 1.0 * _m_running() / _m_enabled(); /* time_running / time_enabled */
}

uint64_t event::scale() const
{
    if (_m_running() == _m_enabled()) {
        return _m_raw_pmu();
    }

    uint64_t res = 0;

    if (_m_running() > _m_enabled()) {
        perfm_warn("time_running (%zu) > time_enabled (%zu)\n", _m_running(), _m_enabled());
    }

    if (_m_running() == 0 && _m_raw_pmu() != 0) {
        perfm_warn("time_running is 0, scaling failed. %zu, %zu, %zu.\n", _m_raw_pmu(), _m_enabled(), _m_running());
    }

    if (_m_running() != 0) {
        res = static_cast<uint64_t>(1.0 * _m_raw_pmu() * _m_enabled() / _m_running());
    }
    
    return res;
}

uint64_t event::delta() const
{
    return scale() - _m_initial(); /* current scaled value - previous/initial scaled value */
}

event::cntr_t event::pmu_cntr() const
{
    return std::make_tuple(_m_raw_pmu(), _m_enabled(), _m_running()); 
}

void event::pmu_cntr(uint64_t raw_val, uint64_t tim_ena, uint64_t tim_run)
{
    _m_initial() = scale();

    _m_raw_pmu() = raw_val;
    _m_enabled() = tim_ena;
    _m_running() = tim_run;
}

void event::print() const
{
    FILE *fp = perfm_options.fp_out;
    if (!fp) {
        fp = stdout;
    }

    #define DELIMITER "  "

    perfm_fatal("TODO\n");
}

void event::prcfg() const
{
    FILE *fp = stdout;
    
    // event's name
    fprintf(fp, "- %s -\n", raw_name().c_str());

    // event's pid,cpu,group_leader,...
    fprintf(fp, "  event.pid/process            : %2d\n"
                "       .cpu/processor          : %2d\n"
                "       .group_leader/fd        : %2d\n"
                "       .flags                  : %2lu\n"
                "\n",
                process(),
                cpu(),
                leader(),
                flag()
           );

    // event's perf attr
    struct perf_event_attr *hw = attr();

    fprintf(fp, "  perf.type                    : %s\n"
                "  perf.config                  : %zx\n"
                "      .config1                 : %zx\n"
                "      .config2                 : %zx\n"
                "  perf.disabled                : %s\n"
                "      .inherit                 : %s\n"
                "      .pinned                  : %s\n"
                "      .exclusive               : %s\n"
                "      .exclude_user            : %s\n"
                "      .exclude_kernel          : %s\n"
                "      .exclude_hv              : %s\n"
                "      .exclude_idle            : %s\n"
                "      .inherit_stat            : %s\n"
                "      .task                    : %s\n"
                "      .exclude_callchain_kernel: %s\n"
                "      .exclude_callchain_user  : %s\n"
                "      .use_clockid             : %s\n"
                "\n",
                pmu_type_list[hw->type],
                hw->config,
                hw->config1,
                hw->config2,
                hw->disabled                 ? "true" : "false",
                hw->inherit                  ? "true" : "false",
                hw->pinned                   ? "true" : "false",
                hw->exclusive                ? "true" : "false",
                hw->exclude_user             ? "true" : "false",
                hw->exclude_kernel           ? "true" : "false",
                hw->exclude_hv               ? "true" : "false",
                hw->exclude_idle             ? "true" : "false",
                hw->inherit_stat             ? "true" : "false",
                hw->task                     ? "true" : "false",
                hw->exclude_callchain_kernel ? "true" : "false",
                hw->exclude_callchain_user   ? "true" : "false",
                hw->use_clockid              ? "true" : "false"
            );

    free(hw);
}

} /* namespace perfm */
