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

void top::open()
{
    _nr_total_cpu = num_cpus_total();

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

void top::print(int cpu, double usr, double sys, double idle) const
{
    FILE *fp = stderr;

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

            /* TODO */

            refresh();
        } else {
            for (size_t c = 0, n = 0; n < _nr_select_cpu && c < _nr_total_cpu; ++c) {
                if (!is_set(c)) {
                    continue;
                }

                ++n;

                cpu_ev(c)->read();
                std::vector<event::ptr_t> elist = cpu_ev(c)->elist();
                uint64_t usr_cycle = elist[0]->delta();
                uint64_t sys_cycle = elist[1]->delta();

                double usr  = 100.0 * usr_cycle / cpu_fr(c) * 1000000 * perfm_options.delay;
                double sys  = 100.0 * sys_cycle / cpu_fr(c) * 1000000 * perfm_options.delay;
                double idle = 100.0 - usr - sys;

                this->print(cpu_id(c), usr, sys, idle);
            }
        }
    }
}

//
// how TSC was computed in Intel's emon:
// 1 - TSC for a hyperthread/smt is the core/smt's maximum frequency (no turbo), this is a __constant__ value
// 2 - TSC for a core is the summary of each smt's TSC belong to this core
// 3 - TSC for a socket is the summary of each core's TSC belong to this socket
// 4 - TSC for a system is the summary of each socket's TSC belong to this system
//
// TSC = frequency = Cycles per second
//

} /* namespace perfm */
