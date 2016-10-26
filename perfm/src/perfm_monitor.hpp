/**
 * perfm_monitor.hpp - interface for perfm monitor
 *
 */
#ifndef __PERFM_MONITOR_HPP__
#define __PERFM_MONITOR_HPP__

#include <cstdlib>
#include <vector>
#include <string>

#include <sys/types.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_evgrp.hpp"
#include "perfm_option.hpp"

namespace perfm {

class monitor_t {

public:

    void open();
    void close();

    void start();
    void stop();

private:
    int rr();

private:
    std::vector<evgrp_t::ptr_t> grp_list; /* event group list */
};

} /* namespace perfm */

#endif /* __PERFM_MONITOR_HPP__ */
