
#include "perfm_parser.hpp"
#include "perfm_util.hpp"

#ifndef NOT_USE_GLOB
#include <glob.h>
#endif

#include <string>
#include <vector>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include "linux/perf_event.h"

namespace {

/* 
 * the following definition must be consistent with `perf_type_id` defined in <linux/perf_event.h> 
 */

#define EVENT_SOURCE_MAX PERF_TYPE_MAX

const char *pmu_type_list[EVENT_SOURCE_MAX + 1] = {
    "pmu_hardware",
    "pmu_software",
    "pmu_tracepoint",
    "pmu_hw_cache",
    "pmu_raw",
    "pmu_breakpoint",
    "pmu_unknown"
};

} /* namespace */

namespace perfm {

parser::ptr_t parser::alloc()
{
    parser *p = nullptr;

    try {
        p = new parser;
    } catch (const std::bad_alloc &) {
        p = nullptr;
    }

    return ptr_t(p);
}

long parser::parse_event_source(const std::string &esrc)
{
    if (!_already_initialized) {
        parse_event_source();
    }

    auto it = _event_source_list.find(esrc);
    if (it != _event_source_list.end()) {
        return it->second;
    } else {
        return -1;
    }
}

#ifndef NOT_USE_GLOB

void parser::parse_event_source()
{
    glob_t globuf;

    int stat = glob("/sys/bus/event_source/devices/*/type", 0, NULL, &globuf);
    switch (stat) {
    case 0:   
        break;
    
    case GLOB_NOSPACE:
        perfm_fatal("running out of memory\n");
    
    case GLOB_ABORTED:
        perfm_fatal("read error\n");

    case GLOB_NOMATCH:
        perfm_fatal("no found matches\n");

    default:
        perfm_fatal("unknown error\n");
    }

    _event_source_list.clear();

    char esrc[256];

    for (size_t i = 0; i < globuf.gl_pathc; ++i) {
        if (sscanf(globuf.gl_pathv[i], "/sys/bus/event_source/devices/%[^/]/type", esrc) != 1) {
            perfm_warn("invalid %s\n", globuf.gl_pathv[i]);
            continue;
        }
        
        int type;

        std::fstream fp(globuf.gl_pathv[i], std::ios::in);
        if (!fp || !(fp >> type)) {
            perfm_warn("failed to open or read %s\n", globuf.gl_pathv[i]);
            continue;
        }

        _event_source_list.insert({std::move(std::string(esrc)), type});
    }

    _already_initialized = true;

    globfree(&globuf);
}

#else

void parser::parse_event_source()
{
    _event_source_list.clear();

    const std::string path("/sys/bus/event_source/devices/");

    DIR *dirp = ::opendir(path.c_str());
    if (!dirp) {
        perfm_fatal("failed to opendir %s\n", path.c_str());
    }

    struct dirent *dp = nullptr;
    errno = 0;

    while ((dp = ::readdir(dirp)) != NULL) {
        if (dp->d_type != DT_DIR && dp->d_type != DT_LNK) {
            continue;
        }

        if (std::strncmp(dp->d_name, ".", 1) == 0 || std::strncmp(dp->d_name, "..", 2) == 0) {
            continue;
        }

        std::string esrc_path = path + dp->d_name;   // PMU's directory
        std::string type_path = esrc_path + "/type"; // PMU's type file

        if (!file_exist(type_path.c_str())) {
            continue;
        }

        int type;

        std::fstream fp(type_path, std::ios::in);
        if (!fp || !(fp >> type)) {
            perfm_warn("failed to open or read %s\n", type_path.c_str());
            continue;
        }

        _event_source_list.insert({std::move(std::string(dp->d_name)), type});
    }

    if (errno) {
        perfm_warn("failed (may be partial successfully) to read dir %s\n", path.c_str());
    }

    _already_initialized = true;
}

#endif

void parser::parse_encoding_format()
{
    for (auto it = _event_source_list.begin(); it != _event_source_list.end(); ++it) {
        parse_encoding_format(it->first);
    }
}

void parser::parse_encoding_format(const std::string &pmu)
{
    const std::string path = "/sys/bus/event_source/devices/" + pmu + "/format/";

    // generic perf event do *not* has the format directory
    // e.g. breakpoint, software, tracepoint
    if (!file_exist(path.c_str())) {
        return;        
    }

    DIR *dirp = ::opendir(path.c_str());
    if (!dirp) {
        perfm_fatal("failed to opendir %s\n", path.c_str());
    }

    struct dirent *dp = NULL;
    errno = 0;

    _event_format_t format;

    while ((dp = ::readdir(dirp)) != NULL) {
        if (dp->d_type != DT_REG) {
            continue;
        }

        if (std::strncmp(dp->d_name, ".", 1) == 0 || std::strncmp(dp->d_name, "..", 2) == 0) {
            continue;
        }

        std::string nam(dp->d_name); 
        std::string fmt;

        std::fstream fp(path + nam, std::ios::in);
        if (!fp || !(fp >> fmt)) {
            perfm_fatal("failed to open/read %s\n", (path + nam).c_str());
        }

        auto slice = str_split(fmt, ":", 2);

        int config, lowbit, higbit;

        if (slice[0] == "config") {
            config = PERF_CONFIG_0; 
        } else if (slice[0] == "config1") {
            config = PERF_CONFIG_1; 
        } else if (slice[0] == "config2") {
            config = PERF_CONFIG_2; 
        } else {
            perfm_fatal("invalid format %s\n", fmt.c_str());
        }

        /* FIXME */
        auto del = slice[1].find("-");
        if (del != std::string::npos) {
            try {
                lowbit = std::stoi(slice[1]);
                higbit = std::stoi(slice[1].substr(del + 1));
            } catch (const std::exception &) {
                perfm_fatal("invalid format %s\n", fmt.c_str());
            }
        } else {
            try {
                lowbit = std::stoi(slice[1]);
                higbit = lowbit;
            } catch (const std::exception &) {
                perfm_fatal("invalid format %s\n", fmt.c_str());
            }
        }

        format.insert({std::move(nam), std::make_tuple(config, lowbit, higbit)});
    }

    if (errno) {
        perfm_warn("failed (may be partial successfully) to read dir %s\n", path.c_str());
    }

    _event_format_list.insert({pmu, std::move(format)}); 
}

void parser::print() const
{
    FILE *fp = stdout; 
    
    #define pr(fmt, ...) fprintf(fp, fmt, ##__VA_ARGS__)

    pr("Total %lu PMUs available (powered by linux's perf_event)\n", _event_source_list.size()); 
    for (const auto &pmu : _event_source_list) {
        pr("  %-15s: %s(type: %d)\n", pmu.first.c_str(), pmu_type_list[pmu.second], pmu.second); 
    }
    pr("\n");

    const char *config[] = {
        "config",
        "config1",
        "config2"
    };

    for (const auto &pmu : _event_source_list) {
        pr("Encoding format for PMU %s(%s, %d):\n", pmu.first.c_str(), pmu_type_list[pmu.second], pmu.second);

        auto fmt = _event_format_list.find(pmu.first);
        if (fmt == _event_format_list.end()) {
            switch (pmu.second) {
            case PERF_TYPE_HARDWARE:
            case PERF_TYPE_SOFTWARE:
            case PERF_TYPE_HW_CACHE:
            case PERF_TYPE_BREAKPOINT:
            case PERF_TYPE_TRACEPOINT:
                pr("  kernel generalized pmu, no format info\n");
                pr("\n");
                continue;
            }

            perfm_warn("invalid pmu %s\n", pmu.first.c_str());
            continue;
        }
        
        for (auto it = fmt->second.begin(); it != fmt->second.end(); ++it) {
            pr("  %-15s %s:%d-%d\n", it->first.c_str(), config[std::get<0>(it->second)], std::get<1>(it->second), std::get<2>(it->second));
        }

        pr("\n");
    }
}


} /* namespace perfm */

int main(int argc, char **argv)
{
    perfm::parser::ptr_t parser = perfm::parser::alloc();

    parser->parse_event_source();
    parser->parse_encoding_format();

    parser->print();
}
