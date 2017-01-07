/**
 * perfm_json.hpp - json parser interface (based on boost::property_tree)
 *
 */
#ifndef __PERFM_JSON_HPP__
#define __PERFM_JSON_HPP__

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace perfm {

namespace property_tree = boost::property_tree;
namespace json          = property_tree::json_parser;

} /* namespace perfm */

#endif /* __PERFM_JSON_HPP__ */
