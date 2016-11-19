#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

namespace {

/* 
 * this definition must be consistent with the 'perf_type_id' defined in
 *     <linux/perf_event.h> 
 * or
 *     <perfmon/perf_event.h>
 */

#define PERF_MAX PERF_TYPE_MAX

const char *ev_type[PERF_MAX] = {
    "PERF_HARDWARE",
    "PERF_SOFTWARE",
    "PERF_TRACEPOINT",
    "PERF_HW_CACHE",
    "PERF_RAW",
    "PERF_BREAKPOINT",
};

} /* namesapce */

namespace perfm {

perf_event::ptr_t perf_event::alloc()
{
    perf_event *event = nullptr;
    
    try {
        event = new perf_event;
    } catch (const std::bad_alloc &e) {
        perfm_warn("%s\n", e.what());
        event = nullptr; 
    }

    return ptr_t(event);
}

int perf_event::open()
{
    this->_fd = perf_event_open(&this->_hw, this->_pid, this->_cpu, this->_group_fd, this->_flags);

    if (this->_fd == -1) {
        perfm_warn("failed to open perf_event, %s\n", strerror_r(errno, NULL, 0));
    }

    return this->_fd;
}

int perf_event::close()
{
    if (this->_fd == -1) {
        return 0;
    }

    int ret = ::close(this->_fd);
    if (ret != -1) {
        this->_fd = -1;
    } else {
        perfm_warn("failed to close fd\n");
    }

    return ret;
}

struct perf_event_attr *perf_event::attribute() {
    perf_event_attr *hw_copy = nullptr;
    try {
        hw_copy = new perf_event_attr;
    } catch (const std::bad_alloc &) {
        hw_copy = nullptr;
    }
    
    if (hw_copy) {
        memmove(&hw_copy, &this->_hw, sizeof(hw_copy));
    }

    return hw_copy;
}

bool event::read()
{
    bool read_succ = true;

    this->_pmu_prev[0] = this->_pmu_curr[0];
    this->_pmu_prev[1] = this->_pmu_curr[1];
    this->_pmu_prev[2] = this->_pmu_curr[2];

    ssize_t nr = ::read(this->perf_fd(), this->_pmu_curr, sizeof(this->_pmu_curr));
    if (nr != sizeof(this->_pmu_curr)) {
        perfm_warn("read PMU counters failed, %s\n", strerror_r(errno, NULL, 0));
        read_succ = false;
    }

    return read_succ;
}

uint64_t event::delta() const
{
    if (_pmu_curr[2] > _pmu_curr[1]) {
        perfm_warn("running time (%zu) > enabled time (%zu).\n", _pmu_curr[2], _pmu_curr[1]);
    }

    if (_pmu_curr[2] == 0 && _pmu_curr[0] != 0) {
        perfm_warn("running time *zero*, scaling failed. %zu, %zu, %zu.\n", _pmu_curr[0], _pmu_curr[1], _pmu_curr[2]);
    }

    if (_pmu_curr[2] <= _pmu_prev[2]) {
        perfm_warn("running time curr (%zu) <= prev (%zu).\n", _pmu_curr[2], _pmu_prev[2]);
        return 0;
    }

    double prev[3], curr[3];
    
    prev[0] = _pmu_prev[0];
    prev[1] = _pmu_prev[1];
    prev[2] = _pmu_prev[2];

    curr[0] = _pmu_curr[0];
    curr[1] = _pmu_curr[1];
    curr[2] = _pmu_curr[2];

    return static_cast<uint64_t>((curr[0] - prev[0]) * (curr[1] - prev[1]) / (curr[2] - prev[2]));
}

uint64_t event::scale() const
{
    uint64_t res = 0;

    if (_pmu_curr[2] > _pmu_curr[1]) {
        perfm_warn("running time (%zu) > enabled time (%zu).\n", _pmu_curr[2], _pmu_curr[1]);
    }

    if (_pmu_curr[2] == 0 && _pmu_curr[0] != 0) {
        perfm_warn("running time *zero*, scaling failed. %zu, %zu, %zu.\n", _pmu_curr[0], _pmu_curr[1], _pmu_curr[2]);
    }

    if (_pmu_curr[2] != 0) {
        res = static_cast<uint64_t>(1.0 * _pmu_curr[0] * _pmu_curr[1] / _pmu_curr[2]);
    }
    
    return res;
}

event::ptr_t event::alloc()
{
    event *ev = nullptr;

    try {
        ev = new event;
    } catch (const std::bad_alloc &e) {
        perfm_warn("%s\n", e.what());
        ev = nullptr;
    }
    
    return ptr_t(ev);
}

int event::open(const std::string &evn, pid_t pid, int cpu, int grp, unsigned long plm, unsigned long flg)
{
    this->process(pid);
    this->cpu(cpu);
    this->mask(flg);
    this->leader(grp);

    this->_plm = plm;
    this->_nam = evn;

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

    // disabled = 1 for group leader; disabled = 0 for group member
    hw.disabled = -1 == this->leader() ? 1 : 0;

    // include timing information for scaling
    if (perfm_options.rdfmt_timeing) {
        hw.read_format |= PEV_RDFMT_TIMEING;
    }
    
    // PERF_FORMAT_GROUP
    if (perfm_options.rdfmt_evgroup && this->leader() == -1) {
        hw.read_format |= PEV_RDFMT_EVGROUP;
    }

    // count events of child tasks as well as the task specified
    if (perfm_options.incl_children) {
        hw.inherit = 1;
    } 

    /* TODO (we need more setting) */

    this->attribute(&hw);

    return this->open();
}

void event::print() const
{
    FILE *fp = perfm_options.fp_out;
    if (!fp) {
        fp = stdout;
    }

    fprintf(fp, "- EVENT - %s\n", this->name().c_str());

    /* TODO */
}

void event::prcfg() const
{
    /* TODO */
}

void ev2perf(const std::string &evn, FILE *fp)
{
    struct perf_event_attr hw; 
    memset(&hw, 0, sizeof(hw));

    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));

    arg.attr = &hw;
    arg.size = sizeof(arg);

    pfm_err_t ret = pfm_get_os_event_encoding(evn.c_str(), PFM_PLM3 | PFM_PLM0, PFM_OS_PERF_EVENT, &arg);
    if (ret != PFM_SUCCESS) {
        perfm_warn("%s, %s\n", evn.c_str(), pfm_strerror(ret));
        return;
    }

    fprintf(fp, "- EVENT - %s\n", evn.c_str());
    fprintf(fp, "  perf.type                    : %s\n"
                "      .config                  : %zx\n"
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
                "",
                ev_type[hw.type],
                hw.config,
                hw.disabled                 ? "true" : "false",
                hw.inherit                  ? "true" : "false",
                hw.pinned                   ? "true" : "false",
                hw.exclusive                ? "true" : "false",
                hw.exclude_user             ? "true" : "false",
                hw.exclude_kernel           ? "true" : "false",
                hw.exclude_hv               ? "true" : "false",
                hw.exclude_idle             ? "true" : "false",
                hw.inherit_stat             ? "true" : "false",
                hw.task                     ? "true" : "false",
                hw.exclude_callchain_kernel ? "true" : "false",
                hw.exclude_callchain_user   ? "true" : "false",
                hw.use_clockid              ? "true" : "false"
            );
    fprintf(fp, "\n");
}

} /* namespace perfm */
