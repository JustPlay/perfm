#include "utils.hpp"
#include "cgroup.hpp"

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace cgroup {

std::string cgroup::is_supported()
{
    std::fstream fp("/proc/filesystems", std::ios_base::in);
    std::string  fs;

    /*
     * On Fedora24 with linux-4.7.7, there exists two cgroup fs in '/proc/filesystems', 
     * - nodev cgroup
     * - nodev cgroup2
     * the above one is used by default
     */
    while (std::getline(fp, fs)) {
        size_t pos = fs.find_first_of("cgroup");

        if (pos == std::string::npos) {
            continue;
        }

        return std::move(fs.substr(pos));
    }

    return std::move(std::string(""));
}

std::string cgroup::is_installed(const std::string &subsys)
{
    std::fstream fp("/proc/mounts", std::ios_base::in); 
    std::string  fs;

    // whether the cgroup filesystem itself had been mounted
    if (subsys.empty()) {
        fatal("not impled\n");

    // whether the cgroup subsystem specified by @subsys had been mounted
    } else {
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
    }

    return std::move(std::string(""));
}

bool cpuset::setcpus(const std::string &cpulist)
{
    std::string file = "/cpuset.cpus"; /* FIXME */

    char *buf = ::strdup(cpulist.c_str()); 
    if (!buf) {
        char buferr[BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        warn("strdup() %s %s\n", cpulist.c_str(), buferr);
        return false;
    }

    size_t sz = ::strlen(buf);

    ssize_t nr = util::write_file(file.c_str(), buf, sz);
    if (nr != sz) {
        warn("write() %s %zd %zu\n", buf, nr, sz); 
        free(buf);
        return false;
    }

    free(buf);
    return true;
}

} /* namespace cgroup */
