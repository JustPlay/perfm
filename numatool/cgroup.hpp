/**
 * cgroup.hpp - interface for handling cgroup issue 
 *
 */
#ifndef __CGROUP_HPP_
#define __CGROUP_HPP_

#include <memory>

#include <sys/types.h>

namespace cgroup {

class cgroup {

public:
    using ptr_t = std::shared_ptr<cgroup>;

    static ptr_t creat() {
        return ptr_t(new cgroup);
    }

    static std::string is_supported();
    static std::string is_installed(const std::string &subsys = "");

    ~cgroup() { }

public:
    std::string root() const {
        return path;
    }

private:
    cgroup() = default;

public:

private:
    std::vector<std::string> system;       /* cgroup subsystem name list
                                            * - cpuset:
                                            * - memory:
                                            * - blkio:
                                            */

    std::string path = "/sys/fs/cgroup/";  /* where the cgroup filesystem is mounted,
                                            * defaults to '/sys/fs/cgroup/'
                                            */

};

class cpuset {

public:
    using ptr_t = std::shared_ptr<cpuset>;

    static ptr_t creat() {
        return ptr_t(new cpuset);
    }

    static std::string is_installed() {
        return cgroup::is_installed("cpuset");
    }

    ~cpuset() { }

private:
    cpuset() = default;

public:
    bool setcpus(const std::string &cpulist);
    bool setmems(const std::string &memlist);
    bool settask(pid_t pid);

    std::string getcpus() const;
    std::string getmems() const;
    std::string gettask() const;

private:
    cpuset::ptr_t parent;

    std::string path = "cpuset/";  /* relative path to it's parent,
                                    * default to '/sys/fs/cgroup/cpuset' (full path)
                                    */
};

} /* namespace cgroup */

#endif /* __CGROUP_HPP_ */
