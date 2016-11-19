#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint> /* uint64_t */
#include <vector>
#include <memory>

#include <sys/types.h>

namespace perfm {

group::ptr_t group::alloc()
{
    group *g = nullptr;

    try {
        g = new group;
    } catch (const std::bad_alloc &e) {
        g = nullptr;
        perfm_warn("%s\n", e.what());
    }
    
    return ptr_t(g);
}

int group::open(const std::string &elist, pid_t pid, int cpu, const std::string &plm)
{
    const auto &eargv = str_split(elist, ",", options::sz_group_max()); 
    return this->open(eargv, pid, cpu, plm);
}

int group::open(const std::vector<std::string> &eargv, pid_t pid, int cpu, const std::string &plm)
{
    this->_pid = pid;
    this->_cpu = cpu;
    this->_grp = 0; // just let the *first* event in this group to be the leader

    for (decltype(eargv.size()) i = 0; i < eargv.size(); ++i) {
        event::ptr_t ev = event::alloc();
        if (!ev) {
            perfm_warn("failed to alloc event object\n");
            continue;
        }

        pfm_perf_encode_arg_t arg;
        memset(&arg, 0, sizeof(arg));

        struct perf_event_attr hw;
        memset(&hw, 0, sizeof(hw));

        arg.attr = &hw;
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
            perfm_warn("privilege level mask should *not* be zero\n");
        }

        this->_plm = plm_val;

        pfm_err_t ret = pfm_get_os_event_encoding(eargv[i].c_str(), this->_plm, PFM_OS_PERF_EVENT, &arg);
        if (ret != PFM_SUCCESS) {
            if (perfm_options.skip_err) {
                perfm_warn("invalid event (ignored), %s\n", eargv[i].c_str());
                continue;
            } else {
                perfm_fatal("event encoding error, %s %s\n", eargv[i].c_str(), pfm_strerror(ret));
            }
        }

        ev->process(this->_pid);
        ev->cpu(this->_cpu);
        ev->mask(this->_flg);
        ev->pl_mask(this->_plm);
        ev->name(eargv[i]);

        _elist.push_back(ev);
    }

    for (size_t i = 0; i < this->size(); ++i) {
        struct perf_event_attr *hw = _elist[i]->attribute();

        hw->disabled = i == this->_grp ? 1 : 0;   /* disabled = 1 for group leader; disabled = 0 for others */
        if (perfm_options.rdfmt_timeing) {
            hw->read_format |= PEV_RDFMT_TIMEING; /* include timing information for scaling */
        }
        if (perfm_options.rdfmt_evgroup && i == this->_grp) {
            hw->read_format |= PEV_RDFMT_EVGROUP; /* PERF_FORMAT_GROUP */
        }
        if (perfm_options.incl_children) {
            hw->inherit = 1;
        }

        _elist[i]->leader(i == this->_grp ? -1 : leader()->perf_fd());
        _elist[i]->attribute(hw);

        _elist[i]->open();

        free(hw);
    }

    return leader()->perf_fd(); /* return the group leader's perf_event fd */
}

int group::close() {
    for (auto &e : _elist) {
        e->close();
    }

    return 0; /* FIXME (error handling) */
}

size_t group::read()
{
    if (perfm_options.rdfmt_evgroup) {
        size_t num_events = this->size();
        size_t siz_buffer = sizeof(uint64_t) * (3 + num_events);

        uint64_t *val = new uint64_t[siz_buffer];
        if (!val) {
            perfm_fatal("memory allocate failed for %zu bytes\n", siz_buffer);
        }

        ssize_t ret = ::read(this->leader()->perf_fd(), val, siz_buffer);
        if (ret == -1) {
            perfm_fatal("%s\n", "error occured reading pmu counters");
        } 
        if (ret < siz_buffer) {
            perfm_warn("read events counters, need %zu bytes, actually %zd bytes\n", siz_buffer, ret);
        }

        for (size_t i = 0; i < num_events; ++i) {
            _elist[i]->pmu_cntr(val[3 + i], val[1], val[2]);
        }

        if (val) {
            delete[] val;
        }

        return num_events;

    } else {
        size_t num_events = 0; /* # of events which has been read succesfully */

        for (auto iter = _elist.begin(); iter != _elist.end(); ++iter) {
            if ((*iter)->read()) {
                ++num_events;
            }
        }
        return num_events;
    }
}

void group::print() const
{
    FILE *fp = perfm_options.fp_out;
    if (!fp) {
        fp = stdout;
    }

    fprintf(fp, "-------------------------------------------------------\n"); 
    fprintf(fp, "- Event Group - nr_events: %-4lu  leader: %-10d   -\n", size(), leader()->perf_fd());
    fprintf(fp, "-------------------------------------------------------\n"); 

    for (decltype(_elist.size()) i = 0; i < _elist.size(); ++i) {
        _elist[i]->print();
        fprintf(fp, "\n");
    }

    /* TODO */
}

} /* namespace perfm */

