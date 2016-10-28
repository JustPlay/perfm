/**
 * perfm_option.hpp - configure options for perfm
 *
 */
#ifndef __PERFM_OPTION_HPP__
#define __PERFM_OPTION_HPP__

#include <vector>
#include <string>

#include <sys/types.h>

#define COMM_OPTIONS_ERROR    -1
#define COMM_OPTIONS_VALID     0
#define COMM_OPTIONS_USAGE     1
#define COMM_OPTIONS_VERSION   2

typedef enum {
    PERFM_RUNNING_MODE_MONITOR = 0,
    PERFM_RUNNING_MODE_SAMPLE,
    PERFM_RUNNING_MODE_ANALYZE,
    PERFM_RUNNING_MODE_MAX
} perfm_running_mode_t;

namespace perfm {

class options_t {

public:
    options_t() = default;

    ~options_t() {
        if (in_fp) {
            ::fclose(in_fp);
        }

        if (out_fp) {
            ::fclose(out_fp);
        }
    }

    int parse_cmd_args(int argc, char **argv);

    void pr_options() const;

    size_t nr_groups() const {
        return this->ev_groups.size();
    }

    const std::vector<std::string> &get_event_groups() const {
        return this->ev_groups;
    }

    static int nr_group_supp() {
        return options_t::max_nr_grps;
    }

    static int nr_event_supp() {
        return options_t::max_nr_evts;
    }

private:
    bool parse_evcfg_file();

public:
    bool rdfmt_timeing = true;   /* Always be true */
    bool rdfmt_evgroup = false;  /* Reading all events in a group at once by just one read() call,
                                  * NOTE: inherit does not work for some combinations of read_formats,
                                  *       such as PERF_FORMAT_GROUP, see perf_event_open(2) for more detail.
                                  */

    bool incl_children = false;  /* The counter should count events of child tasks, see perf_event_open(2) */
    bool start_on_exec = false;  /* Start the counter automatically after a call to exec(2) */

    double interval;             /* Time (s) that an event group is monitored */
    int loops;                   /* The number of times each event group is monitored */
    pid_t pid;                   /* Process/thread id to be monitored */

    int verbose = 0;             /* Verbose level */
    int cpu = -1;                /* The CPU to be monitered; -1 for all CPUs */
    std::string plm = "ukh";     /* Privilege level mask */

    std::string in_file;
    std::string out_file;

    FILE *in_fp  = NULL;
    FILE *out_fp = NULL;

    static constexpr int max_nr_grps = 64;  /* the maximum number of event groups support by perfm,
                                             * for now, the value is 64
                                             */

    static constexpr int max_nr_evts = 32;  /* the maximum number of events in one group support by perfm,
                                             * for now, we support a maximum of 32 events in one group,
                                             * which would be large enough
                                             */ 
    perfm_running_mode_t running_mode = PERFM_RUNNING_MODE_MONITOR;

    bool list_pmu_avail = false; /* list the available PMUs */
    bool ignore_error   = true;  /* ignore non-fatal error */

private:
    std::vector<std::string> ev_groups; /* event group list 
                                         * - events separated by ',' within the same group
                                         * - event group separated by ';'
                                         *
                                         * - for command line options, use: -e <event1,event2;event3,event4;...>
                                         * - elems in ev_groups are in the form: <event1,event2,...>
                                         */
};

extern options_t perfm_options; /* the global configure options */

void usage(const char *cmd = "perfm");

inline void version() {
    fprintf(stderr, "perfm - in developing...\n");
}


} /* namespace perfm */

#endif /* __PERFM_OPTION_HPP__ */
