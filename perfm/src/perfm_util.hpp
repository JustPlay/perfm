/**
 * perfm_util.hpp - helper functions for perfm
 *
 */
#ifndef __PERFM_UTIL_HPP__
#define __PERFM_UTIL_HPP__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* strerror_r(2) */
#endif

#include <cstdio>
#include <cstdlib>

#include <vector>
#include <string>
#include <utility>
#include <map>

#include <unistd.h>

//
// http://www.cnblogs.com/caosiyang/archive/2012/08/21/2648870.html
//
// #define LOG(format, ...)     fprintf(stdout, format, ##__VA_ARGS__)
// #define LOG(format, args...) fprintf(stdout, format, ##args)
//

#define PERFM_BUFERR 256
#define PERFM_BUFSIZ 8192

#define program "perfm"

#ifndef NDEBUG

#define perfm_fatal(fmt, ...) do {                                                      \
    fprintf(stderr, "[%s, %d, %s()] %s: ", __FILE__, __LINE__, __FUNCTION__, program);  \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
    exit(EXIT_FAILURE);                                                                 \
} while (0)

#define perfm_warn(fmt, ...) do {                                                       \
    fprintf(stderr, "[%s, %d, %s()] %s: ", __FILE__, __LINE__, __FUNCTION__, program);  \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
} while (0)

#define perfm_info(fmt, ...) do {                                                       \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                                \
} while (0)

#else  /* NDEBUG */

#define perfm_fatal(fmt, ...) do {                                                      \
    fprintf(stderr, program ": " fmt, ##__VA_ARGS__);                                   \
    exit(EXIT_FAILURE);                                                                 \
} while (0)

#define perfm_warn(fmt, ...) do {                                                       \
    fprintf(stderr, program ": " fmt, ##__VA_ARGS__);                                   \
} while (0)

#define perfm_info(fmt, ...) do {                                                       \
    fprintf(stdout, fmt, ##__VA_ARGS__);                                                \
} while (0)

#endif /* NDEBUG */

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
 * nanosecond_sleep - high-resolution sleep without EINTR 
 *
 * @seconds              time to sleep (can be a float value, e.g. 0.01)
 * @sleep_with_abs_time  clock_nanosleep(2)
 *
 * Return:
 *     void
 *
 * Description:
 *     nanosecond_sleep will ...
 *
 *     @seconds may *not* greater than the value which 'long int' can hold
 * 
 */
void nanosecond_sleep(double seconds, bool sleep_with_abs_time = false);

/**
 * str_find - find the target string in the given C style string array
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
 * @str           the input string to split
 * @del           the boundary string
 * @limit         if set and positive, the returned std::vector will contain a maximum of @limit + 1 
 *                elements with the last element containing the rest of @str (if it has ...)
 *                
 *                if the @limit parameter is zero or default, then it is treated 
 *                as @str.size() (as large as possible)
 *
 * @ignore_empty  ignore empty component, defaults to false. for example
 *                when @str is "1,2,,3,," and @del is ",", 
 *                if @ignore_empty is true, then the return component list will be "1", "2", "3",
 *                otherwise, the return component list will be "1", "2", "", "3", ""
 *
 * Return:
 *     a std::vector of strings created by splitting the @str parameter on boundaries formed by the @del
 * 
 * Description:  
 *     none
 */
std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit = 0, bool ignore_empty = false);

/**
 * str_trim - remove characters in both left and right of the given string
 *
 * @str       the string to trim
 * @charlist  the characters to trim
 *
 * Return:
 *     the trimed version (copy) of @str
 *     
 * Description:
 *
 */
std::string str_trim(const std::string &str, const char *charlist = NULL);

/**
 * save_file - save content from @buf with @sz bytes to the file @filp
 *
 * @filp  filepath to save to
 * @buf   content to save (buf's size should greater than or equal to @sz) 
 * @sz    # of bytes to save
 *
 * Return:
 *     true  - save succ
 *     false - save fail (due to some serious error)
 * 
 * Descritpion:
 *     if @filp does not exist, it will be created (as a regular file)
 *     if @filp already existed, it will be truncted first
 */
bool save_file(const char *filp, void *buf, size_t sz);

/**
 * read_file - read the entire file specified by @filp
 *
 * @filp  filepath to read
 * @sz    filesize
 *
 * Return:
 *     a dynamicly allocated buffer which contains the entrie file or NULL if error occured
 *     if @sz not NULL, it will point to the filesize (the size of the returned buffer)
 * 
 * Descritpion:
 *     the return buffer should be freed by the caller
 */
void *read_file(const char *filp, size_t *sz = NULL);

/**
 * num_cpu_usable - return the number of processors installed on this system
 *
 * Return:
 *     the number of processors installed on this system
 *
 * Description:
 *     this func return the number of dirs named "cpuX" in "/sys/devices/system/cpu/" ('X' is the cpu number)
 *     
 *     in older vesion glibc or linux kernel, the _SC_NPROCESSORS_CONF *do* change when do cpu hotplug
 */
size_t num_cpu_usable();

/**
 * cpu_frequency - get frequency for all online processors
 *
 * Return:
 *     a map of <cpu, freq>, where cpu means processor id, freq means frequency in MHz
 *
 * Description:
 *     the frequency were obtained from /proc/cpuinfo, so the returned freq may be outdated
 *     
 *     for now, cpu_frequency only works on x86 platforms, because other platform may not 
 *     provide the 'cpu MHz' column (e.g. aarch64 & mips)
 */
std::map<int, int> cpu_frequency();

/**
 * read_tsc - read the TSC counter
 *
 * Return:
 *     the TSC counter's current value
 *
 * Description: 
 *    rdtsc/rdtscp instruction is used to move the 64-bit TSC counter into registers EDX:EAX.
 *    read_tsc() should only be used on newer intel processor with constant_tsc flag enabled in /proc/cpuinfo
 */
inline uint64_t read_tsc()
{
    uint32_t eax, edx;

    __asm__ volatile("rdtscp" : "=a" (eax), "=d" (edx));

    return static_cast<uint64_t>(eax) | (static_cast<uint64_t>(edx) << 32);
}

inline bool file_exist(const char *filp)
{
    return filp && ::access(filp, F_OK) == 0;
}

inline bool file_readable(const char *filp)
{
    return filp && ::access(filp, R_OK) == 0;
}

inline bool file_writable(const char *filp)
{
    return filp && ::access(filp, W_OK) == 0;
}

inline bool pid_exist(int p)
{
    return p >= 1 && ::access(std::string("/proc/" + std::to_string(p) + "/status").c_str(), F_OK) == 0;
}

inline bool cpu_exist(int c)
{
    return c >= 0 && ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(c)).c_str(), F_OK) == 0;
}

inline bool cpu_online(int c)
{
    // sysconf(3)
    //
    // _SC_NPROCESSORS_CONF - should be constant, _but_ in older version linux or glibc ... 
    // _SC_NPROCESSORS_ONLN - will change when do cpu hotplug

    // when cpuX is online, '/sys/devices/system/cpu/cpuX/cache' EXISTS on x86 linux, otherwise NOT
    return c >= 0 && ::access(std::string("/sys/devices/system/cpu/cpu" + std::to_string(c) + "/cache").c_str(), F_OK) == 0;
}

} /* namespace perfm */

#endif /* __PERFM_UTIL_HPP_ */
