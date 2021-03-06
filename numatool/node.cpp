
#include "utils.hpp"
#include "node.hpp"

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <cstring>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

namespace numa {

std::string node::cpulist() const 
{
    std::string cpulst;
    std::string cpucfg = numacfg + "node" + std::to_string(nid()) + "/cpulist";

    if (access(cpucfg.c_str(), R_OK) != 0) {
        warn("access() %s\n", cpucfg.c_str());
        return std::move(cpulst);
    }     

    std::fstream fp(cpucfg, std::ios_base::in);

    if (std::getline(fp, cpulst)) {
        return std::move(cpulst);
    }
    
    return std::move(std::string(""));
}

size_t nodelist::init()
{
    size_t nr = 0;

    if (access(numacfg.c_str(), R_OK) != 0) {
        warn("access() %s", numacfg.c_str());        
        return 0;
    }

    DIR *dirp = ::opendir(numacfg.c_str());        
    if (!dirp) {
        char buferr[BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        warn("opendir() %s %s", numacfg.c_str(), buferr);        
        return 0;
    }

    struct dirent *dp = NULL;

    errno = 0;

    while ((dp = ::readdir(dirp)) != NULL) {
        if (std::strncmp("node", dp->d_name, sizeof("node")) == 0) {
            node::ptr_t np = node::creat();

            try {
                np->id = std::stoi(dp->d_name + sizeof("node"));             
            } catch (const std::invalid_argument &e) {
                warn("std::stoi() %s %s\n", dp->d_name, e.what()); 
                continue;
            } catch (const std::out_of_range &e) {
                warn("std::stoi() %s %s\n", dp->d_name, e.what()); 
                continue;
            }

            nlist.push_back(np);
            ++nr;
        }
    }

    if (errno) {
        char buferr[BUFERR] = { '\0' };
        strerror_r(errno, buferr, sizeof(buferr));
        warn("readdir() %s %s", numacfg.c_str(), buferr);        
    }

    return nr;
}

} /* namespace numa */
