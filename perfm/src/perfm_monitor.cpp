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

inline bool pid_exist(int p)
{
    return ::access(std::string("/proc/" + std::to_string(p) + "/status").c_str(), F_OK) == 0;
}

inline bool cpu_exist(int c)
{
    // int sc_cpu_conf = sysconf(_SC_NPROCESSORS_CONF); /* should be constant, BUT in older version linux or glibc ... */
    // int sc_cpu_onln = sysconf(_SC_NPROCESSORS_ONLN); /* will change when do cpu hotplug */

    // when cpuX is online, '/sys/devices/system/cpu/cpuX/cache' EXISTS on x86 linux, otherwise NOT
    return c >= 0 || ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cache").c_str(), F_OK) == 0;
}

monitor::ptr_t monitor::alloc()
{
    monitor *m = nullptr;

    try {
        m = new monitor;
    } catch (const std::bad_alloc &) {
        m = nullptr;
    }

    return ptr_t(m);
}

void monitor::parse_cpu_list(const std::string &list)
{
    std::vector<std::string> slice = str_split(list, ",");

    for (size_t i = 0; i < slice.size(); ++i) {
        if (slice[i].empty()) {
            continue;
        }

        size_t pos = slice[i].find("-");

        if (pos == std::string::npos) {
            int c;
            try {
                c = std::stoi(slice[i]);
            } catch (const std::exception &e) {
                perfm_warn("%s %s\n", e.what(), slice[i].c_str());
                continue;
            }

            ++_nr_select_cpu;
            do_set(c);
        } else {
            int from, to;
            try {
                from = std::stoi(slice[i]);
                to   = std::stoi(slice[i].substr(pos + 1));
            } catch (const std::exception &e) {
                perfm_warn("%s %s\n", e.what(), slice[i].c_str());
                continue;
            }

            for (int c = from; c <= to; ++c) {
                ++_nr_select_cpu;
                do_set(c);
            }
        }
    }
}

void monitor::open()
{
    _nr_total_cpu = num_cpus_total();

    if (perfm_options.cpu_list.empty()) {
        for (unsigned int c = 0; c < _nr_total_cpu; ++c) {
            do_set(c);
            ++_nr_select_cpu;
        }
    } else {
        parse_cpu_list(perfm_options.cpu_list);
    }

    for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (!cpu_exist(c)) {
            perfm_warn("cpu %2d does NOT online and will be ignored\n", c);
            do_clr(c);
        }
    }

    int pid = perfm_options.pid;
    if (pid != -1 && !pid_exist(pid)) {
        perfm_fatal("target process [%d] does NOT existed, exiting...\n", pid);
    }

    /* TODO (permissions checking) */

    const auto &ev_group = perfm_options.get_event_group();
    
    for (const auto &ev_list : ev_group) {
        group::ptr_t g = group::alloc();
        if (!g) {
            perfm_warn("failed to alloc group object\n");
            continue;
        }

        // FIXME
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

