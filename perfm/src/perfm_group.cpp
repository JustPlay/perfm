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
    } catch (const std::bad_alloc &) {
        g = nullptr;
    }
    
    return ptr_t(g);
}

int group::open(const std::string &elist, pid_t pid, int cpu, unsigned long flags)
{
    const auto &eargv = str_split(elist, ",", options::sz_group_max()); 
    return this->open(eargv, pid, cpu, flags);
}

int group::open(const std::vector<std::string> &eargv, pid_t pid, int cpu, unsigned long flags)
{
    this->_pid   = pid;
    this->_cpu   = cpu;
    this->_grp   = 0; // just let the *first* event in this group to be the leader
    this->_flags = flags;

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

        pfm_err_t ret = pfm_get_os_event_encoding(eargv[i].c_str(), PFM_PLM3 | PFM_PLM0, PFM_OS_PERF_EVENT, &arg);
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
        ev->mask(this->_flags);
        ev->name(eargv[i]);
        ev->attribute(&hw);

        _elist.push_back(ev);
    }

    for (size_t i = 0; i < this->size(); ++i) {

        struct perf_event_attr *hw = _elist[i]->attribute();
        if (!hw) {
            perfm_fatal("failed to get event's attribute\n");
        }

        hw->disabled = i == this->_grp ? 1 : 0;   /* disabled = 1 for group leader, 0 for others */
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
    for (size_t e = 0; e < _elist.size(); ++e) {
        _elist[e]->close();
    }

    return 0; /* FIXME (error handling) */
}

size_t group::read()
{
    if (perfm_options.rdfmt_evgroup) {
        size_t nr_events = this->size();
        size_t sz_buffer = sizeof(uint64_t) * (3 + nr_events);

        uint64_t *val = new uint64_t[sz_buffer];
        if (!val) {
            perfm_fatal("memory allocate failed for %zu bytes\n", sz_buffer);
        }

        ssize_t ret = ::read(this->leader()->perf_fd(), val, sz_buffer);
        if (ret == -1) {
            perfm_fatal("%s\n", "error occured reading pmu counters");
        } 
        if (ret < sz_buffer) {
            perfm_warn("read events counters, need %zu bytes, actually %zd bytes\n", sz_buffer, ret);
        }

        for (size_t i = 0; i < nr_events; ++i) {
            _elist[i]->pmu_cntr(val[3 + i], val[1], val[2]);
        }

        if (val) {
            delete[] val;
        }

        return nr_events;

    } else {
        size_t nr_events = 0; /* # of events which has been read succesfully */

        for (auto iter = _elist.begin(); iter != _elist.end(); ++iter) {
            if ((*iter)->read()) {
                ++nr_events;
            }
        }
        return nr_events;
    }
}

void group::print() const
{
    FILE *fp = perfm_options.fp_out;
    if (!fp) {
        fp = stdout;
    }

    #define DELIMITER "  "

    // 1. each event a line
    // 2. event groups are separated by an empty line
    // 3. column fields are separated by DELIMITER
    for (decltype(_elist.size()) i = 0; i < _elist.size(); ++i) {
        fprintf(fp, "%s" DELIMITER "%zu\n", _elist[i]->name().c_str(), _elist[i]->scale());
    }

    fprintf(fp, "\n");
}

} /* namespace perfm */

