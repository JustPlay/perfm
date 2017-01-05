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

void monitor::init()
{
    /* for now, do nothing */
}

void monitor::open()
{
    // 
    // FIXME:
    //   how to handle uncore PMU event ?
    //
    this->_nr_usable_cpu = num_cpu_usable();
    this->_nr_select_cpu = 0;

    memset(this->_cpu_list, 0, sizeof(this->_cpu_list));

    // alloc memory for each cpu
    //
    // FIXME:
    //   for now, we assume the processor's id is always less than _nr_usable_cpu
    //   processor's id can be obtained from /proc/cpuinfo or /sys/devices/system/cpu
    try {
        this->_cpu_data = new _pmu_dat_t[this->_nr_usable_cpu]; 
        this->_ev_group = new _e_group_t[perfm_options.nr_group()];
    } catch (const std::bad_alloc &e) {
        perfm_fatal("failed to alloc memory, %s\n", e.what());
    }

    // parse cpu list specified by user
    parse_cpu_list(perfm_options.cpu_list);

    // check the validness of the user provided pid
    int pid = perfm_options.pid;
    if (pid != -1 && !pid_exist(pid)) {
        perfm_fatal("process %d does not existed\n", pid);
    }

    // TODO:
    //   for now, we require root privilege
    if (::geteuid() != 0) {
        perfm_fatal("perfm monitor requires root privilege to run\n");
    }

    // open event groups for each selected CPUs
    for (size_t g = 0; g < perfm_options.nr_group(); ++g) {
        _ev_group[g] = str_split(perfm_options.egroups[g], ",");

        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < NR_MAX_PROCESSOR; ++c) {
            if (!is_set(c)) {
                continue;
            }
            ++n;

            group::ptr_t group = group::alloc();
            if (!group) {
                perfm_fatal("failed to alloc group object\n");
            }

            // FIXME: 
            //   the pid & cpu argument
            group->open(_ev_group[g], perfm_options.pid, c);

            _cpu_data[c].push_back(group);
        }
    }
}

void monitor::close()
{
    for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < NR_MAX_PROCESSOR; ++c) {
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

int monitor::loop()
{
    double random_interval = perfm_options.interval / 100; /* random interval in seconds */

    std::random_device rd;
    std::default_random_engine e(rd());

    std::uniform_real_distribution<double> dis(-random_interval, random_interval);

    int n = perfm_options.loops;
    int r = 0;

    while (!should_quit && r < n) {
        rr(perfm_options.interval - dis(e));
        ++r;
    }

    return r;
}

void monitor::rr(double second)
{
    size_t nr_group = perfm_options.nr_group();
    uint64_t tsc_prev;
    uint64_t tsc_curr;

    for (size_t g = 0; g < nr_group; ++g) {
        tsc_curr = read_tsc();

        // start
        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < NR_MAX_PROCESSOR; ++c) {
            if (!is_set(c)) {
                continue;
            }
            ++n;

            _cpu_data[c][g]->start();
        }

        nanosecond_sleep(second);

        tsc_prev = tsc_curr;
        tsc_curr = read_tsc();

        // stop
        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < NR_MAX_PROCESSOR; ++c) {
            if (!is_set(c)) {
                continue;
            } 
            ++n;
            
            _cpu_data[c][g]->stop();
        }

        // read
        for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < NR_MAX_PROCESSOR; ++c) {
            if (!is_set(c)) {
                continue;
            } 
            ++n;
            
            _cpu_data[c][g]->read();
        }
    }
}

void monitor::print(size_t g, uint64_t tsc_cycles) const
{
    // 
    // FIXME:
    //   how to handle uncore PMU event ?
    //
    #define delimiter " "
    #define is_first(x) (x) == 0

    FILE *fp = perfm_options.fp_out ? perfm_options.fp_out : stdout;

    // event_name, tsc_cycles, cpu0, cpu1, cpu2, cpu3, ...
    // event_name, tsc_cycles, cpu0, cpu1, cpu2, cpu3, ...
    //
    // event_name, tsc_cycles, sockek0, socket1, ...
    // event_name, tsc_cycles, sockek0, socket1, ...

    for (size_t e = 0, n = _ev_group[g].size(); e < n; ++e) {
        /* FIXME */
        
        if (thread_level_event()) {
            for (unsigned int c = 0; c < _nr_usable_cpu; ++c) {
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
        } else if (socket_level_event()) {
            /* TODO */        
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}

void monitor::parse_cpu_list(const std::string &list)
{
    // if @list empty, select all online CPUs
    if (list.empty()) {
        for (unsigned int c = 0, n = 0; n < _nr_usable_cpu && c < NR_MAX_PROCESSOR; ++c) {
            if (!cpu_exist(c)) {
                continue;
            }

            ++n;

            if (cpu_online(c)) {
                ++_nr_select_cpu;
                do_set(c);
            }
        }

    // parse @list and eliminate the non-exist & off-line CPUs
    } else {
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

                if (!cpu_exist(c) || !cpu_online(c)) {
                    perfm_warn("cpu %d does not exist/online, ignored\n", c);
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
                    if (!cpu_exist(c) || !cpu_online(c)) {
                        perfm_warn("cpu %d does not exist/online, ignored\n", c);
                        continue;
                    }

                    ++_nr_select_cpu;
                    do_set(c);
                }
            }
        }
    }
}

} /* namespace perfm */
