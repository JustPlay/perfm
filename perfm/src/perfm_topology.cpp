#include "perfm_util.hpp"
#include "perfm_option.hpp"
#include "perfm_topology.hpp"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <set>
#include <algorithm>

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
    _socket_present_list.reset();
    _socket_online_list.reset();

    for (unsigned int s = 0; s < _nr_max_socket; ++s) {
        for (unsigned int c = 0; c < _nr_max_core_per_skt; ++c) {
            is_core_exist(s, c) = false;
            nr_core_thrds(s, c) = 0;
        }
    }

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
            if (!cpu_present(c)) {
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
    const std::string filp_core   = "/topology/core_id";
    const std::string filp_socket = "/topology/physical_package_id";
    
    std::string filp;
    int core_id;
    int socket;

    // build the '<socket, core> => processors' map
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
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
        if (!skt_present(socket)) {
            ++_nr_socket;
            _socket_present_list.set(socket);
        }

        if (!is_core_exist(socket, core_id)) {
            is_core_exist(socket, core_id) = true;
            ++_nr_core;
        }

        threads_array(socket, core_id)[nr_core_thrds(socket, core_id)++] = c;
    }

    // build the 'processor => <core, socket>' map
    for (unsigned int s = 0, n = 0; n < _nr_socket && s < _nr_max_socket; ++s) {
        if (!skt_present(s)) {
            continue;
        }

        ++n;

        for (unsigned int c = 0; c < _nr_max_core_per_skt; ++c) {
            if (!is_core_exist(s, c)) {
                continue;
            }

            for (int p = 0; p < nr_core_thrds(s, c); ++p) {
                _cpu[threads_array(s, c)[p]] = std::make_pair(c, s);
            }
        }
    }

    std::set<std::pair<int, int>> flag;
    for (unsigned int c = 0, n = 0; n < _nr_onln_cpu; ++c) {
        if (!cpu_online(c)) {
            continue;
        }

        ++n;

        if (!skt_online(processor_socket(c))) {
            _socket_online_list.set(processor_socket(c));
            ++_nr_onln_socket;
        }

        if (flag.find(std::make_pair(processor_core(c), processor_socket(c))) == flag.end()) {
            ++_nr_onln_core;
            flag.insert(std::make_pair(processor_core(c), processor_socket(c)));
        }
    }
}

void topology::processor_online() const 
{   //
    // put all presented processor online
    //
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue; 
        }

        ++n;

        if (!cpu_online(c)) {
            processor_hotplug(c, +1);
        }
    }
}

void topology::processor_offline() const
{   //
    // offline processors not in the _cpu_online_list
    //
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }

        ++n;

        if (!cpu_online(c)) {
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

void topology::print(const char *filp)
{
    FILE *fp = stdout;

    if (filp) {
        fp = ::fopen(filp, "w");
        if (!fp) {
            fp = stdout;
            perfm_warn("failed to open %s, will use stdout\n", filp);
        }
    }

    fprintf(fp, "------------------------------------------------------\n");
    fprintf(fp, "- Number of sockets (online/total)          : %zu/%zu\n", _nr_onln_socket, _nr_socket);
    fprintf(fp, "- Number of physical cores (online/total)   : %zu/%zu\n", _nr_onln_core, _nr_core);
    fprintf(fp, "- Number of logical cores (online/total)    : %zu/%zu\n", _nr_onln_cpu, _nr_cpu);
    fprintf(fp, "- Physical cores per socket                 : %zu\n", _nr_core / _nr_socket);
    fprintf(fp, "- Threads (logical cores) per physical core : %zu\n", _nr_cpu / _nr_core); 
    fprintf(fp, "------------------------------------------------------\n");
    fprintf(fp, "\n");

    auto compute_width = [](int a, int b, int c) -> int {
        a = a > b ? a : b;
        a = a > c ? a : c;

        if (a < 10) {
            return 2;
        }

        if (a >= 10 && a < 100) {
            return 3;
        }

        return 4;
    };

    int column_width[_nr_cpu];

    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }

        column_width[n++] = compute_width(c, processor_core(c), processor_socket(c));
    }

    fprintf(fp, "Processor presented: %2zu - ", _nr_cpu);
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;
        
        switch (compute_width(c, 0, 0)) {
        case 2:
            fprintf(fp, "%-2u", c);
            break;
        case 3:
            fprintf(fp, "%-3u", c);
            break;
        case 4:
            fprintf(fp, "%-4u", c);
            break;
        }
    }
    fprintf(fp, "\n");

    fprintf(fp, "Processor online   : %2zu - ", _nr_onln_cpu);
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;

        if (!cpu_online(c)) {
            switch (compute_width(c, 0, 0)) {
            case 2:
                fprintf(fp, "%-2s", "*");
                break;
            case 3:
                fprintf(fp, "%-3s", "*");
                break;
            case 4:
                fprintf(fp, "%-4s", "*");
                break;
            }
        } else {
            switch (compute_width(c, 0, 0)) {
            case 2:
                fprintf(fp, "%-2u", c);
                break;
            case 3:
                fprintf(fp, "%-3u", c);
                break;
            case 4:
                fprintf(fp, "%-4u", c);
                break;
            }
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "\n");

    // processor list
    fprintf(fp, "Processor: ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;

        switch (column_width[n]) {
        case 2:
            fprintf(fp, "%2d", c);
            break;
        case 3:
            fprintf(fp, "%3d", c);
            break;
        case 4:
            fprintf(fp, "%4d", c);
            break;
        }
    }
    fprintf(fp, "\n");

    // physical core list
    fprintf(fp, "Core id:   ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;

        switch (column_width[n]) {
        case 2:
            fprintf(fp, "%2d", processor_core(c));
            break;
        case 3:
            fprintf(fp, "%3d", processor_core(c));
            break;
        case 4:
            fprintf(fp, "%4d", processor_core(c));
            break;
        }
    }
    fprintf(fp, "\n");

    // socket list
    fprintf(fp, "Socket id: ");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;

        switch (column_width[n]) {
        case 2:
            fprintf(fp, "%2d", processor_socket(c));
            break;
        case 3:
            fprintf(fp, "%3d", processor_socket(c));
            break;
        case 4:
            fprintf(fp, "%4d", processor_socket(c));
            break;
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "\n");

    fprintf(fp, "------------------------------------------\n");
    fprintf(fp, "[Processor] - [Core] - [Socket] - [Online]\n");
    for (unsigned int c = 0, n = 0; n < _nr_cpu; ++c) {
        if (!cpu_present(c)) {
            continue;
        }
        ++n;

        fprintf(fp, "%7d       %4d     %5d       %4d\n", c, processor_core(c), processor_socket(c), cpu_online(c) ? 1 : 0);
    }
    fprintf(fp, "------------------------------------------\n");

    fp != stdout ? ::fclose(fp) : 0;
}

} /* namespace perfm */
