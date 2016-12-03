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
#include <random>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <perfmon/pfmlib_perf_event.h>

namespace {

volatile sig_atomic_t should_quit = 0; /* SIGINT */

void sig_handler(int signo)
{
    switch (signo) {
    case SIGINT:
        should_quit = 1;
        break;

    default:
        ;
    }
}

} /* namespace */

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
    return c >= 0 && ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cache").c_str(), F_OK) == 0;
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
    this->_nr_total_cpu  = num_cpus_total();
    this->_nr_select_cpu = 0;
    this->_cpu = -1;

    memset(this->_cpu_list, 0, sizeof(this->_cpu_list));

    // alloc memory for each cpu
    try {
        this->_cpu_data = new _cpu_data_t[this->_nr_total_cpu]; 
    } catch (const std::bad_alloc &e) {
        perfm_fatal("failed to alloc memory, %s\n", e.what());
    }

    // parse cpu list specified by user
    if (perfm_options.cpu_list.empty()) {
        for (unsigned int c = 0; c < _nr_total_cpu; ++c) {
            ++_nr_select_cpu;
            do_set(c);
        }
    } else {
        parse_cpu_list(perfm_options.cpu_list);
    }

    for (unsigned int c = 0; c < _nr_total_cpu; ++c) {
        if (is_set(c) && !cpu_exist(c)) {
            perfm_warn("cpu %d does not online and will be ignored\n", c);
            --_nr_select_cpu;
            do_clr(c);
        }

        if (is_set(c) && _cpu == -1) {
            _cpu = c;
        }
    }

    int pid = perfm_options.pid;
    if (pid != -1 && !pid_exist(pid)) {
        perfm_fatal("process %d does not existed, exiting...\n", pid);
    }

    /* TODO (permissions checking) */

    for (const auto &ev_list : perfm_options.egroups) {
        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) { 
            if (!is_set(c)) {
                continue;
            }

            group::ptr_t g = group::alloc();
            if (!g) {
                perfm_fatal("failed to alloc group object\n");
            }

            /* FIXME (the pid & cpu argument) */
            g->open(ev_list, perfm_options.pid, c);

            _cpu_data[c].push_back(g);
        }
    }
}

void monitor::close()
{
    for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (!is_set(c)) {
            continue;
        }

        ++n;

        for (size_t g = 0; g < _cpu_data[c].size(); ++g) {
            _cpu_data[c][g]->close();
        }
    }
}

void monitor::start() 
{
    struct sigaction sig;

    memset(&sig, 0, sizeof(sig));
    sigemptyset(&sig.sa_mask);
    sig.sa_handler = sig_handler;

    if (sigaction(SIGINT, &sig, NULL) != 0) {
        perfm_warn("failed to install handler for SIGINT, %s\n", strerror_r(errno, NULL, 0));
    }

    loop();
}

void monitor::stop() 
{
    /* TODO */
}

void monitor::rr(double second)
{
    size_t nr_group = perfm_options.nr_group();

    for (size_t g = 0; g < nr_group; ++g) {
        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            }

            _cpu_data[c][g]->start();
        }

        nanosecond_sleep(second);

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            } 

            _cpu_data[c][g]->read();
        }

        print();

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            } 

            _cpu_data[c][g]->stop();
        }
    }
}

int monitor::loop()
{
    std::random_device rd;
    std::default_random_engine e(rd());
    std::uniform_real_distribution<double> dis(-0.01, 0.01); /* [-10ms, 10ms) */

    int n = perfm_options.loops;
    int r = 0;

    while (!should_quit && r < n) {
        rr(perfm_options.interval - dis(e));
        ++r;
    }

    return r;
}

void monitor::print() const
{
    #define delimiter " "
    #define is_first(cpu) (cpu) == 0

    FILE *fp = perfm_options.fp_out ? perfm_options.fp_out : stdout; 
    
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    //
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...

    const size_t nr_group = perfm_options.nr_group();

    size_t g = 0;
    size_t n;
    size_t e;

    while (g < nr_group) {
        n = _cpu_data[_cpu][g]->size();
        e = 0;

        while (e < n) {
            unsigned int c = 0;

            while (c < _nr_total_cpu) {
                event::ptr_t event = _cpu_data[c][g]->event(e);

                if (is_first(c)) {
                    fprintf(fp, "%s", event->name().c_str());
                }

                if (is_set(c)) {
                    fprintf(fp, delimiter "%lu", event->delta());
                } else {
                    fprintf(fp, delimiter "0");
                }
                
                ++c;
            }

            fprintf(fp, "\n");
            ++e;
        }

        fprintf(fp, "\n");
        ++g;
    }
}

} /* namespace perfm */

