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

namespace perfm {

namespace xml = rapidxml;

class analyzer {
private:
    enum pmu_event_type {
        PMU_CORE = 0,  /* core PMU */
        PMU_UNCORE,    /* uncore PMU (socket level) */
        PMU_OFFCORE,   /* offcore PMU */
        PMU_UNKNOWN,   /* unknown type */
        PMU_TYPE_MAX
    };

    using _ev_name_map_t = std::unordered_map<std::string, pmu_event_type>;  /* event name=>type map */
    using _ev_data_fmt_t = std::tuple<uint64_t>;                             /* event data format */
    using _ev_data_map_t = std::unordered_map<std::string , _ev_data_fmt_t>; /* event name=>data map */

    using _metric_nam_t = std::string;
    using _expression_t = std::pair<std::string, std::map<std::string, _ev_name_map_t::iterator>>;

public:
    bool metric_parse(const char *filp);

    void metric_eval();
    void metric_eval(const _metric_nam_t &metric);

private:
    bool metric_parse(xml::xml_node<char> *metric);

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
    _ev_data_map_t _ev_data;   /* event name=>data list */
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
