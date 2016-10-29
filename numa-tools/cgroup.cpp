#include "utils.hpp"
#include "cgroup.hpp"

#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace cgroup {

std::string cgroup::is_cgroup_supported()
{
    std::fstream fp("/proc/filesystems", std::ios_base::in);
    std::string  fs;

    while (std::getline(fp, fs)) {
        size_t pos = fs.find_first_of("cgroup");

        if (pos == std::string::npos) {
            continue;
        }

        return std::move(fs.substr(pos));
    }

    return std::move(std::string(""));
}

std::string cgroup::is_cgroup_installed()
{
    
}

std::string cgroup::is_subsys_installed(const std::string &subsys)
{
    std::fstream fp("/proc/mounts", std::ios_base::in); 
    std::string  fs;

    while (std::getline(fp, fs)) {
        std::vector<std::string> fs_mnt = util::str_split(fs, " ", 4);

        std::string fs_dev = fs_mnt[0];
        std::string fs_dir = fs_mnt[1];
        std::string fs_typ = fs_mnt[2];
        std::string fs_opt = fs_mnt[3];

        if (fs_typ == "cgroup" && fs_opt.find_first_of(subsys) != std::string::npos) {
            return fs_dir;
        }
    }

    return std::move(std::string(""));
}

} /* namespace cgroup */
