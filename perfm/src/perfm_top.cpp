#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_top.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <new>
#include <random>

#include <unistd.h>
#include <signal.h>

namespace {

volatile sig_atomic_t should_quit = 0; // SIGINT
volatile sig_atomic_t resize_wsiz = 0; // SIGWINCH

void sighandler(int signo)
{
    switch (signo) {
    case SIGINT:
        should_quit = 1;
        break;

    case SIGWINCH:
        /* TODO */
        break;
    }
}

} /* namespace */

namespace perfm {

// The rdtsc instruction is used to move the 64-bit TSC counter into the registers EDX:EAX.
// Below is a piece of code that will return the 64-bit wide counter value.s
// Note that this should work on both i386 and x86_64 architectures.
unsigned long long int rdtsc()
{
    unsigned eax, edx;

    __asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));

    return ((unsigned long long)eax) | (((unsigned long long)edx) << 32);
}

top::ptr_t top::alloc()
{
    top *topper = nullptr;

    try {
        topper = new top;
    } catch (const std::bad_alloc &) {
        topper = nullptr;
    }

    return ptr_t(topper);
}

void top::init()
{
    // setup signal handlers
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = sighandler;

    if (sigaction(SIGINT, &act, NULL) != 0) {
        perfm_warn("failed to install handler for SIGINT\n");
    }

    // setup display screen 
    if (!perfm_options.batch_mode) {
        if (tcgetattr(0, &this->_termios) != 0) {
            perfm_fatal("failed to get terminal's attribute\n");
        }
        
        struct winsize ws;
        if (ioctl(1, TIOCGWINSZ, &ws) != 0) {
            perfm_fatal("failed to get terminal's window size\n");
        }

        this->_term_row = ws.ws_row > this->_term_row ? ws.ws_row : this->_term_row;
        this->_term_col = ws.ws_col > this->_term_col ? ws.ws_col : this->_term_col;

        initscr();
        nocbreak();
        resize_term(this->_term_row, this->_term_col);
    }
}

void top::fini()
{
    if (perfm_options.batch_mode) {
        return;
    } 

    endwin();
    tcsetattr(0, TCSAFLUSH, &this->_termios);
}

void top::open()
{
    _nr_total_cpu  = num_cpus_total();
    _nr_select_cpu = 0;

    memset(_cpu_list, 0, sizeof(_cpu_list));

    try {
        this->_cpu_data = new cpu_data_t[_nr_total_cpu];
    } catch (const std::bad_alloc &e) {
        perfm_fatal("failed to alloc memory, %s\n", e.what());
    }

    // parse cpu list, if empty, select all available cpus
    if (perfm_options.cpu_list.empty()) {
        for (unsigned int cpu = 0; cpu < _nr_total_cpu; ++cpu) {
            do_set(cpu);
            ++_nr_select_cpu;
        }
    } else {
        parse_cpu_list(perfm_options.cpu_list);
    }

    // get the frequency for each online cpu
    auto freq_list = cpu_frequency();

    // open event for each selected cpu
    for (unsigned int c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (!is_set(c)) {
            continue;
        } 

        ++n;

        int freq = 0;

        auto it = freq_list.find(c);
        if (it == freq_list.end()) {
            perfm_warn("freq of cpu %d unknown, use freq of cpu 0 instead\n", c);
            freq = freq_list[0];
        } else {
            freq = it->second;
        }

        group::ptr_t g = group::alloc();
        if (!g) {
            perfm_warn("failed to alloc group object\n");
            continue;
        }

        g->open(_ev_list, -1, c);

        _cpu_data[c] = std::make_tuple(c, freq, g); 
    } 
}

void top::close()
{
    for (unsigned int c = 0, n = 0; c < _nr_total_cpu && n < _nr_select_cpu; ++c) {
        if (!is_set(c)) {
            continue;
        }
        
        ++n;
        cpu_pmu(c)->close();
    }
}

void top::parse_cpu_list(const std::string &list)
{
    std::vector<std::string> slice = str_split(list, ",");

    for (size_t i = 0; i < slice.size(); ++i) {
        if (slice[i].empty()) {
            continue;
        }

        size_t del_pos = slice[i].find("-");

        if (del_pos == std::string::npos) {
            int cpu;
            try {
                cpu = std::stoi(slice[i]);
            } catch (const std::exception &e) {
                perfm_warn("%s %s\n", e.what(), slice[i].c_str());
                continue;
            }

            ++_nr_select_cpu;
            do_set(cpu);
        } else {
            int from, to;
            try {
                from = std::stoi(slice[i]);
                to   = std::stoi(slice[i].substr(del_pos + 1));
            } catch (const std::exception &e) {
                perfm_warn("%s %s\n", e.what(), slice[i].c_str());
                continue;
            }

            for (int cpu = from; cpu <= to; ++cpu) {
                ++_nr_select_cpu;
                do_set(cpu);
            }
        }
    }
}

void top::print(double seconds) const
{
    for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (!is_set(c)) {
            continue;
        }
        ++n;
    
        uint64_t delta_cycle = seconds * cpu_mhz(c) * 1000000;

        cpu_pmu(c)->read();
        std::vector<event::ptr_t> elist = cpu_pmu(c)->elist();
        uint64_t usr_cycle_delta = elist[U_CYCLE_EID]->delta();
        uint64_t sys_cycle_delta = elist[K_CYCLE_EID]->delta();

        double usr  = 100.0 * usr_cycle_delta / delta_cycle;
        double sys  = 100.0 * sys_cycle_delta / delta_cycle;

        usr = usr > 100 ? 100 : usr;
        sys = sys > 100 ? 100 : sys;

        double idle = usr + sys > 100 ? 0 : 100 - usr - sys;

        print(cpu_num(c), cpu_mhz(c) / 1000.0, usr, sys, idle);
    }
}

void top::print(int cpu, double freq, double usr, double sys, double idle) const
{
    FILE *fp = stderr;

    if (perfm_options.batch_mode) {
        fprintf(fp, "Cpu%-2d : %.1fGHz,  usr: %5.1f%%,  sys: %5.1f%%,  idle: %5.1f%%\n", cpu, freq, usr, sys, idle); 
    } else {
        printw(     "Cpu%-2d : %.1fGHz,  usr: %5.1f%%,  sys: %5.1f%%,  idle: %5.1f%%\n", cpu, freq, usr, sys, idle); 
    }
}

void top::loop()
{
    int iter = perfm_options.iter <= 0 ? INT_MAX : perfm_options.iter;

    // start counting ...
    for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (is_set(c)) {
            cpu_pmu(c)->start();
            ++n;
        }
    }

    // skip the first few ...
    for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (!is_set(c)) {
            continue;
        }
        ++n;
    
        cpu_pmu(c)->read();
    }

    // 1. std::random_device is a uniformly-distributed __integer random number generator__ 
    //    that produces non-deterministic random numbers.
    // 2. std::random_device may be implemented in terms of an implementation-defined pseudo-random
    //    number engine if a non-deterministic source (e.g. a hardware device) is not available to the
    //    implementation. In this case each std::random_device object may generate the same number sequence.  
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-0.01, 0.01); /* [-10ms, 10ms) */

    // display ...
    while (iter-- && !should_quit) {
        double seconds = perfm_options.delay - dis(gen);
        nanosecond_sleep(seconds); 

        if (!perfm_options.batch_mode) {
            move(0, 0);
            print(seconds);
            refresh();
        } else {
            print(seconds);
        }
    }
}

} /* namespace perfm */
