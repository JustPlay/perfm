#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>

#include <sys/types.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_option.hpp"

namespace perfm {

int group_t::gr_open(const std::string &ev_list, pid_t pid, int cpu, const std::string &plm)
{
    auto ev_argv = explode(",", ev_list, options_t::nr_event_supp()); 
    return this->gr_open(ev_argv, pid, cpu, plm);
}

int group_t::gr_open(const std::vector<std::string> &ev_argv, pid_t pid, int cpu, const std::string &plm)
{
    this->pid = pid;
    this->cpu = cpu;
    this->grp = 0; // just let the *first* event in this group to be the leader

    // Event encoding (initialize the perf_event_attr structure)
    pfm_perf_encode_arg_t arg;

    for (decltype(ev_argv.size()) i = 0; i < ev_argv.size(); ++i) {
        event_t::ptr_t ev = event_t::creat();
        memset(&ev->pea, 0, sizeof(ev->pea));

        memset(&arg, 0, sizeof(arg));
        arg.attr = &ev->pea;
        arg.size = sizeof(arg);

        unsigned long plm_val= 0;

        if (std::strchr(plm.c_str(), 'u')) {
            plm_val |= PFM_PLM3;
        }

        if (std::strchr(plm.c_str(), 'k')) {
            plm_val |= PFM_PLM0;
        }

        if (std::strchr(plm.c_str(), 'h')) {
            plm_val |= PFM_PLMH;
        }

        if (!plm_val) {
            perfm_warning("%s\n", "The privilege level mask argument should NOT be zero");
        }

        this->plm = plm_val;

        pfm_err_t ret = pfm_get_os_event_encoding(ev_argv[i].c_str(), this->plm, PFM_OS_PERF_EVENT, &arg);
        if (ret != PFM_SUCCESS) {
            perfm_error("Event encoding error: %s, %s\n", ev_argv[i].c_str(), pfm_strerror(ret));
        }

        ev->nam = ev_argv[i];
        ev->pid = this->pid;
        ev->cpu = this->cpu;
        ev->plm = this->plm;
        ev->flg = this->flg;

        ev_list.push_back(ev);
    }

    // Call perf_event_open() for each event in this group
    for (int i = 0; i < this->gr_size(); ++i) {
        ev_list[i]->pea.disabled    = i == this->grp ? 1 : 0; /* disabled = 1 for group leader; disabled = 0 for others */
        ev_list[i]->pea.read_format = PERF_EVENT_READ_FORMAT_SCALE; /* include timing information for scaling */

        ev_list[i]->grp = i == this->grp ? -1 : gr_leader()->ev_fd();

        /* TODO */

        ev_list[i]->ev_open();
    }

    return gr_leader()->ev_fd(); /* return the group leader's perf_event fd */
}

size_t group_t::gr_read()
{
    size_t nr_event_read = 0; /* # of events in this group which has been read succesfully */

    for (auto iter = ev_list.begin(); iter != ev_list.end(); ++iter) {
        if ((*iter)->ev_read()) {
            ++nr_event_read;
        }
    }

    return nr_event_read;
}

void group_t::gr_print() const
{
    FILE *fp = perfm_options.fp_output();
    if (!fp) {
        fp = stdout;
    }

    fprintf(fp, "-------------------------------------------------------\n"); 
    fprintf(fp, "- Event Group - nr_events: %-4lu  leader: %-10d   -\n", gr_size(), gr_leader()->ev_fd());
    fprintf(fp, "-------------------------------------------------------\n"); 

    for (decltype(ev_list.size()) i = 0; i < ev_list.size(); ++i) {
        event_t::ptr_t evp = ev_list[i];

        fprintf(fp, "- Event - %s\n", evp->ev_nam().c_str());

        fprintf(fp, "- curr pmu vals : %llu  %llu  %llu\n", evp->pmu_val_curr[0], evp->pmu_val_curr[1], evp->pmu_val_curr[2]);
        fprintf(fp, "- prev pmu vals : %llu  %llu  %llu\n", evp->pmu_val_prev[0], evp->pmu_val_prev[1], evp->pmu_val_prev[2]);

        fprintf(fp, "\n");
    }
}

} /* namespace perfm */

