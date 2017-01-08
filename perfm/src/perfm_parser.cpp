
#include "perfm_json.hpp"
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

// 
// #include <> /* only search the std include path */ 
// #include "" /* (1) cwd + (2) std */  
// 
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

long parser::pmu_detect(const std::string &pmu)
{
    if (!_pmu_source_detected) {
        pmu_detect();
    }

    auto it = _event_source_list.find(pmu);
    if (it != _event_source_list.end()) {
        return it->second;
    } else {
        return -1;
    }
}

#ifndef NOT_USE_GLOB

void parser::pmu_detect()
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

    globfree(&globuf);

    detect_encode_format();

    _pmu_source_detected = true;
}

#else

void parser::pmu_detect()
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

    detect_encode_format();

    _pmu_source_detected = true;
}

#endif

void parser::detect_encode_format()
{
    for (auto it = _event_source_list.begin(); it != _event_source_list.end(); ++it) {
        detect_encode_format(it->first);
    }
}

void parser::detect_encode_format(const std::string &pmu)
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

bool parser::parse_event(const std::string &raw, struct perf_event_attr *hw)
{
    // 1. raw event descriptor  --> perf style descriptor
    // 2. perf style descriptor --> perf_event_attr

    std::string p;

    if (!parse_raw_event(raw, p) || !parse_perf_event(p, hw)) {
        return false;
    }

    return true;
}

bool parser::parse_perf_event(const std::string &e, struct perf_event_attr *hw)
{
    // linux/tools/perf/Documentation/{perf-record.txt, perf-stat.txt}
    //
    // 1. a symbolic event name (use 'perf list' to list all events)
    //
    // 2. a raw PMU event (eventsel+umask) in the form of rNNN where NNN is hexadecimal event descriptor. 
    //
    // 3. a symbolically formed PMU event like 'pmu/param1=0x3,param2/' where param1', 'param2', etc are
    //    defined as formats for the PMU in sys/bus/event_source/devices/<pmu>/format/*
    //
    // 4. a symbolically formed event like 'pmu/config=M,config1=N,config3=K/' where M, N, K are 
    //    numbers (in decimal, hex, octal format)
    // 
    // FIXME:
    //     for now, we only focus on 2 & 3

    if (e.empty() || !hw) {
        return false;
    }

    memset(hw, 0, sizeof(struct perf_event_attr));

    // cache hit
    auto it = _p2a_cache.find(e); 
    if (it != _p2a_cache.end()) {
        memmove(hw, it->second, sizeof(struct perf_event_attr));    
        return true;
    }

    // cache miss
    hw->size = sizeof(struct perf_event_attr);
    hw->type = PERF_TYPE_RAW; /* defaults to PERF_TYPE_RAW */

    int modifier_pos = 0;

    // 
    // std::string const s("Emplary");
    // assert(s.size() == std::strlen(s.c_str()));
    // assert(std::equal(s.begin(), s.end(), s.c_str()));
    // assert(std::equal(s.c_str(), s.c_str() + s.size(), s.begin()));
    // assert(0 == *(s.c_str() + s.size()));
    //
    if (std::sscanf(e.c_str(), "r%zx%n", &hw->config, &modifier_pos) == 1) {
        if (modifier_pos > e.size()) {
            perfm_warn("this should not happen\n");
            return false;
        }

        if (modifier_pos == e.size()) {
            return true;
        }

        if (e[modifier_pos] == ':') {
            return parse_modifier(hw, e.substr(modifier_pos + 1));
        }

    } else {
        // FIXME:
        //    how to handle modifier ?
        auto slice = str_split(e, "/", 2);
        if (slice.size() != 2) {
            perfm_warn("invalid descriptor %s\n", e.c_str());
            return false;
        }

        if (!parse_encoding(hw, slice[0], slice[1])) {
            return false;
        }
    }

    auto hw_copy = static_cast<struct perf_event_attr *>(calloc(1, sizeof(struct perf_event_attr)));
    if (hw_copy) {
        memmove(hw_copy, hw, sizeof(struct perf_event_attr));
        _p2a_cache.insert({e, hw_copy});
    } else {
        perfm_warn("memory allocate failed, will not do cache for perf event %s\n", e.c_str());
    }

    return true;
}

bool parser::parse_raw_event(const std::string &r, std::string &p)
{
    if (r.empty()) {
        return false;
    }

    // cache hit
    auto it = _r2p_cache.find(r);    
    if (it != _r2p_cache.end()) {
        p = it->second;
        return true;
    }

    // cache miss
    /* TODO */
    // 1. find a event `descriptor`
    // 2. use this `descriptor` to init @p

    return true;
}

bool parser::parse_modifier(struct perf_event_attr *hw, const std::string &modifier) const
{   // 
    // FIXME: 
    //     for now, we only support 'K', 'U', 'H', 'P', all the other will be ingored
    //
    if (modifier.empty()) {
        return true;
    } 

    if (!hw) {
        return false;
    }

    for (size_t i = 0; i < modifier.size(); ++i) {
        switch (modifier[i]) {
        case 'K': case 'k': // kernel, don't count user
            hw->exclude_user = 1;
            break;

        case 'U': case 'u': // user, don't count kernel
            hw->exclude_kernel = 1;
            break;

        case 'H': case 'h': // hypervisor, don't count in guest
            hw->exclude_guest = 1;
            break;

        case 'P': case 'p': // skid constraint
            hw->precise_ip++;
            break;

        default:
            perfm_warn("ignored unknown modifier '%c'\n", modifier[i]);
        }
    }

    return true;
}

std::string parser::pmu_name(int type) const
{
    for (auto it = _event_source_list.begin(); it != _event_source_list.end(); ++it) {
        if (it->second == type) {
            return it->first;
        }
    }

    return "";
}

bool parser::parse_pmu_type(struct perf_event_attr *hw, const std::string &pmu) const
{
    if (pmu.empty() || !hw) {
        return false;
    }

    if (!_pmu_source_detected) {
        perfm_warn("you should run pmu_detect() first\n");
        return false;
    }

    auto it = _event_source_list.find(pmu);
    if (it != _event_source_list.end()) {
        hw->type = it->second;
        return true;
    } else {
        perfm_warn("invalid pmu %s\n", pmu.c_str());
        return false;
    }
}

#define LSB(v, x) ((v) & ((1UL << (x)) - 1)) // fetch the least significant bit

bool parser::parse_encoding(struct perf_event_attr *hw, const std::string &pmu, const std::string &e) const
{
    if (!parse_pmu_type(hw, pmu) || e.empty() || !hw) {
        return false;
    }

    std::string evn;

    auto p = e.find_first_of("\n"); 
    if (p != std::string::npos) {
        evn = e.substr(0, p);
    } else {
        evn = e;
    }

    if (evn.empty()) {
        return false;
    }

    auto pmu_format = _event_format_list.find(pmu);
    if (pmu_format == _event_format_list.end()) {
        perfm_warn("invalid pmu %s\n", pmu.c_str()); 
        return false;
    }

    std::string term;
    uint64_t value, *config;
    int l, r;

    auto slice = str_split(evn, ",");

    for (size_t i = 0; i < slice.size(); ++i) {
        auto equal = slice[i].find("=");
        if (equal != std::string::npos) {
            term = slice[i].substr(0, equal); 
            try {
                // If the value of base is 0, the numeric base is auto-detected:
                // - if the prefix is 0, the base is octal
                // - if the prefix is 0x or 0X, the base is hexadecimal
                // - otherwise the base is decimal
                value = std::stoull(slice[i].substr(equal + 1), nullptr, 0); 
            } catch (const std::exception &) {
                perfm_warn("invalid descriptor %s(%s)\n", evn.c_str(), slice[i].c_str()); 
            }
        } else {
            term  = slice[i];
            value = 1;    
        }

        auto it = pmu_format->second.find(term);
        if (it == pmu_format->second.end()) {
            perfm_warn("invalid format field %s\n", term.c_str());
            return false;
        }

        l = std::get<1>(it->second);
        r = std::get<2>(it->second);

        switch (std::get<0>(it->second)) {
        case PERF_CONFIG_0:
            config = &hw->config;
            break;

        case PERF_CONFIG_1:
            config = &hw->config1;
            break;

        case PERF_CONFIG_2:
            config = &hw->config2;
            break;
        }

        *config |= (LSB(value, r - l + 1) << l);
    }

    return true;
}

void parser::load_event_description(const std::string &thread_filp, const std::string &socket_filp, bool apppend)
{
    if (thread_filp.empty() && socket_filp.empty() && !append) {
        perfm_fatal("please specify a json file which describes the pmu events\n");
    }

    if (!thread_filp.empty()) {
        load_thread_event_description(thread_filp, append);
    }

    if (!socket_filp.empty()) {
        load_socket_event_description(socket_filp, append);
    }
}

void parser::load_thread_event_description(const std::string &json_filp, bool append)
{
    if (json_filp.empty() && !append) {
        perfm_fatal("please specify a json file which describes the pmu events\n");
    }

    if (!append) {
        // do some cleaning work 
    }

    // Define the root of the property tree
    property_tree::ptree ptree;

    // Read JSON from a the given file and translate it to a property tree.
    // - Clears existing contents of property tree. In case of error the property tree unmodified.
    // - Items of JSON arrays are translated into ptree keys with empty names.
    //   Members of objects are translated into named keys.
    // - JSON data can be a string, a numeric value, or one of literals "null", "true" and "false".
    //   During parse, any of the above is copied verbatim into ptree data string.
    try {
        json::read_json(json_filp, ptree);
    } catch (const json::json_parser_error &e) {
        perfm_fatal("%s\n", e.what());
    } catch (const std::exception &e) {
        perfm_fatal("%s\n", e.what());
    }

    std::string _e_name; // EventName
    std::string _e_code; // EventCode
    std::string _e_umsk; // UMask
    std::string _e_cmsk; // CounterMask
    std::string _e_desc; // BriefDescription
    std::string _e_inv;  // Invert
    std::string _e_any;  // AnyThread
    std::string _e_edge; // EdgeDetect  
    std::string _e_prid; // SampleAfterValue
    
    for (property_tree::ptree::iterator it = ptree.begin(); it != ptree.end(); ++it) {
        try { 
            _e_name = it->second.get<std::string>("EventName");
            _e_code = it->second.get<std::string>("EventCode");  
            _e_umsk = it->second.get<std::string>("UMask");
            _e_cmsk = it->second.get<std::string>("CounterMask");
            _e_desc = it->second.get<std::string>("BriefDescription");
            _e_inv  = it->second.get<std::string>("Invert");
            _e_any  = it->second.get<std::string>("AnyThread");
            _e_edge = it->second.get<std::string>("EdgeDetect");
            _e_prid = it->second.get<std::string>("SampleAfterValue");
        } catch (const property_tree::ptree_bad_path &e) {
            perfm_fatal("%s\n", e.what());
        } catch (const std::exception &e) {
            perfm_fatal("%s\n", e.what());
        }
        /* TODO */
    }

    /* TODO */
}

void parser::load_socket_event_description(const std::string &json_filp, bool append)
{
    if (json_filp.empty() && !append) {
        perfm_fatal("please specify a json file which describes the pmu events\n");
    }

    if (!append) {
        // do some cleaning work 
    }

    // Define the root of the property tree
    property_tree::ptree ptree;

    // Read JSON from a the given file and translate it to a property tree.
    // - Clears existing contents of property tree. In case of error the property tree unmodified.
    // - Items of JSON arrays are translated into ptree keys with empty names.
    //   Members of objects are translated into named keys.
    // - JSON data can be a string, a numeric value, or one of literals "null", "true" and "false".
    //   During parse, any of the above is copied verbatim into ptree data string.
    try {
        json::read_json(json_filp, ptree);
    } catch (const json::json_parser_error &e) {
        perfm_fatal("%s\n", e.what());
    } catch (const std::exception &e) {
        perfm_fatal("%s\n", e.what());
    }

    /* TODO */
}

} /* namespace perfm */

int main(int argc, char **argv)
{
    perfm::parser::ptr_t parser = perfm::parser::alloc();

    parser->pmu_detect();

    parser->print();
}
