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
const char *perf_event_type_desc[PERF_TYPE_MAX] = {
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
        perfm_warning("Event encoding error: %s, %s\n", this->nam.c_str(), pfm_strerror(ret));
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
        perfm_warning("Call to perf_event_open() failed for event: %s, process: %d, cpu: %d\n", this->nam.c_str(), this->pid, this->cpu);
    }

    return this->fd;
}

bool event_t::ev_read()
{
    bool is_read_succ = true;

    this->pmu_val_prev[0] = this->pmu_val_curr[0];
    this->pmu_val_prev[1] = this->pmu_val_curr[1];
    this->pmu_val_prev[2] = this->pmu_val_curr[2];

    ssize_t ret = ::read(this->fd, this->pmu_val_curr, sizeof(this->pmu_val_curr)); 
    if (ret != sizeof(this->pmu_val_curr) && ret != sizeof(this->pmu_val_curr[0])) {
        char buferr[PERFM_BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        perfm_warning("Read PMU counters failed, %s\n", buferr);

        is_read_succ = false;
    }

    return is_read_succ;
}

void event_t::ev_print() const
{
    FILE *fp = perfm_options.out_fp;
    if (!fp) {
        fp = stdout;
    }

    fprintf(fp, "-----------------------------------------------------\n"); 
    fprintf(fp, "- Event - %s\n", this->ev_nam().c_str());
    fprintf(fp, "-----------------------------------------------------\n"); 

    fprintf(fp, "- perf event type   : %s\n",  perf_event_type_desc[this->pea.type]);
    fprintf(fp, "- perf event config : %lx\n", this->pea.config);

    fprintf(fp, "- perf event fd     : %d\n",  this->ev_fd());
    fprintf(fp, "- perf event group  : %d\n",  this->ev_grp());
    fprintf(fp, "- monitored cpu     : %d\n",  this->ev_cpu());
    fprintf(fp, "- monitored process : %d\n",  this->ev_pid());

    fprintf(fp, "- curr pmu vals     : %zu  %zu  %zu\n", pmu_val_curr[0], pmu_val_curr[1], pmu_val_curr[2]);
    fprintf(fp, "- prev pmu vals     : %zu  %zu  %zu\n", pmu_val_prev[0], pmu_val_prev[1], pmu_val_prev[2]);
}

} /* namespace perfm */

