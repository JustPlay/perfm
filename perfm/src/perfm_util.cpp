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
                perfm_warning("nanosleep() failed, remaining %ld(ns)\n", tv[rem].tv_sec * 1000000000 + tv[rem].tv_nsec);
                break;
            }
        }

    } else {  /* absolute time sleep */
        perfm_error("%s\n", "not impled for now");
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
            if (--limit) {
                search = target + del.size();
            } else {
                search = target;
            }
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

    int fd = ::open(filp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
    if (fd == -1) {
        char errbuf[PERFM_BUFERR] = { '\0' };        
        strerror_r(errno, errbuf, sizeof(errbuf));
        perfm_warning("::open() %s %s\n", filp, errbuf);
        return false;
    }

    ssize_t nr = 0;
    do {
        nr = ::write(fd, buf, sz); 
        if (nr == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                char errbuf[PERFM_BUFERR] = { '\0' };
                strerror_r(errno, errbuf, sizeof(errbuf));
                perfm_warning("::write() %s %s\n", filp, errbuf);
                return false;
            }
        }

        buf = static_cast<char *>(buf) + nr;
        sz -= nr;
    } while (sz);

    return true;
}

} /* namespace perfm */
