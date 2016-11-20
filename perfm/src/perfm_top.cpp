
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <vector>
#include <memory>

namespace perfm {

class top {

public:
    using ptr_t = std::shared_ptr<top>;

public:
    static ptr_t alloc();

    virtual ~top() { }
};

} /* namespace perfm */
