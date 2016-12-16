#include "perfm_util.hpp"
#include "perfm_pmu.hpp"
#include "perfm_option.hpp"
#include "perfm_monitor.hpp"
#include "perfm_analyzer.hpp"
#include "perfm_top.hpp"
#include "perfm_topology.hpp"

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
    
inline bool perf_event_available()
{
    return ::access("/proc/sys/kernel/perf_event_paranoid", F_OK) == 0;
}

void run_monitor()
{
    if (!perf_event_available()) {
        perfm_fatal("perf_event NOT supported, exiting...\n");
    }

    if (geteuid() != 0) {
        fprintf(stderr, "%s: linux's perf_event requires root privilege to do system-wide monitor\n", program);
        exit(EXIT_FAILURE);
    }

    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    perfm::monitor::ptr_t m = perfm::monitor::alloc();
    if (!m) {
        perfm_fatal("failed to alloc the monitor object\n");
    }

    m->init();
    m->open();
    m->start();
    m->close();

    pfm_terminate();

    perfm::topology::ptr_t topology = perfm::topology::alloc();
    if (!topology) {
        perfm_fatal("failed to alloc the topology object\n");
    }

    topology->build();
    topology->print(perfm_options.sys_topology_filp.c_str());
}

void run_sampler()
{
    if (!perf_event_available()) {
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
    perfm::analyzer::ptr_t analyzer = perfm::analyzer::alloc();
    if (!analyzer) {
        perfm_fatal("failed to alloc the analyzer object\n");
    }

    analyzer->collect("perfm.dat");
    analyzer->topology();
    analyzer->parse();

    /* TODO */
}

void run_topper()
{
    if (!perf_event_available()) {
        perfm_fatal("perf_event NOT supported, exiting...\n");
    }

    if (geteuid() != 0) {
        fprintf(stderr, "%s: linux's perf_event requires root privilege to do system-wide monitor\n", program);
        exit(EXIT_FAILURE);
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
