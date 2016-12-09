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

monitor::~monitor()
{
    if (_cpu_data) {
        delete[] _cpu_data;
    }

    if (_ev_group) {
        delete[] _ev_group;
    }
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

    memset(this->_cpu_list, 0, sizeof(this->_cpu_list));

    // alloc memory for each cpu
    try {
        this->_cpu_data = new _cpu_data_t[this->_nr_total_cpu]; 
        this->_ev_group = new _ev_group_t[this->_nr_total_cpu];
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
    }

    int pid = perfm_options.pid;
    if (pid != -1 && !pid_exist(pid)) {
        perfm_fatal("process %d does not existed, exiting...\n", pid);
    }

    /* TODO (permissions checking) */

    for (size_t g = 0; g < perfm_options.nr_group(); ++g) {
        _ev_group[g] = str_split(perfm_options.egroups[g], ",");

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) { 
            if (!is_set(c)) {
                continue;
            }

            ++n;

            group::ptr_t group = group::alloc();
            if (!group) {
                perfm_fatal("failed to alloc group object\n");
            }

            /* FIXME (the pid & cpu argument) */
            group->open(_ev_group[g], perfm_options.pid, c);

            _cpu_data[c].push_back(group);
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
    uint64_t tsc_prev;
    uint64_t tsc_curr;

    for (size_t g = 0; g < nr_group; ++g) {
        tsc_curr = read_tsc();

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            }

            ++n;

            _cpu_data[c][g]->start();
        }

        nanosecond_sleep(second);

        tsc_prev = tsc_curr;
        tsc_curr = read_tsc();

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            } 

            ++n;
            
            _cpu_data[c][g]->read();
        }

        print(g, tsc_curr - tsc_prev);

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
            if (!is_set(c)) {
                continue;
            } 

            ++n;

            _cpu_data[c][g]->stop();
        }
    }
}

int monitor::loop()
{
    std::random_device rd;
    std::default_random_engine e(rd());
    std::uniform_real_distribution<double> dis(-0.005, 0.005); /* [-5ms, 5ms) */

    int n = perfm_options.loops;
    int r = 0;

    while (!should_quit && r < n) {
        rr(perfm_options.interval - dis(e));
        ++r;
    }

    return r;
}

void monitor::print(size_t g, uint64_t tsc_cycles) const
{
    #define delimiter " "
    #define is_first(cpu) (cpu) == 0

    FILE *fp = perfm_options.fp_out ? perfm_options.fp_out : stdout; 
    
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    //
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...
    // EventName, TSC Cycles, CPUa, CPUb, CPUc, CPUd, ...

    for (size_t e = 0, n = _ev_group[g].size(); e < n; ++e) {
        for (unsigned int c = 0; c < _nr_total_cpu; ++c) {
            if (is_first(c)) {
                fprintf(fp, "%s" delimiter "%zu", _ev_group[g][e].c_str(), tsc_cycles);
            }

            if (is_set(c)) {
                event::ptr_t event = _cpu_data[c][g]->fetch_event(e);
                if (event->name() != _ev_group[g][e]) {
                    perfm_fatal("event group must be consistent between all selected cpus\n");
                }
                fprintf(fp, delimiter "%lu", event->delta());
            } else {
                fprintf(fp, delimiter "0");
            }
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}

} /* namespace perfm */
