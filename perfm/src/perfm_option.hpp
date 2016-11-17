/**
 * perfm_option.hpp - configure options for perfm
 *
 */
#ifndef __PERFM_OPTION_HPP__
#define __PERFM_OPTION_HPP__

#include <vector>
#include <string>

#include <sys/types.h>

typedef enum {
    PERFM_MONITOR = 0,
    PERFM_SAMPLE,
    PERFM_ANALYZE,
    PERFM_MAX
} perfm_switch_t;

namespace perfm {

void usage(const char *cmd = "perfm");

inline void version() {
    fprintf(stderr, "perfm - in developing...\n");
}

class options_t {

public:
    options_t() = default;

    ~options_t() {
        if (fp_in) {
            ::fclose(fp_in);
        }

        if (fp_out) {
            ::fclose(fp_out);
        }
    }

    void parse(int argc, char **argv);
    void print() const;

    size_t nr_group() const {
        return this->ev_groups.size();
    }

    const std::vector<std::string> &get_event_group() const {
        return this->ev_groups;
    }

    static int nr_group_max() {
        return options_t::max_nr_group;
    }

    static int sz_group_max() {
        return options_t::max_sz_group;
    }

private:
    bool parse_evcfg_file();

public:
    //
    // general options for perfm
    //
    perfm_switch_t perfm_switch = PERFM_MAX;

    bool error    = false;
    bool usage    = false;
    bool version  = false;
    bool verbose  = false; /* verbose level */
    bool list_pmu = false; /* list available PMUs */
    bool skip_err = false; /* ignore non-fatal error */

    //
    // options for perfm.monitor
    // 
    bool rdfmt_timeing = true;   /* always be true */
    bool rdfmt_evgroup = false;  /* reading all events in a group at once by just one read() call,
                                  * NOTE: inherit does not work for some combinations of read_formats,
                                  *       such as PERF_FORMAT_GROUP, see perf_event_open(2) for more detail.
                                  */

    bool incl_children = false;  /* the counter should count events of child tasks, see perf_event_open(2) */
    bool start_on_exec = false;  /* start the counter automatically after a call to exec(2) */

    double interval;             /* time (s) that an event group is monitored */
    int loops;                   /* the number of times each event group is monitored */
    pid_t pid;                   /* process/thread id to be monitored */
    int cpu = -1;                /* the CPU to be monitered; -1 for all CPUs */
    std::string plm = "ukh";     /* privilege level mask */

    std::string in_file;
    std::string out_file;

    FILE *fp_in  = NULL;
    FILE *fp_out = NULL;

    static constexpr int max_nr_group = 64;  /* the maximum number of event groups support by perfm,
                                              * for now, the value is 64
                                              */

    static constexpr int max_sz_group = 32;  /* the maximum number of events in one group support by perfm,
                                              * for now, we support a maximum of 32 events in one group,
                                              * which would be large enough
                                              */ 
    //
    // options for perfm.sample
    // 

    //
    // options for perfm.analyze
    // 
private:
    std::vector<std::string> ev_groups; /* event group list 
                                         * - events separated by ',' within the same group
                                         * - event group separated by ';'
                                         *
                                         * - for command line options, use: -e <event1,event2;event3,event4;...>
                                         * - elems in ev_groups are in the form: <event1,event2,...>
                                         */
};

extern options_t perfm_options; /* the global configure options for perfm */

} /* namespace perfm */

#endif /* __PERFM_OPTION_HPP__ */
