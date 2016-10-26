#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <vector>
#include <string>

#include <time.h>

#include "perfm_util.hpp"

namespace {

const char *pmu_type_desc[PFM_PMU_TYPE_MAX] = {
    "unknown type",
    "processor core",
    "processor socket-level",
    "generic os-provided"
};

} /* namespace */

namespace perfm {

std::string str_trim(const std::string &str)
{
    if (str.empty()) {
        return str;
    }

    std::string::size_type s = 0, e = str.size() - 1;

    while (s <= e && std::isspace(str[s])) {
        ++s;
    }

    while (e >= s && std::isspace(str[e])) {
        --e;
    }

    if (s <= e) {
        return str.substr(s, e - s + 1);
    }
    
    return "";
}

// @argv  same as 'argv' in main()
// @argc  same as 'argc' in main()
// @trg   pointer to a '\0' ended C string
int str_find(char **argv, int argc, const char *trg)
{
    if (!trg || !argv || argc <= 0) {
        return -1;
    }

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(trg, argv[i]) == 0) {
            return i;
        }  
    }

    return -1;
}

void nanoseconds_sleep(double seconds, bool use_abs_clock)
{
    long int sec  = static_cast<long int>(seconds);
    long int nsec = static_cast<long int>((seconds - sec) * 1000000000);

    if (!use_abs_clock) {  /* relative time sleep */
        struct timespec tv[2] = {
            {
                .tv_sec  = sec,
                .tv_nsec = nsec,
            },

            {
                .tv_sec  = 0,
                .tv_nsec = 0,
            }
        };     

        int req = 0;
        int rem = 1;
        while (-1 == ::nanosleep(&tv[req], &tv[rem])) {
            if (EINTR == errno) {
                req ^= rem; rem ^= req; req ^= rem;
                continue;
            } else {
                perfm_warning("Call to nanosleep() failed, remaining %ld(ns)\n", tv[rem].tv_sec * 1000000000 + tv[rem].tv_nsec);
                break;
            }
        }

    } else {  /* absolute time sleep */
        perfm_error("%s\n", "NOT implemented");
    }
}

bool pmu_is_available(pfm_pmu_t pmu)
{
    pfm_pmu_info_t pinfo;

    memset(&pinfo, 0, sizeof(pinfo));

    pfm_err_t ret = pfm_get_pmu_info(pmu, &pinfo);

    return ret == PFM_SUCCESS ? pinfo.is_present : 0;
}

void pr_pmu_list(bool pr_all)
{
    FILE *fp = stdout;

    pfm_pmu_info_t pinfo;
    int i = 0;
    pfm_err_t ret = PFM_SUCCESS;

    memset(&pinfo, 0, sizeof(pinfo));
    
    if (pr_all) {
        int nr_supported_pmus   = 0;
        int nr_supported_events = 0;

        // Supported PMUs
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


    // Available PMUs
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

double mround(double number, double multiple)
{
    if (multiple == 0) {
        return number;
    }

    // http://en.cppreference.com/w/cpp/numeric/math/round
    // http://en.cppreference.com/w/cpp/numeric/math/ceil

    return std::round(number / multiple) * multiple;
}

std::vector<std::string> explode(const std::string &delimiter, const std::string &str, size_t limit)
{
    if (limit == 0 && delimiter == "") {
        return std::move(std::vector<std::string>({ str }));
    }

    std::vector<std::string> result;

    std::string::size_type search_pos = 0;
    std::string::size_type target_pos = 0;

    do {
        target_pos = str.find_first_of(delimiter, search_pos);

        if (target_pos != std::string::npos) {
            result.push_back(str.substr(search_pos, target_pos - search_pos));

            if (--limit) {
                search_pos = target_pos + delimiter.size();
            } else {
                search_pos = target_pos;
            }
        } else {
            result.push_back(str.substr(search_pos));
            search_pos = std::string::npos;
            break;
        }
    } while (limit && search_pos < str.size());

    if (search_pos < str.size()) {
        result.push_back(str.substr(search_pos));
    }

    return std::move(result);
}

} /* namespace perfm */
