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

class analyzer_t {
public:
    using evnam_set_t = std::unordered_set<std::string>;               /* event name list */
    using evdat_fmt_t = std::tuple<uint64_t>;                          /* event value fmt */
    using evdat_map_t = std::unordered_map<std::string , evdat_fmt_t>; /* event data list */

    using metric_nam_t = std::string;
    using me_formula_t = std::pair<std::string, std::map<std::string, evnam_set_t::iterator>>;

public:
    bool metric_parse(const char *filp);

    void metric_eval();
    void metric_eval(const std::string &metric);

private:
    bool metric_parse(xml::xml_node<char> *metric);

    std::string expr_in2postfix(const std::string &infix) const;

    double expr_eval(const me_formula_t &expr) const;

private:
    std::unordered_map<metric_nam_t, me_formula_t> formula_list; /* metric = formula
                                                                  * e.g.
                                                                  *     metric_CPI = a / b
                                                                  *     a = CPU_CLK_UNHALTED.THREAD
                                                                  *     b = INST_RETIRED.ANY
                                                                  */
    std::vector<metric_nam_t> metrics_list;

    evnam_set_t _evnam;   /* event name set */
    evdat_map_t _evdat;   /* event data set */
};

} /* namespace perfm */

#endif /* __PERFM_ANALYZER_HPP__ */
