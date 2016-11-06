/**
 * perfm_util.hpp - helper functions for perfm
 *
 */
#ifndef __PERFM_UTIL_HPP__
#define __PERFM_UTIL_HPP__

#include <cstdio>
#include <cstdlib>

#include <vector>
#include <string>

#include <perfmon/perf_event.h>  /* libpfm4 */

//
// http://www.cnblogs.com/caosiyang/archive/2012/08/21/2648870.html
//
// #define LOG(format, ...)     fprintf(stdout, format, ##__VA_ARGS__)
// #define LOG(format, args...) fprintf(stdout, format, ##args)
//

#define perfm_error(fmt, ...) do {                                           \
    fprintf(stderr, "[%s, %d, %s()]: ", __FILE__, __LINE__, __FUNCTION__);   \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                     \
    exit(EXIT_FAILURE);                                                      \
} while (0)

#define perfm_warning(fmt, ...) do {                                         \
    fprintf(stderr, "[%s, %d, %s()]: ", __FILE__, __LINE__, __FUNCTION__);   \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                     \
} while (0)

#define perfm_message(fmt, ...) do {                                         \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                     \
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
 * struct {
 *     u64 nr;
 *     u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *     u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 * }
 *
 * The read format *with* PERF_FORMAT_GROUP enabled:
 * struct {
 *     u64 nr;
 *     u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED 
 *     u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING
 *     {
 *         u64 value;
 *         u64 id;  // PERF_FORMAT_ID 
 *     } cntr[nr];
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

/**
 * mround - returns a number rounded to the desired multiple
 * 
 * @number    the value to round
 * @multiple  the multiple to which you want to round number
 *
 * Return: 
 *     a double value
 *
 * Description:
 *     mround() rounds up away from zero, if the remainder of
 *     dividing number by multiple is greater than or equal to
 *     half the value of multiple.
 *
 * FIXME:
 *     if @multiple is zero, we just return @number for now
 */
double mround(double number, double multiple);

/**
 * nanoseconds_sleep - high-resolution sleep without EINTR 
 *
 * @seconds        time to sleep (can be a float value, e.g. 0.01)
 * @use_abs_clock  see time.h for linux
 *
 * Return:
 *     void
 *
 * Description:
 *     nanoseconds_sleep will ...
 * 
 * TODO:
 *     sleep with ABS clock has not impled
 */
void nanoseconds_sleep(double seconds, bool use_abs_clock = false);

/**
 * str_find - find the target string in the given string array
 *
 * @argv  same as argv for main()
 * @argc  same as argc for main()
 * @trg   a NULL terminated C style string
 *
 * Return:
 *     the subscript of the @trg in @argv or -1
 *
 * Description:
 *
 */
int str_find(char **argv, int argc, const char *trg);


/**
 * str_split - split a string by string
 *
 * @str    the input string to split
 * @del    the boundary string
 * @limit  if set and positive, the returned std::vector will contain a maximum of @limit + 1 
 *         elements with the last element containing the rest of @str
 *         if the @limit parameter is zero or default, then it is treated as @str.size()
 *
 * Return:
 *     a std::vector of strings created by splitting the @str parameter on boundaries formed by the @del
 * 
 * Description:  
 *     none
 */
std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit = 0);

/**
 * str_trim - remove space in both left and right of the given string
 *
 * @str  the string to trim
 *
 * Return:
 *     the trimed version (copy) of @str
 *     
 * Description:
 *
 */
std::string str_trim(const std::string &str);

} /* namespace perfm */

#endif /* __PERFM_UTIL_HPP_ */
