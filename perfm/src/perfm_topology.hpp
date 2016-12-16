/**
 * perfm_topology.hpp - processor topology interface
 *
 */

#ifndef __PERFM_TOPOLOGY_HPP__
#define __PERFM_TOPOLOGY_HPP__

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <bitset>

namespace perfm {

// On x86 platform, in the directory "/sys/devices/system/cpu/" 
// 1. possible   - 
// 2. present    - list of processors available on this system
// 3. online     - list of processors currently online
// 4. offline    - list of processors currently offline
// 5. kernel_max - the maximum number of processors support by kernel 
// 6. cpuX       - directory for cpu X
//
// in the cpuX directory
// 1. online   - 0 or 1 (cpu0 is unplugable, so there do not exist the online file)
// 2. cache    - cpu's cache (exist if cpuX is online, otherwise not)
// 3. cpufreq  - frequency scaling & setting (exist if cpuX is online, otherwise not)
// 4. topology - topology for smt, core, socket related to cpuX (exist if cpuX is online, otherwise not)
//
// in cpuX/cache
// 1. index0 - L1 data
// 2. index1 - L1 code
// 2. index2 - L2 unified
// 3. index3 - L3 unified
//
// 4. shared_cpu_list - threads/processors which share a physical core (list format)
// 5. shared_cpu_map  - threads/processors which share a physical core (bitmap format)
// 6. type & size     - cache's type & size
//
// in cpuX/topology
// 1. core_id              - physical core (uniq within socket, not uniq in the whole system; core id maybe discontinuous)
// 2. thread_siblings_list - threads which share a physical core (same as shared_cpu_list in cpuX/cache/index[0,1,2,3])
// 3. thread_siblings      - same as above
// 4. physical_package_id  - socket id
// 5. core_siblings_list   - cpus/processors on this socket (processor, not physical core)
// 6. core_siblings        - same as above
//
// nearly all the above entities will be affected when doing processor hotplug

class topology {
public:
    static constexpr size_t NR_CPU_MAX = 512;

public:
    using ptr_t = std::shared_ptr<topology>;

    static ptr_t alloc();

    void build();
    void print(const char *filp = NULL);

private:
    void build_cpu_present_list();
    void build_cpu_online_list();
    void build_cpu_topology();

    void processor_online() const;
    void processor_offline() const;
    void processor_hotplug(int cpu, int action) const;

private:
    static const std::string cpu_directory;

    /* for now, we assume the following limits:
     * - the maximum # of threads/core is 8
     * - the maximum # of cores/socket is 64
     * - the maximum # of socket is 8
     */
    static constexpr size_t _nr_max_smt_per_core = 8;
    static constexpr size_t _nr_max_core_per_skt = 64;
    static constexpr size_t _nr_max_socket       = 8; 
                                               
    size_t _nr_cpu;         /* number of cpus/processors/threads */
    size_t _nr_core;        /* number of physical cores */
    size_t _nr_socket;      /* number of sockets */

    size_t _nr_onln_cpu;    /* number of online cpus/processors/threads */
    size_t _nr_onln_core;   /* number of online cores */
    size_t _nr_onln_socket; /* number of online sockets */

    std::bitset<NR_CPU_MAX> _cpu_present_list;
    std::bitset<NR_CPU_MAX> _cpu_online_list;
    #define cpu_present(cpu) _cpu_present_list.test((cpu)) /* is (logical) processor @cpu presented ? */
    #define cpu_online(cpu)  _cpu_online_list.test((cpu))  /* is (logical) processor @cpu online ? */

    std::bitset<_nr_max_socket> _socket_present_list;
    std::bitset<_nr_max_socket> _socket_online_list;
    #define skt_present(skt) _socket_present_list.test((skt)) /* is socket presented ? */
    #define skt_online(skt)  _socket_online_list.test((skt))  /* is socket online ? */

    using _core2thrds_map_t = std::array<int, _nr_max_smt_per_core>;  

    std::tuple<bool, int, _core2thrds_map_t> _topology[_nr_max_socket][_nr_max_core_per_skt];           
    #define is_core_exist(skt, core) std::get<0>(_topology[(skt)][(core)]) /* core (physical) exist? core's id may be discontinuous */
    #define nr_core_thrds(skt, core) std::get<1>(_topology[(skt)][(core)]) /* how many threads (logical) share this core */
    #define threads_array(skt, core) std::get<2>(_topology[(skt)][(core)]) /* processor/cpu (logical) list on this core (physical) */

    std::array<std::pair<int, int>, NR_CPU_MAX> _cpu; /* subscript is (logical) processor's id
                                                       * array type is: <core, socket>
                                                       */
    #define processor_core(c)   _cpu[(c)].first
    #define processor_socket(c) _cpu[(c)].second
};

} /* namespace perfm */

#endif /* __PERFM_TOPOLOGY_HPP__ */
