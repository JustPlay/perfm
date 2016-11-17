/**
 * perfm_pmu.hpp - interface functions for pmu
 *
 */
#ifndef __PERFM_PMU_HPP__
#define __PERFM_PMU_HPP__

#include <perfmon/pfmlib.h> /* libpfm4 */

namespace perfm {

bool pmu_is_available(pfm_pmu_t pmu);

void pmu_list(bool pr_all = 0);

} /* namespace perfm */

#endif /* __PERFM_PMU_HPP_ */
