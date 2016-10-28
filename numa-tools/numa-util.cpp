
#include "numa-util.hpp"

#include <vector>
#include <string>
#include <fstream>

namespace numa {

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

bool is_subsys_avail(const std::string &subsys)
{
    std::fstream fin("/proc/mounts", std::ios_base::in); 
    std::string line;

    while (std::getline(fin, line)) {
        if (line.find_first_of(subsys) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool is_cgroup_avail()
{
    std::fstream fin("/proc/filesystems", std::ios_base::in);
    std::string line;

    while (std::getline(fin, line)) {
        if (line.find_first_of("cgroup") != std::string::npos) {
            return true;
        }
    }

    return false;
}

} /* namespace numa */
