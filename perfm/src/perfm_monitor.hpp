/**
 * perfm_monitor.hpp - interface for perfm monitor
 *
 */
#ifndef __PERFM_MONITOR_HPP__
#define __PERFM_MONITOR_HPP__

#include <vector>
#include <memory>

#include "perfm_group.hpp"

namespace perfm {

class monitor {

public:
    using ptr_t = std::shared_ptr<monitor>;

public:
    static ptr_t alloc();

    ~monitor() { }

    void open();
    void close();

    void start();
    void stop();

private:
    monitor() { }

    void rr();
    int loop();

private:
    std::vector<group::ptr_t> _glist; /* event group list */
};

} /* namespace perfm */

#endif /* __PERFM_MONITOR_HPP__ */
