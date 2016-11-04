#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_option.hpp"

namespace {

/* 
 * Must be consistent with the 'perf_type_id' defined in
 *     <linux/perf_event.h> 
 * or
 *     <perfmon/perf_event.h>
 */
const char *perf_evtype[PERF_TYPE_MAX] = {
    "PERF_TYPE_HARDWARE",
    "PERF_TYPE_SOFTWARE",
    "PERF_TYPE_TRACEPOINT",
    "PERF_TYPE_HW_CACHE",
    "PERF_TYPE_RAW",
    "PERF_TYPE_BREAKPOINT",
};

} /* namesapce */

namespace perfm {

int event_t::ev_open(const std::string &evn, pid_t pid, int grp, int cpu, unsigned long plm, unsigned long flg)
{
    this->pid = pid;
    this->grp = grp;
    this->cpu = cpu;
    this->flg = flg;

    this->plm = plm;
    this->nam = evn;

    this->fd  = -1;

    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));

    memset(&this->pea, 0, sizeof(this->pea));

    arg.attr = &this->pea;
    arg.size = sizeof(arg);

    pfm_err_t ret = pfm_get_os_event_encoding(this->nam.c_str(), this->plm, PFM_OS_PERF_EVENT, &arg);
    if (ret != PFM_SUCCESS) {
        perfm_warning("%s, %s\n", this->nam.c_str(), pfm_strerror(ret));
        return -1;
    }

    /* disabled = 1 for group leader; disabled = 0 for group member */
    this->pea.disabled = -1 == this->grp ? 1 : 0;

    /* include timing information for scaling */
    if (perfm_options.rdfmt_timeing) {
        this->pea.read_format |= PEV_RDFMT_TIMEING;
    }
    
    /* PERF_FORMAT_GROUP */
    if (perfm_options.rdfmt_evgroup && this->grp == -1) {
        this->pea.read_format |= PEV_RDFMT_EVGROUP;
    }

    /* count events of child tasks as well as the task specified */
    if (perfm_options.incl_children) {
        this->pea.inherit = 1;
    } 

    /* TODO */

    return this->ev_open();
}

int event_t::ev_open()
{
    this->fd = perf_event_open(&this->pea, this->pid, this->cpu, this->grp, this->flg); 

    if (this->fd == -1) {
        perfm_warning("perf_event_open() failed on event: %s, process: %d, cpu: %d\n", this->nam.c_str(), this->pid, this->cpu);
    }

    return this->fd;
}

int event_t::ev_close()
{
    if (this->fd == -1) {
        return 0;
    }

    int ret = ::close(this->fd);
    if (ret != -1) {
        this->fd = -1;
    }

    return ret;
}

bool event_t::ev_read()
{
    bool is_read_succ = true;

    this->pmu_prev[0] = this->pmu_curr[0];
    this->pmu_prev[1] = this->pmu_curr[1];
    this->pmu_prev[2] = this->pmu_curr[2];

    ssize_t ret = ::read(this->fd, this->pmu_curr, sizeof(this->pmu_curr)); 
    if (ret != sizeof(this->pmu_curr)) {
        char buferr[PERFM_BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        perfm_warning("read PMU counters failed, %s\n", buferr);

        is_read_succ = false;
    }

    return is_read_succ;
}

uint64_t event_t::ev_delta() const
{
    if (pmu_curr[2] > pmu_curr[1]) {
        perfm_warning("running time (%zu) > enabled time (%zu).\n", pmu_curr[2], pmu_curr[1]);
    }

    if (pmu_curr[2] == 0 && pmu_curr[0] != 0) {
        perfm_warning("running time *zero*, scaling failed. %zu, %zu, %zu.\n", pmu_curr[0], pmu_curr[1], pmu_curr[2]);
    }

    if (pmu_curr[2] <= pmu_prev[2]) {
        perfm_warning("running time curr (%zu) <= prev (%zu).\n", pmu_curr[2], pmu_prev[2]);
        return 0;
    }

    double prev[3], curr[3];
    
    prev[0] = pmu_prev[0];
    prev[1] = pmu_prev[1];
    prev[2] = pmu_prev[2];

    curr[0] = pmu_curr[0];
    curr[1] = pmu_curr[1];
    curr[2] = pmu_curr[2];

    return static_cast<uint64_t>((curr[0] - prev[0]) * (curr[1] - prev[1]) / (curr[2] - prev[2]));

}

uint64_t event_t::ev_scale() const
{
    uint64_t res = 0;

    if (pmu_curr[2] > pmu_curr[1]) {
        perfm_warning("running time (%zu) > enabled time (%zu).\n", pmu_curr[2], pmu_curr[1]);
    }

    if (pmu_curr[2] == 0 && pmu_curr[0] != 0) {
        perfm_warning("running time *zero*, scaling failed. %zu, %zu, %zu.\n", pmu_curr[0], pmu_curr[1], pmu_curr[2]);
    }

    if (pmu_curr[2] != 0) {
        res = static_cast<uint64_t>(1.0 * pmu_curr[0] * pmu_curr[1] / pmu_curr[2]);
    }
    
    return res;
}

void event_t::ev_print() const
{
    FILE *fp = perfm_options.out_fp;
    if (!fp) {
        fp = stdout;
    }

    fprintf(fp, "- EVENT - %s\n", this->ev_nam().c_str());
    fprintf(fp, "  pmu curr: %zu  %zu  %zu\n", pmu_curr[0], pmu_curr[1], pmu_curr[2]);
    fprintf(fp, "  pmu prev: %zu  %zu  %zu\n", pmu_prev[0], pmu_prev[1], pmu_prev[2]);
}

void event_t::ev_prcfg() const
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
        perfm_warning("%s, %s\n", evn.c_str(), pfm_strerror(ret));
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
                perf_evtype[hw.type],
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
