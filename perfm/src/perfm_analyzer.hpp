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

namespace perfm {

class analyzer_t {

public:
    bool metric_parse(const char *filp);


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
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
