/**
 * numa-util.hpp - helper functions for numa-tools
 *
 */
#ifndef __NUMA_UTIL_HPP_
#define __NUMA_UTIL_HPP_

#include <vector>
#include <string>

namespace numa {

std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit = 0);

} /* namespace numa */

#endif /* __NUMA_UTIL_HPP_ */
