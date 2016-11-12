#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <errno.h>
#include <getopt.h>

#include "perfm_util.hpp"
#include "perfm_option.hpp"

namespace perfm {

options_t perfm_options; /* the global configure options */

const char *perfm_comm_switch[PERFM_RUNNING_MODE_MAX] = {
    "monitor",
    "sample",
    "analyze",
};

void usage(const char *cmd) 
{
    fprintf(stderr,
            "perfm - A performance monitor, sampler, analyzer (powered by libpfm4 & perf_event).\n\n"
            "Usage:\n"
            "        %s [general-options] command <command-options>\n\n", cmd
            );

    fprintf(stderr,
            "General Options:\n"
            "  -h, --help                        display this help information\n"
            "  -V, --version                     display version information\n"
            "\n"
           );

    fprintf(stderr,
            "Commands:\n"
            "  monitor                           perfm will run in counting mode\n"
            "  sample                            perfm will run in sampling mode\n"
            "  analyze                           analyze the collected data\n"
            "\n");

    fprintf(stderr,
            "Commandline Options for: monitor\n"
            "  -l, --loop <loops>                # of times each event set is monitored\n"
            "  -t, --time <interval>             time (s) that an event set is monitored, granularity: 0.01s\n"
            "  -e, --event <event1,event2;...>   event list to be monitored\n"
            "  -i, --input <input file path>     event config file for perfm, will override -e & --event\n"
            "  -o, --output <output file path>   output file\n"
            "  -c, --cpu <cpu>                   which CPU to monitor, -1 for any CPUs\n"
            "  -p, --pid <pid>                   which PID to monitor, -1 for any PIDs\n"
            "  -v, --verbose                     run perfm in verbose mode\n"
            "  -m, --plm <plm string>            privilege level mask\n"
            "  --incl-children                   TODO\n"
            "\n");

    fprintf(stderr,
            "Commandline Options for: analyze\n"
            "  -i, --input <input file path>     input file for perfm.\n"
            "  -o, --output <output file path>   output file.\n"
            );
}

bool options_t::parse_evcfg_file()
{
    this->ev_groups.clear();

    if (this->in_file == "") {
        return false;
    } 

    std::fstream fin(this->in_file, std::ios_base::in);
    std::string line;
    std::string evgrp;
    
    while (std::getline(fin, line)) {
        std::string event = str_trim(line);

        if (event.empty() || event[0] == '#') {
            continue;
        } 

        if (event == ";") {
            if (!evgrp.empty()) {
                this->ev_groups.push_back(evgrp);
            }
            evgrp.clear();
        } else {
            evgrp += evgrp.empty() ? event : "," + event;
        }
    }

    return true;
}

int options_t::parse_cmd_args(int argc, char **argv) 
{
    char ch;
    int  perfm_comm_switch_id = 0;  /* whether the 'command switch option' was provided by the user,
                                     * - if provided correctly, a positive value which show the position in argv
                                     * - else, zero
                                     */

    { /* Step 1. Parse Command Switch */
        for (int m = 0; m < PERFM_RUNNING_MODE_MAX; ++m) {
            int p = str_find(argv, argc, perfm_comm_switch[m]);
            if (p != -1) {
                perfm_options.running_mode = static_cast<perfm_running_mode_t>(m);
                perfm_comm_switch_id       = p;
                break;
            }
        }
    }

    { /* Step 2. Parse General Options */
        const char *general_opts = "hV?";

        const struct option general_longopts[] = {
            {"help",    no_argument, NULL, 'h'},
            {"version", no_argument, NULL, 'V'},
            {NULL,      no_argument, NULL,  0 },
        };

        while ((ch = getopt_long(perfm_comm_switch_id ? perfm_comm_switch_id : argc, argv, general_opts, general_longopts, NULL)) != -1) {
            switch(ch) {
            case '?':
            case 'h':
                return COMM_OPTIONS_USAGE;

            case 'V':
                return COMM_OPTIONS_VERSION;

            default:
                return COMM_OPTIONS_ERROR;
            }
        }

        if (!perfm_comm_switch_id) {
            return COMM_OPTIONS_USAGE;
        }
    }

    /* Step 3. Parse Command-line Options */
    if (perfm_comm_switch_id == argc - 1) {
        return COMM_OPTIONS_ERROR;
    }

    if (perfm_options.running_mode == PERFM_RUNNING_MODE_MONITOR) {
        // Case 1. runnning_mode: monitor

        const struct option comm_longopts[] = {
            {"verbose",       no_argument,       NULL, 'v'},
            {"loop",          required_argument, NULL, 'l'},
            {"time",          required_argument, NULL, 't'},
            {"event",         required_argument, NULL, 'e'},
            {"input",         required_argument, NULL, 'i'},
            {"output",        required_argument, NULL, 'o'},
            {"cpu",           required_argument, NULL, 'c'},
            {"plm",           required_argument, NULL, 'm'},
            {"pid",           required_argument, NULL, 'p'},
            {"incl-children", no_argument,       NULL,  1 },
            {NULL,            no_argument,       NULL,  0 },
        };

        const char *comm_opts= "vl:t:e:i:o:c:m:p:";

        while ((ch = getopt_long(argc - perfm_comm_switch_id, argv + perfm_comm_switch_id, comm_opts, comm_longopts, NULL)) != -1) {
            switch(ch) {
            case 'v':
                this->verbose = 1;
                break;

            case 'l':
                this->loops = std::atoi(optarg);
                break;

            case 't':
                this->interval = std::stod(optarg);
                break;

            case 'e':
                this->ev_groups = str_split(optarg, ";", options_t::nr_group_supp());
                break;

            case 'i':
                this->in_file = std::move(std::string(optarg));
                this->in_fp   = ::fopen(optarg, "r");
                if (!this->in_fp) {
                    char *err = strerror_r(errno, NULL, 0);
                    perfm_fatal("failed to open file %s, %s\n", optarg, err);
                }
                break;

            case 'o':
                this->out_file = std::move(std::string(optarg));
                this->out_fp   = ::fopen(optarg, "w");
                if (!this->out_fp) {
                    char *err = strerror_r(errno, NULL, 0);
                    perfm_fatal("failed to open file %s, %s\n", optarg, err);
                }
                break;

            case 'c':
                this->cpu = std::atoi(optarg);
                break;

            case 'm':
                this->plm = std::move(std::string(optarg));
             break;

            case 'p':
                this->pid = std::atoi(optarg);
                break;

            case 1:
                this->incl_children = true;
                break;

            default:
                return COMM_OPTIONS_ERROR;
            }

            if (this->interval < 0.01) {
                this->interval = 1;  /* 1 second */
            }

            if (this->loops <= 0) {
                this->loops = 5;     /* 5 loops */
            }
        }

        if (this->in_file != "") {
            if (!parse_evcfg_file()) {
                perfm_fatal("event parsing (%s) error, exit...\n", this->in_file.c_str());
            } 
        }
    
    } else if (perfm_options.running_mode == PERFM_RUNNING_MODE_SAMPLE) {
        perfm_fatal("%s\n", "TODO");    
    } else if (perfm_options.running_mode == PERFM_RUNNING_MODE_ANALYZE) {
        perfm_fatal("%s\n", "TODO");    
    } else {
        perfm_fatal("%s\n", "this should never happen!");    
    }

    return COMM_OPTIONS_VALID;
}

void options_t::pr_options() const
{
    FILE *fp = stdout;
    int rmod = this->running_mode;

    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- perfm will run in mode: %-24s    -\n", perfm_comm_switch[rmod]);
    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- time that an event set is monitored   : %.2f(s)\n", this->interval);
    fprintf(fp, "- # of times each event set is monitored: %d\n",      this->loops);
    fprintf(fp, "- process/thread to monitor (pid/tid)   : %s\n",      this->pid == -1 ? "any" : std::to_string(this->pid).c_str());
    fprintf(fp, "- processor to monitor                  : %s\n",      this->cpu == -1 ? "any" : std::to_string(this->cpu).c_str());
    fprintf(fp, "- event config file                     : %s\n",      this->in_fp  ? this->in_file.c_str() : "none");      
    fprintf(fp, "- output result file                    : %s\n",      this->out_fp ? this->out_file.c_str() : "stdout");      
    fprintf(fp, "- privilege level mask                  : %s\n",      this->plm.c_str());
    fprintf(fp, "- # of event groups to monitor          : %lu\n",     this->nr_groups());
    fprintf(fp, "-------------------------------------------------------\n");

    int i = 0;
    for (const auto &grp : ev_groups) {
        auto ev_list = str_split(grp, ",", options_t::nr_event_supp()); 
        
        fprintf(fp, "- Event Group #%d (%lu events)\n", i++, ev_list.size());
        for (const auto &ev : ev_list) {
            fprintf(fp, "\t%s\n", ev.c_str());
        }

        fprintf(fp, "\n");
    }

    if (!this->nr_groups()) {
        fprintf(fp,
                "\n"
                "-------------------------------------------------------\n"
                "- You *must* specify at least one event to moitor...  -\n"
                "-------------------------------------------------------\n"
                "\n"
                );
        exit(EXIT_FAILURE);
    }
}

} /* namespace perfm */

