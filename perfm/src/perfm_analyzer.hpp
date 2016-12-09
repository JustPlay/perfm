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

class analyzer {
public:
    using ptr_t = std::shared_ptr<analyzer>;

private:
    enum {
        PMU_CORE = 0,  /* core PMU */
        PMU_UNCORE,    /* uncore PMU (socket level) */
        PMU_OFFCORE,   /* offcore PMU */
        PMU_UNKNOWN,   /* unknown type */
        PMU_TYPE_MAX
    };

    using _ev_name_map_t = std::unordered_map<std::string, int>;             /* event name=>type map */
    using _ev_data_fmt_t = std::vector<double>;                              /* event data format */
    using _ev_data_map_t = std::unordered_map<std::string , _ev_data_fmt_t>; /* event name=>data map */

    using _metric_nam_t = std::string;
    using _expression_t = std::pair<std::string, std::map<std::string, _ev_name_map_t::iterator>>;

public:
    ptr_t alloc();

    void collect(const char *filp);
    void parse();

private:
    void metric_parse(const char *filp = "perfm_metric.xml");
    bool metric_parse(xml::xml_node<char> *metric);

    void metric_eval(/* TODO */);
    void metric_eval(/* TODO */const _metric_nam_t &metric);

    std::string expr_in2postfix(const std::string &infix) const;

    double expr_eval(const _expression_t &expr) const;

private:
    std::unordered_map<_metric_nam_t, _expression_t> _formula_list; /* metric = formula
                                                                     * e.g.
                                                                     *     metric_CPI = a / b
                                                                     *     a = CPU_CLK_UNHALTED:THREAD
                                                                     *     b = INST_RETIRED:ANY
                                                                     */
    std::vector<_metric_nam_t> _metrics_list;

    _ev_name_map_t _ev_name;   /* event name=>type list */
    _ev_data_map_t _ev_data;   /* event name=>data list, averaged value */

    int _nr_thread;
    int _nr_core;
    int _nr_socket;
    int _nr_system = 1;
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
