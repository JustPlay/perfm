#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <locale.h>
#include <libgen.h>
#include <unistd.h>

#include <perfmon/pfmlib_perf_event.h>

#include "perfm_util.hpp"
#include "perfm_pmu.hpp"
#include "perfm_option.hpp"
#include "perfm_event.hpp"

namespace {

bool get_event(std::istream &fp, std::string &evn)
{
    std::string line;

    while (std::getline(fp, line)) {
        evn = perfm::str_trim(line); 
        if (evn.empty() || evn[0] == '#' || evn[0] == ';') {
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
        perfm_error("pfm_initialize(): %s\n", pfm_strerror(ret)); 
    }

    std::string evn;
    while (get_event(std::cin, evn)) {
        perfm::ev2perf(evn);            
    }

    pfm_terminate();

    return 0;
}
