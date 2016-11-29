#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_monitor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>
#include <new>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <perfmon/pfmlib_perf_event.h>

namespace perfm {

inline bool pid_exist(int pid)
{
    return ::access(std::string("/proc/" + std::to_string(pid) + "/status").c_str(), F_OK) == 0 ? true : false;
}

inline bool cpu_exist(int cpu)
{
    // int sc_cpu_conf = sysconf(_SC_NPROCESSORS_CONF); /* should be constant, BUT in older version linux or glibc ... */
    // int sc_cpu_onln = sysconf(_SC_NPROCESSORS_ONLN); /* will change when do cpu hotplug */

    // when cpuX is online, '/sys/devices/system/cpu/cpuX/cache' EXISTS on x86 linux, otherwise NOT
    if (cpu < 0 || ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache").c_str(), F_OK) != 0) {
        return false;
    }

    return true; 
}

monitor::ptr_t monitor::alloc()
{
    monitor *m = nullptr;

    try {
        m = new monitor;
    } catch (const std::bad_alloc &) {
        m = nullptr;
        perfm_warn("failed to alloc monitor object\n");
    }

    return ptr_t(m);
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
        group::ptr_t g = group::alloc();
        g->open(ev_list, perfm_options.pid, perfm_options.cpu);
        _glist.push_back(g);
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

