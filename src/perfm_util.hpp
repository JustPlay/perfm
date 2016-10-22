#ifndef __PERFM_UTIL_HPP__
#define __PERFM_UTIL_HPP__

#include <cstdio>
#include <cstdlib>

#include <vector>
#include <string>

#include <perfmon/pfmlib.h>      /* libpfm4 */
#include <perfmon/perf_event.h>  /* libpfm4 */

//
// http://www.cnblogs.com/caosiyang/archive/2012/08/21/2648870.html
//
// #define LOG(format, ...)     fprintf(stdout, format, ##__VA_ARGS__)
// #define LOG(format, args...) fprintf(stdout, format, ##args)
//

#define perfm_error(fmt, ...) do {                                                             \
    fprintf(stderr, "[FILE: %s, LINE: %d, FUNC: %s()]: ", __FILE__, __LINE__, __FUNCTION__);   \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                       \
    exit(EXIT_FAILURE);                                                                        \
} while (0)

#define perfm_warning(fmt, ...) do {                                                           \
    fprintf(stderr, "[FILE: %s, LINE: %d, FUNC: %s()]: ", __FILE__, __LINE__, __FUNCTION__);   \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                       \
} while (0)

#define perfm_message(fmt, ...) do {                                                           \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                                       \
} while (0)

/*
 * Request timing information because event or/and PMU may be multiplexed
 * and thus it may not count all the time.
 * 
 * The scaling information will be used to scale the raw count
 * as if the event had run all along. The scale rules are list bellow:
 *
 * - TIME_RUNNING <= TIME_ENABLED
 * - TIME_RUNNING != 0 
 * - RAW_COUNT * TIME_ENABLED / TIME_RUNNING
 *
 * The read format *without* PERF_FORMAT_GROUP:
 * {
 *      u64  nr;
 *      u64  time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *      u64  time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 * }
 *
 * The read format *with* PERF_FORMAT_GROUP enabled:
 * {
 *      u64  nr;
 *      u64  time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *      u64  time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 *      {
 *          u64  value;
 *          u64  id;  // PERF_FORMAT_ID 
 *      } cntr[nr];
 * } // PERF_FORMAT_GROUP
 *
 * NOTE: perfm always enable PERF_FORMAT_TOTAL_TIME_ENABLED, PERF_FORMAT_TOTAL_TIME_RUNNING 
 *       perf_event_open(2)
 */

#ifdef  PEV_RDFMT_TIMEING
#undef  PEV_RDFMT_TIMEING
#endif
#define PEV_RDFMT_TIMEING (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

#define PEV_RDFMT_EVGROUP (PERF_FORMAT_GROUP)

#define PERFM_BUFERR 256
#define PERFM_BUFSIZ 8192

namespace perfm {

void nanoseconds_sleep(double seconds, bool use_abs_clock = false);

bool pmu_is_available(pfm_pmu_t pmu);

void pr_pmu_list(bool pr_all = 0);

/**
 * mround - Returns a number rounded to the desired multiple
 * 
 * @number    The value to round
 * @multiple  The multiple to which you want to round number
 *
 * Return: 
 *     A double value
 *
 * Description:
 *     mround() rounds up away from zero, if the remainder of
 *     dividing number by multiple is greater than or equal to
 *     half the value of multiple.
 *
 * FIXME:
 *     If @multiple is zero, we just return @number for now
 */
double mround(double number, double multiple);

/**
 * explode - Split a string by string
 *
 * @delimiter  The boundary string
 * @str        The input string
 * @limit      If set and positive, the returned std::vector will contain a maximum of @limit 
 *             elements with the last element containing the rest of @str
 *             If the @limit parameter is zero or negative, then this is treated as 1
 *
 * Return:
 *     An std::vector of strings created by splitting the @str parameter on boundaries formed by the @delimiter
 * 
 * Description:  
 *     none
 *
 * TODO:
 *     Impl it the same as php::explode
 */
std::vector<std::string> explode(const std::string &delimiter, const std::string &str, size_t limit = 0);

inline std::vector<std::string> explode(const char *delimiter, const char *str, size_t limit = 0) 
{
    return explode(std::string(delimiter), std::string(str), limit);
}

} /* namespace perfm */

#endif /* __PERFM_UTIL_HPP_ */
