/**
 * perfm_util.cpp - 
 *
 */

#include "perfm_util.hpp"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <vector>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <error.h>

namespace perfm {

void nanoseconds_sleep(double seconds, bool use_abs_clock)
{
    long int sec  = static_cast<long int>(seconds);
    long int nsec = static_cast<long int>((seconds - sec) * 1000000000);

    if (!use_abs_clock) {  /* relative time sleep */
        struct timespec tv[2] = {
            {
                .tv_sec  = sec,
                .tv_nsec = nsec,
            },

            {
                .tv_sec  = 0,
                .tv_nsec = 0,
            }
        };     

        int req = 0;
        int rem = 1;
        while (-1 == ::nanosleep(&tv[req], &tv[rem])) {
            if (EINTR == errno) {
                req ^= rem; rem ^= req; req ^= rem;
                continue;
            } else {
                perfm_warn("nanosleep() failed, remaining %ld(ns)\n", tv[rem].tv_sec * 1000000000 + tv[rem].tv_nsec);
                break;
            }
        }

    } else {  /* absolute time sleep */
        perfm_fatal("%s\n", "not impled for now");
    }
}

double mround(double number, double multiple)
{
    if (multiple == 0) {
        return number;
    }

    // http://en.cppreference.com/w/cpp/numeric/math/round
    // http://en.cppreference.com/w/cpp/numeric/math/ceil

    return std::round(number / multiple) * multiple;
}

std::string str_trim(const std::string &str)
{
    if (str.empty()) {
        return str;
    }

    std::string::size_type s = 0, e = str.size() - 1;

    while (s <= e && std::isspace(str[s])) {
        ++s;
    }

    while (e >= s && std::isspace(str[e])) {
        --e;
    }

    if (s <= e) {
        return str.substr(s, e - s + 1);
    }
    
    return "";
}

int str_find(char **argv, int argc, const char *trg)
{
    if (!trg || !argv || argc <= 0) {
        return -1;
    }

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(trg, argv[i]) == 0) {
            return i;
        }  
    }

    return -1;
}

std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit)
{
    std::vector<std::string> result;

    if (str.empty()) {
        return std::move(result);
    }

    if (del.empty()) {
        result.push_back(str);
        return std::move(result);
    }

    size_t search = 0;
    size_t target = 0;

    if (!limit) {
        limit = str.size();
    }

    while (limit && search < str.size()) {
        target = str.find_first_of(del, search);
        if (target != std::string::npos) {
            result.push_back(str.substr(search, target - search));
            search = --limit ? target + del.size() : target;
        } else {
            result.push_back(str.substr(search));
            break;
        }
    }

    if (!limit && search < str.size()) {
        result.push_back(str.substr(search));
    }
    
    return std::move(result);
}

bool write_file(const char *filp, void *buf, size_t sz)
{
    if (!filp || !buf || !sz) {
        return false;
    }

    int fd = ::open(filp, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
    if (fd == -1) {
        char *err = strerror_r(errno, NULL, 0);
        perfm_warn("open() %s %s\n", filp, err);
        return false;
    }

    ssize_t nr = 0;
    while (sz) {
        nr = ::write(fd, buf, sz); 
        if (nr >= 0) {
            buf = static_cast<char *>(buf) + nr;
            sz -= nr;
        }

        if (errno != EINTR) {
            char *err = strerror_r(errno, NULL, 0);
            perfm_warn("write() %s %s\n", filp, err);
            ::close(fd);
            return false;
        }
    }

    return true;
}

void *read_file(const char *filp, size_t *sz)
{
    if (!filp) {
        return NULL;
    } 

    int fd = ::open(filp, O_RDONLY);
    if (fd == -1) {
        char *err = strerror_r(errno, NULL, 0);
        perfm_warn("open() %s %s\n", filp, err);
        return NULL;
    }

    struct stat sb;
    if (fstat(fd, &sb) != 0) {
        char *err = strerror_r(errno, NULL, 0);
        perfm_warn("stat() %s %s\n", filp, err);
        close(fd);
        return NULL;
    }

    void *res = calloc(sb.st_size / sizeof(uint64_t) + 1, sizeof(uint64_t));
    if (!res) {
        perfm_warn("calloc() failed\n");
        close(fd);
        return NULL;
    }
    
    char *buf = static_cast<char *>(res);
    size_t nr_read = 0;
    size_t nr_remn = sb.st_size;

    while (nr_remn) {
        nr_read = ::read(fd, buf, nr_remn); 

        if (nr_read >= 0) {
            nr_remn -= nr_read;
            buf     += nr_read;
            continue;
        }

        if (errno != EINTR) {
            char *err = strerror_r(errno, NULL, 0);
            perfm_warn("read() %s %s\n", filp, err);
            close(fd);
            free(res);
            return NULL;
        }
    }

    if (sz) {
        *sz = sb.st_size;
    }

    return res;
}

} /* namespace perfm */
