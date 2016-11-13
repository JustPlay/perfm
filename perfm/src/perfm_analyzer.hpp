/**
 * perfm_analyzer.hpp - interface for the perfm analyzer
 *
 */
#ifndef __PERFM_ANALYZER_HPP__
#define __PERFM_ANALYZER_HPP__

#include "perfm_util.hpp"
#include "perfm_event.hpp"
#include "perfm_evgrp.hpp"
#include "perfm_option.hpp"

#include <cstdlib>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <utility>
#include <tuple>

namespace perfm {

namespace xml = rapidxml;

class analyzer_t {
    using pmu_data_t = std::tuple<uint64_t>;

public:
    bool metric_parse(const char *filp);

    void metric_eval();
    void metric_eval(const std::string &metric);

private:
    bool metric_parse(xml::xml_node<char> *metric);

    std::string formula_infix2postfix(const std::string &formula_infix) const;

private:
    using metric_nam_t = std::string;
    using me_formula_t = std::pair<std::string, std::map<std::string, std::string>>;
    std::unordered_map<metric_nam_t, me_formula_t> formula_list; /* metric = formula
                                                                  * e.g. 
                                                                  *     metric_CPI = a / b
                                                                  *     a = CPU_CLK_UNHALTED.THREAD
                                                                  *     b = INST_RETIRED.ANY
                                                                  */
    std::vector<metric_nam_t> metrics_list;

    std::unordered_map<std::string, pmu_data_t> metrics_data;
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
