#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
    fprintf(stderr, "perfm - A performance monitor, sampler, analyzer (powered by libpfm4 & perf_event).\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "        %s [general-options] command <command-options>\n", cmd);
    fprintf(stderr, "\n");

    fprintf(stderr, "General Options:\n");
    fprintf(stderr, "  -h, --help                        Display this help information.\n");
    fprintf(stderr, "  -V, --version                     Display version information.\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  monitor                           perfm will run in counting mode.\n");
    fprintf(stderr, "  sample                            perfm will run in sampling mode.\n");
    fprintf(stderr, "  analyze                           analyze the collected data.\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Commandline Options for: monitor\n");
    fprintf(stderr, "  -l, --loop <loops>                The # of times each event set is monitored.\n");
    fprintf(stderr, "  -t, --time <interval>             Time (s) that an event set is monitored.\n");
    fprintf(stderr, "  -e, --event <event1,event2;...>   Event list to be monitored.\n");
    fprintf(stderr, "  -i, --input <input file path>     Input file for perfm.\n");
    fprintf(stderr, "  -o, --output <output file path>   Output file.\n");
    fprintf(stderr, "  -c, --cpu <cpu>                   Which CPU to monitor; -1 for all CPUs.\n");
    fprintf(stderr, "  -p, --pid <pid>                   Which PID to monitor; -1 for all PIDs.\n");
    fprintf(stderr, "  -v, --verbose                     Run perfm in verbose mode.\n");
    fprintf(stderr, "  -m, --plm <plm string>            Privilege level mask.\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Commandline Options for: analyze\n");
    fprintf(stderr, "  -i, --input <input file path>     Input file for perfm.\n");
    fprintf(stderr, "  -o, --output <output file path>   Output file.\n");
}

// @trg   pointer to a '\0' ended C string
// @argc  same as 'argc' in main()
// @argv  same as 'argv' in main()
int find(const char *trg,  char **argv, int argc)
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

int options_t::parse_cmd_args(int argc, char **argv) 
{
    char ch;
    int  perfm_comm_switch_id = 0;  /* whether the 'command switch option' was provided by the user,
                                     * - if provided correctly, a positive value which show the position in argv
                                     * - else, zero
                                     */

    { /* Step 1. Parse Command Switch */
        for (int m = 0; m < PERFM_RUNNING_MODE_MAX; ++m) {
            int p = find(perfm_comm_switch[m], argv, argc);
            if (p != -1) {
                perfm_options.__running_mode = static_cast<perfm_running_mode_t>(m);
                perfm_comm_switch_id         = p;
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
                return CMD_ARGS_USAGE;

            case 'V':
                return CMD_ARGS_VERSION;

            default:
                return CMD_ARGS_ERROR;
            }
        }

        if (!perfm_comm_switch_id) {
            return CMD_ARGS_USAGE;
        }
    }

    /* Step 3. Parse Command-line Options */
    if (perfm_options.running_mode() == PERFM_RUNNING_MODE_MONITOR) {
        // Case 1. runnning_mode: monitor

        const struct option comm_longopts[] = {
            {"verbose", no_argument,       NULL, 'v'},
            {"loop",    required_argument, NULL, 'l'},
            {"time",    required_argument, NULL, 't'},
            {"event",   required_argument, NULL, 'e'},
            {"input",   required_argument, NULL, 'i'},
            {"output",  required_argument, NULL, 'o'},
            {"cpu",     required_argument, NULL, 'c'},
            {"plm",     required_argument, NULL, 'm'},
            {"pid",     required_argument, NULL, 'p'},
            {NULL,      no_argument,       NULL,  0 },
        };

        const char *comm_opts= "vl:t:e:i:o:c:m:p:";

        while ((ch = getopt_long(argc - perfm_comm_switch_id, argv + perfm_comm_switch_id, comm_opts, comm_longopts, NULL)) != -1) {
            switch(ch) {
            case 't':
                this->__interval = std::stod(optarg);
                break;

            case 'l':
                this->__loops = std::atoi(optarg);
                break;

            case 'i':
                this->__in_file = std::move(std::string(optarg));
                this->__in_fp   = ::fopen(optarg, "r");
                if (!this->__in_fp) {
                    char err_buf[ERR_BUFSIZ] = { '\0' };
                    strerror_r(errno, err_buf, sizeof(err_buf));
                    perfm_error("Error occured when opening the input file: %s, %s\n", optarg, err_buf);
                }
                break;

            case 'o':
                this->__out_file = std::move(std::string(optarg));
                this->__out_fp   = ::fopen(optarg, "w");
                if (!this->__out_fp) {
                    char err_buf[ERR_BUFSIZ] = { '\0' };
                    strerror_r(errno, err_buf, sizeof(err_buf));
                    perfm_error("Error occured when opening the output file: %s, %s\n", optarg, err_buf);
                }
                break;

            case 'e':
                this->ev_groups = explode(";", optarg, options_t::nr_group_supp());
                break;

            case 'c':
                this->__cpu = std::atoi(optarg);
                break;

            case 'm':
                this->__plm = std::move(std::string(optarg));
                break;

            case 'p':
                this->__pid = std::atoi(optarg);
                break;

            case 'v':
                this->__verbose = 1;
                break;

            default:
                return CMD_ARGS_ERROR;
            }

            if (this->__interval < 0) {
                this->__interval = 1;  /* 1 second */
            }

            if (this->__loops <= 0) {
                this->__loops = 5;     /* 5 loops */
            }
        }
    
    } else if (perfm_options.running_mode() == PERFM_RUNNING_MODE_SAMPLE) {
        perfm_error("%s\n", "TODO");    
    } else if (perfm_options.running_mode() == PERFM_RUNNING_MODE_ANALYZE) {
        perfm_error("%s\n", "TODO");    
    } else {
        perfm_error("%s\n", "This should NOT happen!");    
    }

    return CMD_ARGS_VALID;
}

void options_t::pr_options() const
{
    FILE *fp = stdout;
    int rmod = this->running_mode();

    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- The global configure options for this run of perfm: -\n");
    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- perfm will run in mode: %7s                     -\n", perfm_comm_switch[rmod]);
    fprintf(fp, "-------------------------------------------------------\n");
    fprintf(fp, "- Time (s) that an event set is monitored  : %lf(s)\n", this->interval());
    fprintf(fp, "- # of times each event set is monitored   : %d\n",     this->loops());
    fprintf(fp, "- Process/thread to monitor (pid/tid)      : %d\n",     this->pid());
    fprintf(fp, "- Processor to monitor                     : %d\n",     this->cpu());
    fprintf(fp, "- Input configure file                     : %s\n",     this->__in_file.c_str());      
    fprintf(fp, "- Output result file                       : %s\n",     this->__out_file.c_str());      
    fprintf(fp, "- Privilege level mask                     : %s\n",     this->plm().c_str());
    fprintf(fp, "- Total event groups to monitor            : %lu\n",    this->nr_groups());
    fprintf(fp, "-------------------------------------------------------\n");

    int i = 0;
    for (const auto &grp : ev_groups) {
        auto ev_list = explode(",", grp, options_t::nr_event_supp()); 
        
        fprintf(fp, "- Event group #%d (%lu events)\n", i++, ev_list.size());
        for (const auto &ev : ev_list) {
            fprintf(fp, "\t%s\n", ev.c_str());
        }

        fprintf(fp, "\n");
    }
}

} /* namespace perfm */

