/**
 * node.hpp - interface for handling numa-node issue
 *
 */
#ifndef __NODE_HPP_
#define __NODE_HPP_

#include <cstdio>
#include <memory>
#include <cstdlib>

namespace numa {

const std::string numacfg("/sys/devices/system/node/"); // numa config directory

class nodelist;
class node {
    friend class nodelist;

public:
    using ptr_t = std::shared_ptr<node>;

    static ptr_t creat() {
        return ptr_t(new node);
    }

    ~node() { }

private:
    node() = default;

public:
    int nid() const {
        return id;
    }

    std::string cpulist() const;

private:
    int id;

};

class nodelist {

public:
    using ptr_t = std::shared_ptr<nodelist>;

    static ptr_t creat() {
        return ptr_t(new nodelist);
    }

    ~nodelist() { }

    size_t init();

    size_t size() const {
        return nlist.size();
    }

private:
    nodelist() = default;

private:
    std::vector<node::ptr_t> nlist;
};

} /* namespace numa */

#endif /* __NODE_HPP_ */
