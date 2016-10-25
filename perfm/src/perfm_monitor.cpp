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

void monitor_t::open()
{
    /* Check the existence of the target process */
    int pid = perfm_options.pid;
    if (pid != -1) {
        if (access(std::string("/proc/" + std::to_string(pid) + "/status").c_str(), F_OK) != 0) {
            fprintf(stderr, "The target process [%d] does NOT existed, Exiting...\n", pid);        
            exit(EXIT_FAILURE);
        }
    }

    /* Check the existence of the target processor */
    int cpu = perfm_options.cpu;
    if (cpu != -1) {
        int nr_cpu_onln = sysconf(_SC_NPROCESSORS_ONLN); 
        if (cpu >= nr_cpu_onln) {
            fprintf(stderr, "The target CPU [%d] does NOT existed or online, Exiting...\n", cpu);        
            exit(EXIT_FAILURE);
        }
    }

    /* Permissions checking */
    /* TODO */

    /* Open event groups */
    auto ev_groups = perfm_options.get_event_groups(); 
    
    for (const auto &evg_list : ev_groups) {
        group_t::ptr_t grp = group_t::creat();
        grp->gr_open(evg_list, perfm_options.pid, perfm_options.cpu, perfm_options.plm); 
        grp_list.push_back(grp);                
    }

    assert(grp_list.size() == ev_groups.size());
}

void monitor_t::close()
{
    for (decltype(grp_list.size()) i = 0; i < grp_list.size(); ++i) {
        grp_list[i]->gr_close();
    }
}

void monitor_t::start() 
{
    rr();
}

void monitor_t::stop() 
{
    /* TODO */
}

int monitor_t::rr()
{
    int iter = 0;

    while (iter < perfm_options.loops) {
       
        assert(grp_list.size());
        for (size_t g = 0; g < grp_list.size(); ++g) {
            // - 1. start the monitor
            grp_list[g]->gr_start();

            // - 2. monitor for the specified time interval
            nanoseconds_sleep(perfm_options.interval);
            
            // - 3. stop the monitor
            grp_list[g]->gr_stop();

            // - 4. read/print the PMU values
            grp_list[g]->gr_read();
            grp_list[g]->gr_print();
        }
 
        ++iter;
    }

    return iter;
}

} /* namespace perfm */

