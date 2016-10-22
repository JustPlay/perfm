#ifndef __PERFM_OPTION_HPP__
#define __PERFM_OPTION_HPP__

#include <vector>
#include <string>

#include <sys/types.h>

#define CMD_ARGS_ERROR    -1
#define CMD_ARGS_VALID     0
#define CMD_ARGS_USAGE     1
#define CMD_ARGS_VERSION   2

typedef enum {
    PERFM_RUNNING_MODE_MONITOR = 0,
    PERFM_RUNNING_MODE_SAMPLE,
    PERFM_RUNNING_MODE_ANALYZE,
    PERFM_RUNNING_MODE_MAX
} perfm_running_mode_t;

namespace perfm {

class options_t {

public:
    int parse_cmd_args(int argc, char **argv);

    void pr_options() const;

    double interval() const {
        return this->__interval;
    }

    int loops() const {
        return this->__loops;
    }

    pid_t pid() const {
        return this->__pid;
    }

    int cpu() const {
        return this->__cpu;
    }

    std::string plm() const {
        return std::move(std::string(this->__plm));
    }

    int running_mode() const {
        return this->__running_mode;
    }

    size_t nr_groups() const {
        return this->ev_groups.size();
    }

    FILE *fp_input() const {
        return this->__in_fp;
    }

    FILE *fp_output() const {
        return this->__out_fp;
    }

    static int nr_group_supp() {
        return options_t::__max_nr_grps;
    }

    static int nr_event_supp() {
        return options_t::__max_nr_evts;
    }

    const std::vector<std::string> &get_event_groups() const {
        return this->ev_groups;
    }

public:
    bool rdfmt_timeing = true; /* Always be true */
    bool rdfmt_evgroup = true; /* Inherit does not work for some combinations of read_formats,
                                * such as PERF_FORMAT_GROUP, see perf_event_open(2) for more detail.
                                */

private:
    std::vector<std::string> ev_groups; /* event group list 
                                         * - events separated by ',' within the same group
                                         * - event group separated by ';'
                                         *
                                         * - for command line options, use: -e <event1,event2;event3,event4;...>
                                         * - elems in ev_groups are in the form: <event1,event2,...>
                                         */

    double __interval;          /* Time (s) that an event group is monitored */
    int __loops;                /* The number of times each event group is monitored */
    pid_t __pid;                /* Process/thread id to be monitored */

    int __verbose = 0;          /* Verbose level */
    int __cpu = -1;             /* The CPU to be monitered; -1 for all CPUs */
    std::string __plm = "ukh";  /* Privilege level mask */

    std::string __in_file;
    std::string __out_file;

    FILE *__in_fp  = NULL;
    FILE *__out_fp = NULL;

    static constexpr int __max_nr_grps = 64;  /* the maximum number of event groups support by perfm,
                                               * for now, the value is 64
                                               */

    static constexpr int __max_nr_evts = 32;  /* the maximum number of events in one group support by perfm,
                                               * for now, we support a maximum of 32 events in one group,
                                               * which would be large enough
                                               */ 
    perfm_running_mode_t __running_mode = PERFM_RUNNING_MODE_MONITOR;
};

extern options_t perfm_options; /* the global configure options */

void usage(const char *cmd = "perfm");

} /* namespace perfm */

#endif /* __PERFM_OPTION_HPP__ */
