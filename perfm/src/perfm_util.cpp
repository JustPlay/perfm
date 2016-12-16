/**
 * perfm_util.cpp - 
 *
 */

#include "perfm_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <vector>
#include <string>
#include <utility>
#include <fstream>
#include <functional>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <error.h>
#include <dirent.h>

namespace perfm {

void nanosecond_sleep(double seconds, bool sleep_with_abs_time)
{
    long int sec  = static_cast<long int>(seconds);
    long int nsec = static_cast<long int>((seconds - sec) * 1000000000);

    if (!sleep_with_abs_time) {  /* relative time sleep */
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
        while (-1 == ::clock_nanosleep(CLOCK_MONOTONIC, 0, &tv[req], &tv[rem])) {
            if (EINTR == errno) {
                req ^= rem; rem ^= req; req ^= rem;
                continue;
            } else {
                perfm_warn("sleep failed, remaining %ld(s) %ld(ns)\n", tv[rem].tv_sec, tv[rem].tv_nsec);
                return;
            }
        }

    } else {  /* absolute time sleep */
        struct timespec req;
        struct timespec rem;

        if (::clock_gettime(CLOCK_MONOTONIC, &req) != 0) {
            perfm_warn("failed to get curent time, sleep failed\n");
            return;
        }

        req.tv_sec  += sec;
        req.tv_nsec += nsec;

        while (-1 == ::clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, &rem)) {
            if (EINTR == errno) {
                continue;
            } else {
                perfm_warn("sleep failed, remaining %ld(s) %ld(ns)\n", rem.tv_sec, rem.tv_nsec);
                return;
            }
        }
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

std::string str_trim(const std::string &str, const char *charlist)
{
    if (str.empty()) {
        return str;
    }

    std::string::size_type s = 0, e = str.size() - 1;

    if (!charlist) {
        while (s <= e && std::isspace(str[s])) {
            ++s;
        }

        while (e >= s && std::isspace(str[e])) {
            --e;
        }

    } else {
        bool chmap[256] = { false };
        auto chlen = std::strlen(charlist);

        for (size_t i = 0; i < chlen; ++i) {
            chmap[static_cast<int>(charlist[i])] = true;
        }

        auto need_trim = [&chmap] (int ch) -> bool {
            return chmap[ch];
        };

        while (s <= e && need_trim(str[s])) {
            ++s;
        }

        while (e >= s && need_trim(str[e])) {
            --e;
        }
    }

    if (s <= e) {
        return std::move(str.substr(s, e - s + 1));
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

std::vector<std::string> str_split(const std::string &str, const std::string &del, size_t limit, bool ignore_empty)
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

    // find_first_of - Finds the first __character__ equal to one of the characters in the given character sequence
    // find - Finds the first __substring__ equal to the given character sequence
    while (limit && search < str.size()) {
        target = str.find(del, search);
        if (target != std::string::npos) {
            if (ignore_empty && target == search) {
                search += del.size();  
            } else {
                result.push_back(std::move(str.substr(search, target - search)));
                search = --limit ? target + del.size() : target;
            }
        } else {
            result.push_back(std::move(str.substr(search)));
            break;
        }
    }

    if (!limit && search < str.size()) {
        result.push_back(std::move(str.substr(search)));
    }
    
    return std::move(result);
}

bool save_file(const char *filp, void *buf, size_t sz)
{
    if (!filp || !buf || !sz) {
        return false;
    }

    int fd = ::open(filp, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
    if (fd == -1) {
        perfm_warn("failed to open %s, %s\n", filp, strerror_r(errno, NULL, 0));
        return false;
    }

    ssize_t nr = 0;
    while (sz) {
        nr = ::write(fd, buf, sz); 
        if (nr >= 0) {
            buf = static_cast<char *>(buf) + nr;
            sz -= nr;
            continue;
        }

        if (errno != EINTR) {
            perfm_warn("failed to write %s, %s\n", filp, strerror_r(errno, NULL, 0));
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
        perfm_warn("failed to open %s, %s\n", filp, strerror_r(errno, NULL, 0));
        return NULL;
    }

    struct stat sb;
    if (fstat(fd, &sb) != 0) {
        perfm_warn("failed to stat file %s, %s\n", filp, strerror_r(errno, NULL, 0));
        close(fd);
        return NULL;
    }

    void *res = calloc(sb.st_size / sizeof(uint64_t) + 1, sizeof(uint64_t));
    if (!res) {
        perfm_warn("failed to alloc memory\n");
        close(fd);
        return NULL;
    }
    
    char *buf = static_cast<char *>(res);
    ssize_t nr_read = 0;
    size_t  nr_remn = sb.st_size;

    while (nr_remn) {
        nr_read = ::read(fd, buf, nr_remn); 

        if (nr_read >= 0) {
            nr_remn -= nr_read;
            buf     += nr_read;
            continue;
        }

        if (errno != EINTR) {
            perfm_warn("failed to read %s, %s\n", filp, strerror_r(errno, NULL, 0));
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


int is_cpu(const struct dirent *dirp)
{
    return std::isdigit(dirp->d_name[3]) && std::strncmp(dirp->d_name, "cpu", 3) == 0;
}

size_t num_cpus_total()
{
    struct dirent **namelist; 
    int nr_dirent = 0;

    nr_dirent = ::scandir("/sys/devices/system/cpu/", &namelist, is_cpu, NULL);
    if (nr_dirent < 0) {
        perfm_warn("failed to get the # of processors on this system\n");
        return 0;
    }

    for (int cpu = 0; cpu < nr_dirent; ++cpu) {
        free(namelist[cpu]);
    }

    free(namelist);
    
    return nr_dirent;
}

std::map<int, int> cpu_frequency()
{
    std::map<int, int> freq_list;

    std::fstream fp("/proc/cpuinfo", std::ios::in);    
    if (!fp.good()) {
        perfm_warn("failed to open /proc/cpuinfo\n");
    }

    int cpu_id;
    int cpu_freq;

    bool pair = true;

    std::string line;
    while (std::getline(fp, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.find("processor") != std::string::npos) {

            if (!pair) {
                perfm_fatal("processor & cpu MHz should appear in pair, check /proc/cpuinfo\n");
            }

            auto slice = str_split(line, ":", 2); 
            if (slice.size() != 2) {
                perfm_fatal("the format of /proc/cpuinfo has changed? e.g. not x86 platform\n");
            }

            try {
                cpu_id = std::stoi(str_trim(slice[1]));
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), line.c_str());
            }

            pair = false;

            continue;
        }

        if (line.find("cpu MHz") != std::string::npos) {
            if (pair) {
                perfm_fatal("processor & cpu MHz should appear in pair, check /proc/cpuinfo\n");
            }

            auto slice = str_split(line, ":", 2);
            if (slice.size() != 2) {
                perfm_fatal("the format of /proc/cpuinfo has changed? e.g. not x86 platform\n");
            }

            try {
                cpu_freq = std::stoi(str_trim(slice[1]));
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), line.c_str());
            }

            freq_list.insert({cpu_id, cpu_freq});

            pair = true;

            continue;
        }
    }

    return std::move(freq_list);
}

} /* namespace perfm */
