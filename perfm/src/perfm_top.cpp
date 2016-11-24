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

#include <unistd.h>

namespace perfm {

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

void top::open(const std::string &cpulist)
{
    _nr_total_cpu = num_cpus_total();

    try {
        this->_cpu_data = new cpu_data_t[_nr_total_cpu];
    } catch (const std::bad_alloc &e) {
        perfm_fatal("failed to alloc memory, %s\n", e.what());
    }

    // parse cpu list, if empty, select all available cpus
    if (cpulist.empty()) {
        for (unsigned int cpu = 0; cpu < _nr_total_cpu; ++cpu) {
            do_set(cpu);
            ++_nr_select_cpu;
        }
    } else {
        perfm_fatal("TODO\n");
    }

    auto freq_list = cpu_frequency();

    for (const auto &f : freq_list) {
        fprintf(stderr, "%2d - %d\n", f.first, f.second); 
    }

    return;
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
        cpu_ev(c)->close();
    }
}

void top::print(int cpu, double usr, double sys, double idle) const
{
    FILE *fp = stdout;

    if (perfm_options.batch_mode) {
        fprintf(fp, "Cpu%-2d - usr: %.2lf%%, sys: %.2lf%%, idle: %.2lf%%\n", cpu, usr, sys, idle); 
    } else {
        printw("Cpu%-2d - usr: %.2lf%%, sys: %.2lf%%, idle: %.2lf%%\n", cpu, usr, sys, idle); 
    }
}

void top::loop()
{
    int max = perfm_options.max <= 0 ? INT_MAX : perfm_options.max;

    FILE *fp = stdout;

    // start counting ...
    for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (is_set(c)) {
            cpu_ev(c)->start();
            ++n;
        }
    }

    // skip the first few ...
    for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
        if (is_set(c)) {
            cpu_ev(c)->read();
            ++n;
        }
    }

    // display ...
    while (max--) {
        nanoseconds_sleep(perfm_options.delay); 

        if (!perfm_options.batch_mode) {
            move(0, 0);

            for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
                if (!is_set(c)) {
                    continue;
                }

                ++n;

                cpu_ev(c)->read();
                std::vector<event::ptr_t> elist = cpu_ev(c)->elist();
                uint64_t usr_cycle = elist[0]->scale();
                uint64_t sys_cycle = elist[1]->scale();

                double usr  = 100.0 * usr_cycle / cpu_fr(c) * 1000000 * perfm_options.delay;
                double sys  = 100.0 * sys_cycle / cpu_fr(c) * 1000000 * perfm_options.delay;
                double idle = 100.0 - usr - sys;

                this->print(cpu_id(c), usr, sys, idle);
            }

            refresh();
        } else {
            /* TODO */
        }
    }
}

} /* namespace perfm */
