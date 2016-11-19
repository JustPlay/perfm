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
#include "perfm_group.hpp"
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

void monitor::open()
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
        group_t::ptr_t grp = group_t::creat();
        grp->open(ev_list, perfm_options.pid, perfm_options.cpu, perfm_options.plm);
        _glist.push_back(grp);
    }

    assert(_glist.size() == ev_group.size());
}

void monitor::close()
{
    for (decltype(_glist.size()) i = 0; i < _glist.size(); ++i) {
        _glist[i]->close();
    }
}

void monitor::start() 
{
    loop();
}

void monitor::stop() 
{
    /* TODO */
}

void monitor::rr()
{
    size_t id = 0;
    size_t nr = _glist.size();

    while (id++ < nr) {
        // 1. start the monitor
        _glist[id]->start();

        // 2. monitor for the specified time interval
        nanoseconds_sleep(perfm_options.interval);
        
        // 3. stop the monitor
        _glist[id]->stop();

        // 4. read/print the PMU values
        _glist[id]->read();
        _glist[id]->print();
    }
}

int monitor::loop()
{
    assert(_glist.size());

    int r = perfm_options.loops;

    while (r--) {
        rr();
    }

    return r;
}

} /* namespace perfm */

