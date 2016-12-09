#include "perfm_util.hpp"
#include "perfm_topology.hpp"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <cstring>
#include <vector>
#include <string>
#include <fstream>

namespace perfm {

const std::string topology::cpu_directory("/sys/devices/system/cpu/");

topology::ptr_t topology::alloc()
{
    topology *p = nullptr;
    try {
        p = new topology;
    } catch (const std::bad_alloc &) {
        p = nullptr;
    }

    return ptr_t(p);
}

void topology::build()
{
    _nr_cpu         = 0;
    _nr_onln_cpu    = 0;

    _nr_core        = 0;
    _nr_onln_core   = 0;

    _nr_socket      = 0;
    _nr_onln_socket = 0;

    _cpu_present_list.reset();
    _cpu_online_list.reset();
    _socket_list.reset();

    build_cpu_present_list();
    build_cpu_online_list();

    //
    // for now, 
    // we need put all processors online before we can build the topology, then restore ...
    // 
    processor_online();

    build_cpu_topology();

    processor_offline();
}

void topology::build_cpu_present_list()
{
    const std::string filp = cpu_directory + "present";

    if (file_readable(filp.c_str())) {
        std::fstream fp(filp, std::ios::in);
        std::string line;

        if (!fp.good() || !std::getline(fp, line)) {
            perfm_fatal("error on opening/reading %s\n", filp.c_str());
        }

        auto slice = str_split(line, ",");
        for (size_t i = 0; i < slice.size(); ++i) {
            size_t del = slice[i].find("-");

            if (del != std::string::npos) {
                int fr = std::stoi(slice[i]);
                int to = std::stoi(slice[i].substr(del + 1));
                _nr_cpu += to - fr + 1;
                for (int c = fr; c <= to; ++c) {
                    _cpu_present_list.set(c);
                }
            } else {
                int c = std::stoi(slice[i]);
                _cpu_present_list.set(c);
                ++_nr_cpu;
            }
        }

    } else {
        DIR *dirp = ::opendir(cpu_directory.c_str()); 
        if (!dirp) {
            perfm_fatal("failed to open %s %s\n", cpu_directory.c_str(), strerror_r(errno, NULL, 0));
        }

        struct dirent *dp = NULL;
        errno = 0;

        while ((dp = ::readdir(dirp)) != NULL) {
            if (std::isdigit(dp->d_name[3]) && std::strncmp(dp->d_name, "cpu", 3) == 0) {
                int c = std::stoi(dp->d_name + 3);
                _cpu_present_list.set(c);
                ++_nr_cpu;
            } 
        }

        if (errno) {
            perfm_fatal("failed (may be partial successful) to read %s %s\n", cpu_directory.c_str(), strerror_r(errno, NULL, 0));
        }

        ::closedir(dirp);
    }
}

void topology::build_cpu_online_list()
{
    const std::string filp = cpu_directory + "online";

    if (file_readable(filp.c_str())) {
        std::fstream fp(filp, std::ios::in);
        std::string line;

        if (!fp.good() || !std::getline(fp, line)) {
            perfm_fatal("error on opening/reading %s\n", filp.c_str());
        }

        std::vector<std::string> slice = str_split(line, ",");

        for (size_t i = 0; i < slice.size(); ++i) {
            size_t del = slice[i].find("-");

            if (del != std::string::npos) {
                int fr = std::stoi(slice[i]);
                int to = std::stoi(slice[i].substr(del + 1));
                _nr_onln_cpu += to - fr + 1;
                for (int c = fr; c <= to; ++c) {
                    _cpu_online_list.set(c);
                }
            } else {
                int c = std::stoi(slice[i]);
                ++_nr_onln_cpu;
                _cpu_online_list.set(c);
            }
        }
    
    } else {
        for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
            if (!is_present(c)) {
                continue;
            }

            ++n;

            if (c == 0) {
                ++_nr_onln_cpu;
                _cpu_online_list.set(c);
                continue;
            }

            int is_onln = 0;
            const std::string path = cpu_directory + "cpu" + std::to_string(c) + "/online"; 
            std::fstream fp(path, std::ios::in);

            if (!fp.good() || !(fp >> is_onln)) {
                perfm_fatal("error on opening/reading %s\n", path.c_str());
            }

            if (is_onln) {
                ++_nr_onln_cpu;
                _cpu_online_list.set(c);
            }
        } 
    }
}

void topology::build_cpu_topology()
{
    for (int s = 0; s < _nr_max_socket; ++s) {
        for (int c = 0; c < _nr_max_core_per_skt; ++c) {
            is_core_exist(s, c) = false;
            nr_core_thrds(s, c) = 0;
        }
    }

    const std::string filp_core   = "/topology/core_id";
    const std::string filp_socket = "/topology/physical_package_id";
    
    std::string filp;
    int core_id;
    int socket;

    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue;
        } 

        ++n;

        // core_id
        {
            filp = cpu_directory + "cpu" + std::to_string(c) + filp_core;
            if (!file_readable(filp.c_str())) {
                perfm_fatal("no read permission to %s\n", filp.c_str()); 
            }

            std::fstream fp(filp, std::ios::in);
            if (!fp.good() || !(fp >> core_id)) {
                perfm_fatal("error on opening/reading %s\n", filp.c_str());
            }
        }

        // physical_package_id
        {
            filp = cpu_directory + "cpu" + std::to_string(c) + filp_socket;
            if (!file_readable(filp.c_str())) {
                perfm_fatal("no read permission to %s\n", filp.c_str()); 
            }

            std::fstream fp(filp, std::ios::in);
            if (!fp.good() || !(fp >> socket)) {
                perfm_fatal("error on opening/reading %s\n", filp.c_str());
            }
        }

        // do some recording
        is_core_exist(socket, core_id) = true;
        threads_array(socket, core_id)[nr_core_thrds(socket, core_id)++] = c;
    }
}

void topology::processor_online() const 
{   //
    // put all presented processor online
    //
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue; 
        }

        ++n;

        if (!is_online(c)) {
            processor_hotplug(c, +1);
        }
    }
}

void topology::processor_offline() const
{   //
    // offline processors not in the _cpu_online_list
    //
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue;
        }

        ++n;

        if (!is_online(c)) {
            processor_hotplug(c, -1);
        }
    }
}

void topology::processor_hotplug(int cpu, int action) const
{   //
    // we assume cpu0 is the boot cpu, and do not support hotplug
    //
    // @action < 0 : offline @cpu
    // @action > 0 : online  @cpu
    // @acrion = 0 : no operation
    //
    if (cpu <= 0 || action == 0) {
        return;
    }

    const std::string filp = cpu_directory + "cpu" + std::to_string(cpu) + "/online";

    if (!file_writable(filp.c_str())) {
        perfm_fatal("no write permission to %s\n", filp.c_str()); 
    }

    std::fstream fp(filp, std::ios::out); 
    if (!fp.good() || !(fp << (action < 0 ? 0 : 1))) {
        perfm_fatal("error on opening/writing %s\n", filp.c_str());
    }
}

void topology::print()
{
    FILE *fp = stdout;    

    fprintf(fp, "------------------------------------------------------\n");
    fprintf(fp, "- Number of sockets (online/total)          : %zu/%zu\n", _nr_onln_socket, _nr_socket);
    fprintf(fp, "- Number of physical cores (online/total)   : %zu/%zu\n", _nr_onln_core, _nr_core);
    fprintf(fp, "- Number of logical cores (online/total)    : %zu/%zu\n", _nr_onln_cpu, _nr_cpu);
    fprintf(fp, "- Physical cores per socket                 : %zu\n", _nr_core / _nr_socket);
    fprintf(fp, "- Threads (logical cores) per physical core : %zu\n", _nr_cpu / _nr_core); 
    fprintf(fp, "------------------------------------------------------\n");

    for (int s = 0; s < _nr_max_socket; ++s) {
        for (int c = 0; c < _nr_max_core_per_skt; ++c) {
            if (!is_core_exist(s, c)) {
                continue;
            }

            for (int p = 0; p < nr_core_thrds(s, c); ++p) {
                _cpu[p] = std::make_pair(c, s);
            }
        }
    }

    // processor list
    fprintf(fp, "processor: ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue;
        }

        ++n;

        fprintf(fp, "%-3d", c);
    }
    fprintf(fp, "\n");

    // physical core list
    fprintf(fp, "core id:   ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue;
        }

        ++n;

        fprintf(fp, "%-3d", _cpu[c].first);
    }
    fprintf(fp, "\n");

    // socket list
    fprintf(fp, "socket id: ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!is_present(c)) {
            continue;
        }

        ++n;

        fprintf(fp, "%-3d", _cpu[c].second);
    }
    fprintf(fp, "\n");
}

} /* namespace perfm */
