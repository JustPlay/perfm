#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_evgrp.hpp"
#include "perfm_monitor.hpp"

namespace perfm {

inline bool pid_exist(int pid)
{
    return ::access(std::string("/proc/" + std::to_string(pid) + "/status").c_str(), F_OK) == 0 ? true : false;
}

inline bool cpu_exist(int cpu)
{
    bool is_onln = true;

    int nr_cpu = sysconf(_SC_NPROCESSORS_ONLN);

    // when cpuX is online, '/sys/devices/system/cpu/cpuX/cache' EXISTS on x86 linux, otherwise NOT
    if (cpu >= nr_cpu || ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache").c_str(), F_OK) != 0) {
        is_onln = false;
    }

    return is_onln; 
}

void monitor_t::open()
{
    int pid = perfm_options.pid;
    if (pid != -1 && !pid_exist(pid)) {
        perfm_fatal("target process [%d] does NOT existed, exiting...\n", pid);
    }

    int cpu = perfm_options.cpu;
    if (cpu != -1 && !cpu_exist(cpu)) {
        perfm_fatal("target cpu [%d] does NOT existed or online, exiting...\n", cpu);
    }

    /* permissions checking */
    /* TODO */

    /* open event groups */
    const auto &ev_group = perfm_options.get_event_group();
    
    for (const auto &ev_list : ev_group) {
        evgrp_t::ptr_t grp = evgrp_t::creat();
        grp->gr_open(ev_list, perfm_options.pid, perfm_options.cpu, perfm_options.plm);
        grp_list.push_back(grp);
    }

    assert(grp_list.size() == ev_group.size());
}

void monitor_t::close()
{
    for (decltype(grp_list.size()) i = 0; i < grp_list.size(); ++i) {
        grp_list[i]->gr_close();
    }
}

void monitor_t::start() 
{
    loop();
}

void monitor_t::stop() 
{
    /* TODO */
}

void monitor_t::rr()
{
    size_t id = 0;
    size_t nr = grp_list.size();

    while (id++ < nr) {
        // 1. start the monitor
        grp_list[id]->gr_start();

        // 2. monitor for the specified time interval
        nanoseconds_sleep(perfm_options.interval);
        
        // 3. stop the monitor
        grp_list[id]->gr_stop();

        // 4. read/print the PMU values
        grp_list[id]->gr_read();
        grp_list[id]->gr_print();
    }
}

int monitor_t::loop()
{
    assert(grp_list.size());

    int r = perfm_options.loops;

    while (r--) {
        rr();
    }

    return r;
}

} /* namespace perfm */

