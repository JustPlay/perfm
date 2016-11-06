
#include "utils.hpp"

#include <vector>
#include <string>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace util {

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

ssize_t write_file(const char *filp, void *buf, size_t sz)
{
    int fd = ::open(filp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IRGRP | S_IROTH); 
    if (fd == -1) {
        char buferr[BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        warn("open(): %s %s\n", filp, buferr);
        return -1;
    } 

    ssize_t nr = ::write(fd, buf, sz);
    if (nr == -1) {
        char buferr[BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        warn("write(): %s %s\n", filp, buferr);
        return -1;
    }

    return nr;
}

} /* namespace util */
