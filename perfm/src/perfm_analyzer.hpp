/**
 * perfm_analyzer.hpp - interface for the perfm analyzer
 *
 */
#ifndef __PERFM_ANALYZER_HPP__
#define __PERFM_ANALYZER_HPP__

#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_xml.hpp"

#include <cstdlib>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <memory>

namespace perfm {

namespace xml = rapidxml;

class metric;
class pmu_val;
class analyzer;

class metric {
    friend class analyzer;

public:
    using ptr_t = std::shared_ptr<metric>;

private:
    enum {
        PMU_CORE = 0,  /* core PMU */
        PMU_UNCORE,    /* uncore PMU (socket level) */
        PMU_OFFCORE,   /* offcore PMU */
        PMU_CONSTANT,  /* constant value */
        PMU_UNKNOWN,   /* unknown type */
        PMU_TYPE_MAX
    };

    /* name=>type
     *
     * key: event's name string
     * val: event's type (e.g. core PMU, uncore PMU)
     */
    using _ev_name_map_t = std::unordered_map<std::string, int>;

    using _metric_nam_t = std::string;
    using _expression_t = std::pair<std::string, std::map<std::string, _ev_name_map_t::iterator>>;

public:
    static ptr_t alloc();

    void metric_parse(const std::string &filp);
    void events_parse(const std::string &filp);

private:
    bool metric_parse(xml::xml_node<char> *m);

private:
    std::unordered_map<_metric_nam_t, _expression_t> _formula_list; /* metric = formula
                                                                     * e.g.
                                                                     *     metric_CPI = a / b
                                                                     *     a = CPU_CLK_UNHALTED:THREAD
                                                                     *     b = INST_RETIRED:ANY
                                                                     */
    std::vector<_metric_nam_t> _metrics_list;

    _ev_name_map_t _ev_name;  /* event name=>type list */
};

class pmu_val {
    friend class analyzer;

public:
    using ptr_t = std::shared_ptr<pmu_val>;

private:

    /* data format for one event (with multiple processors/cores/sockets, a.k.a multiple cloumns)
     * 
     * first:  64-bit unsigned integer - | un-used (24-bit) | processor_id (16-bit) | core_id (16-bit) | socket_id (8-bit) |
     * second: event's pmu value (aggregated & averaged)
     * 
     * processor_id: global uniq (/sys/devices/system/cpu/cpuX)
     * core_id:      uniq in socket level (may be discontinuous)
     * socket_id:    uniq in system level
     */
    using _ev_data_fmt_t = std::vector<std::pair<uint64_t, double>>;

    /* name=>data
     *
     * key: event's name string
     * val: event's pmu value
     */
    using _ev_data_map_t = std::unordered_map<std::string, _ev_data_fmt_t>;

public:
    static ptr_t alloc();

    void collect(const std::string &filp);

private:
    _ev_data_map_t _ev_data;  /* event name=>data list, averaged value */
};

class analyzer {
public:
    using ptr_t = std::shared_ptr<analyzer>;

public:
    static ptr_t alloc();

    void topology();
    void compute();

private:
    void eval(/* TODO */);
    void eval(/* TODO */const _metric_nam_t &metric);

    std::string expr_in2postfix(const std::string &infix) const;

    double expr_eval(const _expression_t &expr, size_t column) const;

private:
    int _nr_thread;
    int _nr_core;
    int _nr_socket;
    int _nr_system = 1;

    metric::ptr_t  _metric;
    pmu_val::ptr_t _pmu_val;

    const static std::string _thread_view_file;
    const static std::string _core_view_file;
    const static std::string _socket_view_file;
    const static std::string _system_view_file;

    static constexpr size_t NR_CPU_MAX = 512;
    std::array<std::tuple<bool, int, int>, NR_CPU_MAX> _cpu; /* subscript is (logical) processor's id
                                                              * array type is: <online, core, socket>
                                                              */
    #define processor_online(c) std::get<0>(_cpu[(c)])
    #define processor_core(c)   std::get<1>(_cpu[(c)])
    #define processor_socket(c) std::get<2>(_cpu[(c)])
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
