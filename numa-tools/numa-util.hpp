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

bool is_cgroup_avail();

bool is_subsys_avail(const std::string &subsys);

inline bool is_subsys_avail(const char *subsys) {
    return is_subsys_avail(std::string(subsys));
}

inline bool is_cpuset_avail() {
    return is_subsys_avail("cpuset");
}

} /* namespace numa */

#endif /* __NUMA_UTIL_HPP_ */
