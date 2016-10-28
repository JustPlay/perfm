/**
 * numa-util.hpp - helper functions for numa-tools
 *
 */
#ifndef __NUMA_UTIL_HPP_
#define __NUMA_UTIL_HPP_

#include <vector>
#include <string>

#include <cstdio>
#include <cstdlib>

#define fatal(fmt, ...) do {                                               \
    fprintf(stderr, "[%s, %d, %s()]: ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                   \
    exit(EXIT_FAILURE);                                                    \
} while (0)

#define warn(fmt, ...) do {                                                \
    fprintf(stderr, "[%s, %d, %s()]: ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                   \
} while (0)

#define info(fmt, ...) do {                                                \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                   \
} while (0)


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

size_t nr_node();

} /* namespace numa */

#endif /* __NUMA_UTIL_HPP_ */
