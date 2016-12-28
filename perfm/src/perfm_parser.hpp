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

namespace perfm {

class parser {

public:
    using ptr_t = std::shared_ptr<parser>;

public:
    static ptr_t alloc();

public:
    long parse_event_source(const std::string &esrc);
    void parse_event_source();

    void parse_encoding_format();
    void parse_encoding_format(const std::string &pmu);

    void print() const;

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
     * 0: which field (a.k.a config, config1, config2) to use for perf_event_attr
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
    bool _already_initialized = false;
};

} /* namespace perfm */

#endif /* __PERFM_PARSER_HPP__ */
