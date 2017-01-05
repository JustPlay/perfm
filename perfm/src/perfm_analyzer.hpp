/**
 * perfm_analyzer.hpp - interface for the perfm analyzer
 *
 */
#ifndef __PERFM_ANALYZER_HPP__
#define __PERFM_ANALYZER_HPP__

#include "perfm_util.hpp"
#include "perfm_config.hpp"
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
#include <array>
#include <memory>

namespace perfm {

namespace xml = rapidxml;

enum {
    PMU_CORE = 0,  /* core PMU */
    PMU_OFFCORE,   /* offcore PMU */
    PMU_UNCORE,    /* uncore PMU (socket level) */
    PMU_CONSTANT,  /* constant value */
    PMU_UNKNOWN,   /* unknown type */
    PMU_TYPE_MAX
};

class metric;
class analyzer;

class metric {
    friend class analyzer;

public:
    using ptr_t = std::shared_ptr<metric>;

private:
    /* name=>type
     *
     * key: event's name string
     * val: event's type (e.g. core PMU, uncore PMU)
     */
    using _e_name_map_t = std::unordered_map<std::string, int>;
    using _metric_nam_t = std::string;
    using _expression_t = std::pair<std::string, std::map<std::string, _e_name_map_t::iterator>>;

public:
    static ptr_t alloc();

    void metric_parse(const std::string &filp);
    void events_parse(const std::string &filp);

    int evn2type(const std::string &evn);

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

    _e_name_map_t _e_name;  /* event name=>type list */
};

class analyzer {
public:
    using ptr_t = std::shared_ptr<analyzer>;

public:
    static ptr_t alloc();

    void analyze(/* TODO */);

private:
    void topology(const std::string &filp = "");

    void collect(const std::string &filp = "");

    void compute();

    void insert(const std::string &evn, const std::vector<double> &val);
    void average(const std::unordered_map<std::string, size_t> &ev_count);

    void thrd_compute();
    void core_compute();

    void socket_compute();
    void system_compute();

    void eval(/* TODO */);
    void eval(/* TODO */const _metric_nam_t &metric);

    std::string expr_in2postfix(const std::string &infix) const;

    double expr_eval(const _expression_t &expr, size_t column) const;

private:
    /* data format for one PMU event
     *
     * since event may be collected simultaneously on multiple processors, cores, sockets and so
     * we use a std::array. for each of the processors/cores/sockets, there exists a elem in the array
     * 
     * event's PMU value are aggregated & averaged by multiple samples (so we use double, not uint64_t)
     */
    using _e_thrd_elem_t   = std::array<double, NR_MAX_PROCESSOR>;
    using _e_core_elem_t   = std::array<double, NR_MAX_CORE>;
    using _e_socket_elem_t = std::array<double, NR_MAX_SOCKET>;
    using _e_system_elem_t = std::array<double, 1U>;

    /* name=>data list
     *
     * key: event's name string
     * val: event's pmu value (a smart pointer to std::array)
     */
    using _e_thrd_t   = std::unordered_map<std::string, std::shared_ptr<_e_thrd_elem_t>>;
    using _e_core_t   = std::unordered_map<std::string, std::shared_ptr<_e_core_elem_t>>;
    using _e_socket_t = std::unordered_map<std::string, std::shared_ptr<_e_socket_elem_t>>;
    using _e_system_t = std::unordered_map<std::string, std::shared_ptr<_e_system_elem_t>>;

private:
    metric::ptr_t  _metric;

    unsigned int _nr_thread;
    unsigned int _nr_core;
    unsigned int _nr_socket;
    unsigned int _nr_system = 1;

    /* event data list, processor level, only for core PMU events
     *
     * key: event's name string
     * val: an array, the sub-script *is* the logical processor's id
     *
     * the logical processor's id can be obtained either from 
     *   "/proc/cpuinfo" 
     * or 
     *   "/sys/devices/system/cpu/"
     *
     * processor_id: global uniq (/sys/devices/system/cpu/cpuX)
     * core_id:      uniq in socket level (may be discontinuous)
     * socket_id:    uniq in system level
     */
    _e_thrd_t _e_thread;

    /* event data list, physical core level, only for core PMU events
     * 
     * key: event's name string
     * val: an array, the sub-script is *maped* to socket's id + core's id
     * 
     * if N = NR_MAX_CORE_PER_SKT, then: 
     *   [0, N - 1]:          cores for socket0 (include non-exist cores)
     *   [N,  2 * N - 1]:     cores for socket1
     *   [2 * N,  3 * N - 1]: cores for socket2
     */
    _e_core_t _e_core;

    std::bitset<NR_MAX_CORE> _core_usable_list;
    #define core_script(socket, coreid)  ((socket) * NR_MAX_CORE_PER_SKT + (coreid))
    #define core_usable(socket, coreid)  _core_usable_list.test(core_script(socket, coreid))

    /* event name list, socket level, for all events (core & uncore PMU events)
     *
     * key: event's name string
     * val: an array, the sub-script is the socket's id
     */
    _e_socket_t _e_socket;

    std::bitset<NR_MAX_SOCKET> _skt_usable_list;

    /* event name list, system level, for all events (core & uncore PMU events)
     *
     * key: event's name string
     * val: an array(with just 1 elem), the sub-script should always be 0
     */
    _e_system_t _e_system;

    const static std::string _thread_view_filp;
    const static std::string _core_view_filp;
    const static std::string _socket_view_filp;
    const static std::string _system_view_filp;

    /* subscript is (logical) processor's id
     * array type is: <status, coreid, socket>
     *
     * status - -1: not presented (not usable), 0: not online, 1: online
     * coreid - physical core id
     * socket - socket id (physical package id)
     */
    std::array<std::tuple<int, int, int>, NR_MAX_PROCESSOR> _cpu_topology;
                                                                           
    #define _m_processor_status(c)  std::get<0>(this->_cpu_topology[(c)])
    #define _m_processor_coreid(c)  std::get<1>(this->_cpu_topology[(c)])
    #define _m_processor_socket(c)  std::get<2>(this->_cpu_topology[(c)])

    #define _m_processor_online(c)  _m_processor_status(c) == 1
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
