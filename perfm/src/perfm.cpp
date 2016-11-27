#include "perfm_util.hpp"
#include "perfm_pmu.hpp"
#include "perfm_option.hpp"
#include "perfm_monitor.hpp"
#include "perfm_analyzer.hpp"
#include "perfm_top.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <locale.h>
#include <libgen.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <perfmon/pfmlib_perf_event.h>

namespace perfm {
    
inline bool perf_event_avail()
{
    return ::access("/proc/sys/kernel/perf_event_paranoid", F_OK) == 0 ? true : false;
}

void run_monitor()
{
    if (!perf_event_avail()) {
        perfm_fatal("perf_event NOT supported, exiting...\n");
    }

    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    /* TODO */

    perfm::monitor::ptr_t m = perfm::monitor::alloc();
    m->open();
    m->start();
    m->close();

    pfm_terminate();
}

void run_sampler()
{
    if (!perf_event_avail()) {
        perfm_fatal("perf_event NOT supported, exiting...\n");
    }

    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    perfm_fatal("TODO\n");

    pfm_terminate();
}

void run_analyzer()
{
    perfm_fatal("TODO\n");
}

void run_topper()
{
    if (!perf_event_avail()) {
        perfm_fatal("perf_event NOT supported, exiting...\n");
    }

    if (geteuid() != 0) {
        perfm_fatal("linux's perf_event requires root privilege to do system-wide monitor\n");
    }

    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    perfm_options.rdfmt_evgroup = true;
    perfm_options.incl_children = false;

    top::ptr_t topper = top::alloc();
    if (!topper) {
        perfm_fatal("failed to alloc top object\n");
    }

    topper->init();
    topper->open();
    topper->loop();
    topper->fini();

    pfm_terminate();
}

void run_general()
{
    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    if (perfm_options.list_pmu) {
        perfm::pmu_list();
    }

    /* TODO */

    pfm_terminate();
}

} /* namespace perfm */

int main(int argc, char **argv)
{
    if (argc < 2) {
        perfm::usage();
        exit(EXIT_SUCCESS);
    }

    perfm::perfm_options.parse(argc, argv);
    
    if (perfm::perfm_options.error) {
        perfm_fatal("invalid options, use '-h' or '--help' to display usage info.\n");
    }

    if (perfm::perfm_options.version) {
        perfm::version();
    }
    
    if (perfm::perfm_options.usage) {
        perfm::usage();
    }    

    if (perfm::perfm_options.usage || perfm::perfm_options.version) {
        exit(EXIT_SUCCESS);
    }

    setlocale(LC_ALL, "");

    perfm::perfm_options.print();

    // run 
    switch (perfm::perfm_options.perfm_switch) {
    case PERFM_MONITOR:
        perfm::run_monitor();
        break;

    case PERFM_SAMPLE:
        perfm::run_sampler();
        break;

    case PERFM_ANALYZE:
        perfm::run_analyzer();
        break;

    case PERFM_TOP:
        perfm::run_topper();
        break;

    default:
        perfm::run_general();
    }

    return 0;
}
