#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <vector>
#include <string>

#include <time.h>

#include "perfm_util.hpp"
#include "perfm_pmu.hpp"

namespace {

const char *pmu_type_desc[PFM_PMU_TYPE_MAX] = {
    "unknown type",
    "processor core",
    "processor socket-level",
    "generic os-provided"
};

} /* namespace */

namespace perfm {

bool pmu_is_available(pfm_pmu_t pmu)
{
    pfm_pmu_info_t pinfo;

    memset(&pinfo, 0, sizeof(pinfo));

    pfm_err_t ret = pfm_get_pmu_info(pmu, &pinfo);

    return ret == PFM_SUCCESS ? pinfo.is_present : 0;
}

void pmu_list(bool pr_all)
{
    FILE *fp = stdout;

    pfm_pmu_info_t pinfo;
    int i = 0;
    pfm_err_t ret = PFM_SUCCESS;

    memset(&pinfo, 0, sizeof(pinfo));
    
    if (pr_all) {
        int nr_supported_pmus   = 0;
        int nr_supported_events = 0;

        // supported PMUs
        fprintf(fp, "-------------------------------------------------------\n");
        fprintf(fp, "- PMU models supported by perfm (powered by libpfm4): -\n");
        fprintf(fp, "-------------------------------------------------------\n");

        pfm_for_all_pmus(i) {
            ret = pfm_get_pmu_info(static_cast<pfm_pmu_t>(i), &pinfo);
            if (ret != PFM_SUCCESS) {
                continue;
            }

            nr_supported_pmus   += 1;
            nr_supported_events += pinfo.nevents;

            fprintf(fp, "- PMU id            : %d\n", i);
            fprintf(fp, "  PMU name          : %s\n", pinfo.name);
            fprintf(fp, "  PMU desc          : %s\n", pinfo.desc);
            fprintf(fp, "  PMU events        : %d\n", pinfo.nevents);
            fprintf(fp, "\n");
        }

        fprintf(fp, "- Total supported PMUs:   %d\n", nr_supported_pmus);
        fprintf(fp, "- Total supported EVENTs: %d\n", nr_supported_events);
    }


    // available PMUs
    if (pr_all) {
        fprintf(fp, "\n");
    }

    int nr_available_pmus   = 0;
    int nr_available_events = 0;

    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- PMU models supported by system (perf_event && cpu): -\n");
    fprintf(fp, "-------------------------------------------------------\n");

    pfm_for_all_pmus(i) {
        ret = pfm_get_pmu_info(static_cast<pfm_pmu_t>(i), &pinfo);    
        if (ret != PFM_SUCCESS) {
            continue;
        }

        if (pinfo.is_present) {
            if (pinfo.type >= PFM_PMU_TYPE_MAX) {
                pinfo.type = PFM_PMU_TYPE_UNKNOWN;
            }

            nr_available_pmus   += 1;
            nr_available_events += pinfo.nevents;

            fprintf(fp, "- PMU id            : %d\n", i);
            fprintf(fp, "  PMU name          : %s\n", pinfo.name);
            fprintf(fp, "  PMU desc          : %s\n", pinfo.desc);
            fprintf(fp, "  PMU events        : %d\n", pinfo.nevents);
            fprintf(fp, "  PMU max_encoding  : %d\n", pinfo.max_encoding);
            fprintf(fp, "  PMU generic cntrs : %d\n", pinfo.num_cntrs);
            fprintf(fp, "  PMU fixed cntrs   : %d\n", pinfo.num_fixed_cntrs);
            fprintf(fp, "  PMU type          : %s\n", pmu_type_desc[pinfo.type]);

            fprintf(fp, "\n");
        }
    }

    fprintf(fp, "- Total available PMUs:   %d\n", nr_available_pmus);
    fprintf(fp, "- Total available EVENTs: %d\n", nr_available_events);
    fprintf(fp, "\n");
}

} /* namespace perfm */
