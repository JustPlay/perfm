/**
 * perfm_parser.hpp - resolve named performance counter events & turn them into perf_event_attr attributes
 *
 */
#ifndef __PERFM_PARSER_HPP__
#define __PERFM_PARSER_HPP__

// 
// http://man7.org/linux/man-pages/man2/perf_event_open.2.html
//
// perf_event related configuration files in /proc/sys/kernel/
// - /proc/sys/kernel/perf_event_paranoid
//       The existence of the perf_event_paranoid file is the official method for
//       determining if a kernel supports perf_event_open().
//
// - /proc/sys/kernel/perf_event_max_sample_rate
//
// - /proc/sys/kernel/perf_event_max_stack
//
// - /proc/sys/kernel/perf_event_mlock_kb
//
// perf_event related configuration files in /sys/bus/event_source/devices/
//   Since Linux 2.6.34, the kernel supports having multiple PMUs available for monitoring.
//   Information on how to program these PMUs can be found under "/sys/bus/event_source/devices/".
//   Each subdirectory corresponds to a different PMU.
//
// - /sys/bus/event_source/devices/*/type (since Linux 2.6.38)`
//       This contains an integer that can be used in the `type` field of `perf_event_attr` to 
//       indicate that you wish to use this PMU. For each type, there should exist an entry
//       in `perf_type_id` defined in <linux/perf_event.h>
//
// - /sys/bus/event_source/devices/cpu/rdpmc (since Linux 3.4)
//
// - /sys/bus/event_source/devices/*/format/ (since Linux 3.4)
//       This subdirectory contains information on the architecture-specific subfields
//       available for programming the various `config` fields in the `perf_event_attr` struct.
//       This is only for non-generic PMUs.
//       The content of each file is the name of the config field, followed by a colon, 
//       followed by a series of integer bit ranges separated by commas.
//
// - /sys/bus/event_source/devices/*/events/ (since Linux 3.4)
//       This subdirectory contains files with predefined events.
//
// - /sys/bus/event_source/devices/*/uevent
//       This file is the standard kernel device interface for injecting hotplug events.
//
// - /sys/bus/event_source/devices/*/cpumask (since Linux 3.7)
//       The cpumask file contains a comma-separated list of integers that indicate a 
//       representative CPU number for each socket (package) on the motherboard.
//       This is needed when setting up uncore or northbridge events, as those PMUs
//       present socket-wide events.
//

#include <unordered_map>
#include <memory>
#include <tuple>

#include <dirent.h>

#include "linux/perf_event.h"

namespace perfm {

//
// http://en.cppreference.com/w/cpp/language/operators
//
// http://en.cppreference.com/w/cpp/utility/functional/equal_to
//
// http://en.cppreference.com/w/cpp/utility/hash
//

class einfo {

public:
    using ptr_t = std::shared_ptr<einfo>;

public:
    static ptr_t alloc();

    virtual ~einfo() { }

public:
    virtual void print(FILE *fp = stdout) const;

protected:
    einfo() { }

public:
    std::string _r_name; // raw event name
    std::string _e_perf; // perf style descriptor  
    std::string _e_desc; // event description
    std::string _e_pmu;  // which PMU this event can use (e.g. tracepoint, software, raw, ...)

    enum {
        E_THREAD_LEVEL = 0,
        E_SOCKET_LEVEL = 1,

        E_PMU_CORE     = E_THREAD_LEVEL,
        E_PMU_UNCORE   = E_SOCKET_LEVEL,

        E_TYPE_MAX
    };

    int _e_type; // event type (e.g. core pmu event (thread level), uncore pmu event (socket level), ...)

    static const char *_e_type_string[E_TYPE_MAX];

public:
    struct hash {
        std::size_t operator()(const einfo &h) const {
            return std::hash<std::string>{}(h._r_name);
        }
    };
};

inline bool operator==(const einfo &lhs, const einfo &rhs)
{
    return lhs._r_name == rhs._r_name;
}

inline bool operator!=(const einfo &lhs, const einfo &rhs)
{
    return !(lhs == rhs);
}

class einfo_intel : public einfo {

public:
    using ptr_t = std::shared_ptr<einfo_intel>;

public:
    static ptr_t alloc();

    virtual void print(FILE *fp = stdout) const;

private:
    einfo_intel() : einfo() { }

public:
    /* Common fields
     * 
     * these fields should be valid for any type events
     */
    uint64_t _e_ecode; // EventCode
    uint64_t _e_umask; // UMask
    uint64_t _e_msrv;  // MSRValue

    /* Fields only for core pmu event
     *
     * these fields will be valid only when @_e_type == E_PMU_CORE
     */
    uint64_t _e_msri[5]; // MSRIndex
    uint64_t _e_cmask;   // CounterMask
    bool _e_inv;         // Invert
    bool _e_any;         // AnyThread
    bool _e_edge;        // EdgeDetect
    bool _e_pebs;        // Precise Event Based Sampling, a profiling technology in Intel CPUs 
                         // that uses microcode to do (mostly) _precise_ event samples.
    uint64_t _e_period;  // SampleAfterValue

    int _nr_msr = 0; 

    /* Fields only for uncore pmu event
     *
     * these fields will be valid only when @_e_type == E_PMU_UNCORE
     */
    std::string _e_unit; // Unit
    bool _e_extsel;      // ExtSel
};

class einfo_loongson : public einfo {

public:
    using ptr_t = std::shared_ptr<einfo_loongson>;

public:
    static ptr_t alloc();

    virtual void print(FILE *fp = stdout) const { }

private:
    einfo_loongson() : einfo() { }

public:

}; 

class parser {

public:
    using ptr_t = std::shared_ptr<parser>;

public:
    static ptr_t alloc();

public:
    /**
     * pmu_detect - detect all available PMUs provided by linux's perf_event subsystem
     *
     * Description:
     *     The supported PMUs will be detected from "/sys/bus/event_source/devices/"
     *     You need call this func only once, and the detected PMUs will be cached
     */
    void pmu_detect();

    /**
     * pmu_detect - detect the PMU named by @pmu, and return it's type
     *
     * Description:
     *     the return numerical type will be used by the `type` field of `perf_event_attr`
     *     
     * Return:
     *     if succ:  a non-negative number corresponding to the @pmu's numerical type
     *     if error: -1
     */
    long pmu_detect(const std::string &pmu);

    /**
     * load_event - load event description from json file
     *
     * @filp_thread: json file for thread level event (core pmu)
     * @filp_socket: json file for socket level event (uncore pmu)
     * @append:      append mode
     *
     * Description:
     *     
     */
    void load_event(const std::string &filp_thread, const std::string &filp_socket, bool append = false);

    /**
     * parse_perf_event - resolve perf style event descriptions to attr
     * 
     * @e:  perf style event description (e.g. cpu/event=1/)
     * @hw: perf_event_attr to fill in
     *
     * Description:
     *     Resolve perf style event descriptor to `perf_event_attr`. The `perf_event_attr` structure
     *     will be cleared firstly. User must initialize other fields of `perf_event_attr` _after_ this call
     *
     * Return:
     *     true:  succ
     *     false: failed
     */
    bool parse_perf_event(const std::string &e, struct perf_event_attr *hw);

    /**
     * parse_raw_event - turn raw event descriptor to perf style descriptor
     *
     * @r: raw event descriptor (arg)
     * @p: perf style event descriptor (res)
     *
     * Description:
     *     Resolve raw (may be architecture-specific) event descriptor to generic perf style descriptor
     *     e.g. INST_RETIRED.ANY --> cpu/event=0x00,umask=0x01
     *
     * Return:
     *     true:  succ
     *     false: failed
     */
    bool parse_raw_event(const std::string &r, std::string &p);

    bool parse_event(const std::string &raw, struct perf_event_attr *hw);

    void print() const;

    std::string pmu_name(int type) const;

private:
    /**
     * print_pmu_info - print pmu info
     *
     * @fp  file stream to write to
     *
     */
    void print_pmu_info(FILE *fp = stdout) const;

    /**
     * print_evn_info - print event info
     *
     * @fp  file stream to write to
     *
     */
    void print_evn_info(FILE *fp = stdout) const;

    /**
     * detect_encode_format - detect the encoding format for all detected PMUs or PMU named by @pmu
     *
     * @pmu: PMU's name 
     *
     * Description:
     *     the PMU's name should occured in "/sys/bus/event_source/devices/"
     */
    void detect_encode_format();
    void detect_encode_format(const std::string &pmu);

    bool parse_modifier(struct perf_event_attr *, const std::string &m) const;
    bool parse_pmu_type(struct perf_event_attr *, const std::string &p) const;
    bool parse_encoding(struct perf_event_attr *, const std::string &p, const std::string &e) const;

    /**
     * load_thread_level_event - load core pmu (thread level pmu) event description from json file
     *
     * @filp  json file to load from
     *
     * Description:
     *     TODO
     */
    void load_thread_level_event(const std::string &filp);

    /**
     * load_socket_level_event - load uncore pmu (socket level pmu) event description from json file
     *
     * @filp  json file to load from
     *
     * Description:
     *     TODO
     */
    void load_socket_level_event(const std::string &filp);

private:
    /* event sources provided by linux's perf_event subsystem 
     *
     * key: PMU's name (e.g. breakpoint, cpu, software, tracepoint)
     * val: PMU's type (e.g. 4 for PERF_TYPE_RAW), used by perf_event_attr.type
     * 
     * - linux kernel supports having multiple PMUs available for monitoring
     * - information on how to program these PMUs can be found under /sys/bus/event_source/devices/ and
     *   each subdirectory corresponds to a different PMU (such as: breakpoint, software, cpu/raw)
     */
    std::unordered_map<std::string, int> _event_source_list;

    enum {
        PERF_CONFIG_0 = 0, // perf_event_attr.config
        PERF_CONFIG_1,     // perf_event_atrr.config1
        PERF_CONFIG_2      // perf_event_attr.config2
    };

    /* information for PMU's format (e.g. event, umask, any)  
     *
     * related to one entry in /sys/bus/event_source/devices/<dev>/format/
     *
     * 0: which field (a.k.a config, config1, config2) to use for `perf_event_attr`
     * 1: lowest bit in the config field  (inclusive)
     * 2: highest bit in the config field (inclusive)
     */
    using _field_format_t = std::tuple<int, int, int>;

    /* PMU's format information provided by /sys/bus/event_source/devices/<dev>/format/
     *
     * key: name
     * val: config
     */
    using _event_format_t = std::unordered_map<std::string, _field_format_t>;

    /*
     * key: PMU's name
     * val: info about how to encording event on this PMU
     */
    std::unordered_map<std::string, _event_format_t> _event_format_list;

private:
    bool _pmu_source_detected = false;
    bool _event_table_loaded  = false;

private:
    std::unordered_map<std::string, einfo::ptr_t> _e_info_raw;

private:
    /* perf style descriptor to `perf_event_attr` cache
     *
     * key: perf style event descriptor
     * val: pointer to `perf_event_attr`
     *
     * FIXME: maybe memory leak ?
     */
    std::unordered_map<std::string, struct perf_event_attr *> _p2a_cache;

    // basedon intel's jevent lib, cache.c
    const std::unordered_map<std::string, std::string> _fixed_cntr_events = {
        {"INST_RETIRED.ANY",             "event=0xc0"}, 
        {"CPU_CLK_UNHALTED.THREAD",      "event=0x3c"}, 
        {"CPU_CLK_UNHALTED.THREAD_ANY",  "event=0x3c,any=1"}, 
    };

    /* attribute field mapping between intel's event description file to perf's format 
     * 
     * key: attribute field in intel's event description file (https://download.01.org/perfmon)
     * val: the corresponding attribute in perf's format directory (/sys/bus/event_source/devices/<pmu>/format)
     *
     * TODO:
     *     we need more ...
     */
    const std::unordered_map<std::string, std::string> _fields_mapping = {
        {"EventCode",         "event"},
        {"UMask",             "umask"},  
        {"SampleAfterValue",  "period"},  
        {"CounterMask",       "cmask"},  
        {"Invert",            "inv"},  
        {"AnyThread",         "any"},  
        {"EdgeDetect",        "edge"},  
    };
};

} /* namespace perfm */

#endif /* __PERFM_PARSER_HPP__ */
