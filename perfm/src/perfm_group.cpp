#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_parser.hpp"

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

bool group::open(const std::string &list, pid_t pid, int cpu, unsigned long flags)
{
    const auto &argv = str_split(list, ",", options::sz_group_max()); 
    return open(argv, pid, cpu, flags);
}

bool group::open(const std::vector<std::string> &argv, pid_t pid, int cpu, unsigned long flags)
{
    /*
     * FIXME:
     *    remove libpfm4
     */
    _pid   = pid;
    _cpu   = cpu;
    _flags = flags;
    _grp   = 0; // just let the *first* event in this group to be the leader

    for (decltype(argv.size()) i = 0; i < argv.size(); ++i) {
        event::ptr_t e = event::alloc();
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

        pfm_err_t ret = pfm_get_os_event_encoding(argv[i].c_str(), PFM_PLM3 | PFM_PLM0, PFM_OS_PERF_EVENT, &arg);
        if (ret != PFM_SUCCESS) {
            if (perfm_options.skip_err) {
                perfm_warn("invalid event (ignored), %s\n", argv[i].c_str());
                continue;
            } else {
                perfm_fatal("event encoding error, %s %s\n", argv[i].c_str(), pfm_strerror(ret));
            }
        }

        e->process(_pid);
        e->cpu(_cpu);
        e->flag(_flags);
        e->raw_name(argv[i]);
        e->attr(&hw);

        _e_list.push_back(e);
    }

    for (size_t i = 0, n = size(); i < n; ++i) {
        struct perf_event_attr *hw = _e_list[i]->attr();
        if (!hw) {
            perfm_fatal("failed to get event's attr\n");
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

        _e_list[i]->leader(i == this->_grp ? -1 : leader()->fd());
        _e_list[i]->attr(hw);

        _e_list[i]->open();

        free(hw);
    }

    /*
     * FIXME:
     *    error handling
     */
    return true;
}

bool group::close() {
    for (size_t e = 0; e < _e_list.size(); ++e) {
        _e_list[e]->close();
    }

    return true; /* FIXME (error handling) */
}

bool group::read()
{
    if (perfm_options.rdfmt_evgroup) {
        size_t nr_events = nr_event();
        size_t sz_buffer = sizeof(uint64_t) * (3 + nr_events);

        uint64_t *buf = nullptr;
        try {
            buf = new uint64_t[sz_buffer];
        } catch (const std::bad_alloc &) {
            buf = nullptr; 
        }

        if (!buf) {
            perfm_fatal("memory allocate failed for %zu bytes\n", sz_buffer);
        }

        _tsc_prev = _tsc_curr;
        _tsc_curr = read_tsc();
        
        ssize_t ret = ::read(leader()->fd(), buf, sz_buffer);
        if (ret == -1) {
            perfm_fatal("%s\n", "error occured reading pmu counters");
        } 
        if (ret < sz_buffer) {
            perfm_fatal("read events counters, need %zu bytes, actually %zd bytes\n", sz_buffer, ret);
        }

        for (size_t i = 0; i < nr_events; ++i) {
            _e_list[i]->pmu_cntr(buf[3 + i], buf[1], buf[2]);
        }

        if (buf) {
            delete[] buf;
        }

        return true;

    } else {
        _tsc_prev = _tsc_curr;
        _tsc_curr = read_tsc();

        size_t nr_succ = 0;

        for (auto iter = _e_list.begin(); iter != _e_list.end(); ++iter) {
            if ((*iter)->read()) {
                ++nr_succ;
            }
        }
        return nr_succ == nr_event() ? true : false;
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
    // 2. column fields separated by DELIMITER
    // 3. event groups separated by an empty line
    for (decltype(_e_list.size()) i = 0; i < _e_list.size(); ++i) {
        fprintf(fp, "%s" DELIMITER "%zu\n", _e_list[i]->name().c_str(), _e_list[i]->scale());
    }

    fprintf(fp, "\n");
}

} /* namespace perfm */
