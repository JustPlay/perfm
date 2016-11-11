/**
 * utils.hpp - helper functions for common purpose
 *
 */
#ifndef __UTILS_HPP_
#define __UTILS_HPP_

#include <vector>
#include <string>

#include <cstdio>
#include <cstdlib>

#ifdef  BUFERR
#undef  BUFERR
#endif
#define BUFERR 256

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

namespace util {

std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit = 0);

ssize_t write_file(const char *file, void *buf, size_t sz);

} /* namespace util */

#endif /* __UTILS_HPP_ */
