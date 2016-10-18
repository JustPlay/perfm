#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <locale.h>
#include <libgen.h>
#include <unistd.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"
#include "perfm_group.hpp"
#include "perfm_monitor.hpp"


int main(int argc, char **argv)
{
    if (access("/proc/sys/kernel/perf_event_paranoid", F_OK) != 0) {
        fprintf(stderr, "perf_event is NOT supported, Exiting...\n");
        exit(EXIT_FAILURE);
    }

    // Step 1. parse & validate the command line options
    if (argc < 2) {
        perfm::usage(basename(argv[0]));
        exit(EXIT_SUCCESS);
    }

    int is_args_valid = perfm::perfm_options.parse_cmd_args(argc, argv);
    
    if (is_args_valid == CMD_ARGS_USAGE) {
        perfm::usage(basename(argv[0]));
        exit(EXIT_SUCCESS);
    }
    
    if (is_args_valid == CMD_ARGS_ERROR) {
        perfm::usage(basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    if (is_args_valid == CMD_ARGS_VERSION) {
        fprintf(stderr, "perfm v1.0.0\n");
        exit(EXIT_SUCCESS);
    }


    setlocale(LC_ALL, "");

    // Step 2. initialize libpfm
    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_error("pfm_initialize(): %s\n", pfm_strerror(ret)); 
    }

    // Step 3. dump the configure for this run of perfm
    perfm::pr_pmu_list();

    perfm::perfm_options.pr_options();

    // Step 4. start the monitor
    perfm::monitor_t pm;
    pm.open();
    pm.start();
    pm.close();

    // Step X. free resources
    pfm_terminate();

    return 0;
}
