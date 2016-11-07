#include "perfm_util.hpp"
#include "perfm_pmu.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_evgrp.hpp"
#include "perfm_monitor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <locale.h>
#include <libgen.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <perfmon/pfmlib_perf_event.h>


int main(int argc, char **argv)
{
    if (access("/proc/sys/kernel/perf_event_paranoid", F_OK) != 0) {
        fprintf(stderr, "perf_event is NOT supported, Exiting...\n");
        exit(EXIT_FAILURE);
    }

    // Step 1. parse & validate the command line options
    if (argc < 2) {
        perfm::usage();
        exit(EXIT_SUCCESS);
    }

    int is_args_valid = perfm::perfm_options.parse_cmd_args(argc, argv);
    
    switch (is_args_valid) {
    case COMM_OPTIONS_USAGE:
        perfm::usage();
        exit(EXIT_SUCCESS);
        
    case COMM_OPTIONS_VERSION:
        perfm::version();
        exit(EXIT_SUCCESS);

    case COMM_OPTIONS_ERROR:
        fprintf(stderr, "Invalid arguments, use '-h' or '--help' to display usage info.\n");
        exit(EXIT_FAILURE);
    }


    setlocale(LC_ALL, "");

    // Step 2. initialize libpfm
    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret)); 
    }

    // Step 3. dump the configure for this run of perfm
    if (perfm::perfm_options.list_pmu_avail) {
        perfm::pr_pmu_list();
    }

    perfm::perfm_options.pr_options();

    // Step 4. start the monitor
    switch (perfm::perfm_options.running_mode)
    {
        case PERFM_RUNNING_MODE_MONITOR: 
        {
            perfm::monitor_t pm;
            pm.open();
            pm.start();
            pm.close();
        }
            break;

        case PERFM_RUNNING_MODE_SAMPLE:
        {
            /* TODO */                      
        }
            break;

        case PERFM_RUNNING_MODE_ANALYZE:
        {
            /* TODO */
        }
            break;

        default:
            ;
    }

    // Step 5. free resources
    pfm_terminate();

    return 0;
}
