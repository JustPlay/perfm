#include "perfm_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <locale.h>
#include <libgen.h>

#include <perfmon/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

namespace {

/* 
 * this definition must be consistent with the 'perf_type_id' defined in
 *     <linux/perf_event.h> 
 * or
 *     <perfmon/perf_event.h>
 */

#define PERF_MAX PERF_TYPE_MAX

const char *ev_type[PERF_MAX] = {
    "PERF_HARDWARE",
    "PERF_SOFTWARE",
    "PERF_TRACEPOINT",
    "PERF_HW_CACHE",
    "PERF_RAW",
    "PERF_BREAKPOINT",
};

void ev2perf(const std::string &evn, FILE *fp = stdout)
{
    struct perf_event_attr hw; 
    memset(&hw, 0, sizeof(hw));

    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));

    arg.attr = &hw;
    arg.size = sizeof(arg);

    pfm_err_t ret = pfm_get_os_event_encoding(evn.c_str(), PFM_PLM3 | PFM_PLM0, PFM_OS_PERF_EVENT, &arg);
    if (ret != PFM_SUCCESS) {
        perfm_warn("%s %s\n", evn.c_str(), pfm_strerror(ret));
        return;
    }

    fprintf(fp, "- %s -\n", evn.c_str());
    fprintf(fp, "  perf.type                    : %s\n"
                "      .config                  : %zx\n"
                "  perf.disabled                : %s\n"
                "      .inherit                 : %s\n"
                "      .pinned                  : %s\n"
                "      .exclusive               : %s\n"
                "      .exclude_user            : %s\n"
                "      .exclude_kernel          : %s\n"
                "      .exclude_hv              : %s\n"
                "      .exclude_idle            : %s\n"
                "      .inherit_stat            : %s\n"
                "      .task                    : %s\n"
                "      .exclude_callchain_kernel: %s\n"
                "      .exclude_callchain_user  : %s\n"
                "      .use_clockid             : %s\n"
                "",
                ev_type[hw.type],
                hw.config,
                hw.disabled                 ? "true" : "false",
                hw.inherit                  ? "true" : "false",
                hw.pinned                   ? "true" : "false",
                hw.exclusive                ? "true" : "false",
                hw.exclude_user             ? "true" : "false",
                hw.exclude_kernel           ? "true" : "false",
                hw.exclude_hv               ? "true" : "false",
                hw.exclude_idle             ? "true" : "false",
                hw.inherit_stat             ? "true" : "false",
                hw.task                     ? "true" : "false",
                hw.exclude_callchain_kernel ? "true" : "false",
                hw.exclude_callchain_user   ? "true" : "false",
                hw.use_clockid              ? "true" : "false"
            );
    fprintf(fp, "\n");
}

bool get_event(std::istream &fp, std::string &ev)
{
    std::string line;

    while (std::getline(fp, line)) {
        ev = perfm::str_trim(line); 
        if (ev.empty() || ev[0] == '#' || ev[0] == ';') {
            continue;
        }
        return true;
    }

    return false;
}

} /* namespace */


int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    pfm_err_t ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        perfm_fatal("pfm_initialize() failed, %s\n", pfm_strerror(ret));
    }

    std::string ev;
    while (get_event(std::cin, ev)) {
        ev2perf(ev);
    }

    pfm_terminate();

    return 0;
}
