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

#include <errno.h>

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
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                \
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
 * nanoseconds_sleep - high-resolution sleep without EINTR 
 *
 * @seconds              time to sleep (can be a float value, e.g. 0.01)
 * @sleep_with_abs_time  clock_nanosleep(2)
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
void nanoseconds_sleep(double seconds, bool sleep_with_abs_time = false);

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

/**
 * save_file - save content specified by @buf & @sz to the file specified by @filp
 *
 * @filp  filepath to save to
 * @buf   
 * @sz
 *
 * Return:
 *     true  - save succ
 *     false - save fail
 * 
 * Descritpion:
 *     if @filp does not exist, it will be created
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

} /* namespace perfm */

#endif /* __PERFM_UTIL_HPP_ */
