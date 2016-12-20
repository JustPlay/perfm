/**
 * perfm_config.hpp - header file for global config, macro, limits
 *
 */
#ifndef __PERFM_CONFIG_HPP__
#define __PERFM_CONFIG_HPP__

namespace perfm {

/* Assumption: 
 * 1.
 * 2.
 * 
 * Limits:
 * 1. the maximum # of (logical) processors is 4096
 * 2. the maximum # of (physical) cores is 512
 * 3. the maximum # of threads/core is 8  (such as Power8/9, Sparc M7)
 * 4. the maximum # of cores/socket is 64 (such as FT2000/64)
 * 5. the maximum # of socket is 8
 * 6. for all the above, not only the total # of X, the X's id (if there exists an id) must also be less than the limit
 */
constexpr size_t NR_MAX_PROCESSOR    = 4096;
constexpr size_t NR_MAX_CORE         = 512;
constexpr size_t NR_MAX_SOCKET       = 8; 

constexpr size_t NR_MAX_SMT_PER_CORE = NR_MAX_PROCESSOR / NR_MAX_CORE;
constexpr size_t NR_MAX_CORE_PER_SKT = NR_MAX_CORE / NR_MAX_SOCKET;

} /* namespace perfm */

#endif /* __PERFM_CONFIG_HPP_ */
