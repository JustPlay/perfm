#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint> /* uint64_t */
#include <vector>
#include <memory>

#include <sys/types.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_option.hpp"

namespace perfm {

group::ptr_t group::creat()
{
    group *evgrp = nullptr;

    try {
        evgrp = new group;
    } catch (const std::bad_alloc &e) {
        evgrp = nullptr;
        perfm_warn("%s\n", e.what());
    }
    
    return ptr_t(evgrp);
}

int group::open(const std::string &elist, pid_t pid, int cpu, const std::string &plm)
{
    const auto &eargv = str_split(elist, ",", options_t::sz_group_max()); 
    return this->open(eargv, pid, cpu, plm);
}

int group::open(const std::vector<std::string> &eargv, pid_t pid, int cpu, const std::string &plm)
{
    this->_pid = pid;
    this->_cpu = cpu;
    this->_grp = 0; // just let the *first* event in this group to be the leader

    // event encoding (initialize the perf_event_attr structure)
    pfm_perf_encode_arg_t arg;

    for (decltype(eargv.size()) i = 0; i < eargv.size(); ++i) {
        event_t::ptr_t ev = event_t::creat();
        if (!ev) {
            perfm_warn("failed to creat event object\n");
            continue;
        }

        memset(&arg, 0, sizeof(arg));
        arg.attr = &ev->_hw;
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

        ev->_nam = eargv[i];
        ev->_pid = this->_pid;
        ev->_cpu = this->_cpu;
        ev->_plm = this->_plm;
        ev->_flg = this->_flg;

        _elist.push_back(ev);
    }

    // call perf_event_open() for each event in this group
    for (size_t i = 0; i < this->size(); ++i) {
        _elist[i]->_hw.disabled = i == this->_grp ? 1 : 0;   /* disabled = 1 for group leader; disabled = 0 for others */
        if (perfm_options.rdfmt_timeing) {
            _elist[i]->_hw.read_format |= PEV_RDFMT_TIMEING; /* include timing information for scaling */
        }
        if (perfm_options.rdfmt_evgroup && i == this->_grp) {
            _elist[i]->_hw.read_format |= PEV_RDFMT_EVGROUP; /* PERF_FORMAT_GROUP */
        }
        if (perfm_options.incl_children) {
            _elist[i]->_hw.inherit = 1;
        }

        _elist[i]->_grp = i == this->_grp ? -1 : leader()->fd();

        /* TODO */

        _elist[i]->open();
    }

    return leader()->fd(); /* return the group leader's perf_event fd */
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

        ssize_t ret = ::read(this->leader()->fd(), val, siz_buffer);
        if (ret == -1) {
            perfm_fatal("%s\n", "error occured reading pmu counters");
        } 
        if (ret < siz_buffer) {
            perfm_warn("read events counters, need %zu bytes, actually %zd bytes\n", siz_buffer, ret);
        }

        for (size_t i = 0; i < num_events; ++i) {
            _elist[i]->set_pmu_cntr(val[3 + i], val[1], val[2]);
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
    fprintf(fp, "- Event Group - nr_events: %-4lu  leader: %-10d   -\n", size(), leader()->fd());
    fprintf(fp, "-------------------------------------------------------\n"); 

    for (decltype(_elist.size()) i = 0; i < _elist.size(); ++i) {
        _elist[i]->print();
        fprintf(fp, "\n");
    }
}

} /* namespace perfm */

