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

options perfm_options; /* the global configure options */

const char *perfm_switch_str[PERFM_MAX] = {
    "monitor",
    "sample",
    "analyze",
    "top",
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
            "  -v, --verbose                     run perfm in verbose mode\n"
            "  -V, --version                     display version information\n"
            " --list-pmu                         list online PMUs\n"
            "\n"
           );

    fprintf(stderr,
            "Commands:\n"
            "  monitor                           perfm will run in counting mode\n"
            "  sample                            perfm will run in sampling mode\n"
            "  analyze                           analyze the collected data\n"
            "  top                               PMU-based CPU utilization tool\n"
            "\n"
           );

    fprintf(stderr,
            "Commandline Options for: monitor\n"
            "  -l, --loop <loops>                # of times each event set is monitored, defaults to 1\n"
            "  -t, --time <interval>             time (s) that an event set is monitored, defaults to 0.1s\n"
            "  -e, --event <event1,event2;...>   event list to be monitor\n"
            "                                    events separated by ',' within the same group\n"
            "                                    event groups separated by ';'\n"
            "  -i, --input <input file path>     event config file for perfm, will override -e & --event\n"
            "  -o, --output <output file path>   output file\n"
            "  -c, --cpu, --processor <CPUs>     CPUs to monitor, if not provided, select all (online) CPUs\n"
            "  -p, --pid <pid>                   PID to monitor, if not provided, any process/thread\n"
            "  -m, --plm <plm string>            privilege level mask\n"
            "  --incl-children                   TODO\n"
            "\n"
           );

    fprintf(stderr,
            "Commandline Options for: analyze\n"
            "  -i, --input <input file path>     input file for perfm.\n"
            "  -o, --output <output file path>   output file.\n"
            "\n"
           );

    fprintf(stderr,
            "Commandline Options for: top\n"
            "  -n, --max <iters>                 specifies the maximum # of iterations, or frames, top should produce before ending\n"
            "  -d, --delay <delay>               specifies the delay between screen updates, granularity: 0.01s\n"
            "  -c, --cpu-list <cpu-list>         CPUs to monitor, in the form: 1,2,3-4,5,8-16\n"
            "  -b, --batch-mode                  top in batch mode, useful for sending output from top to other programs or to a file\n"
            "\n"
           );

}

bool options::parse_event_file()
{
    this->egroups.clear();

    if (this->file_in == "") {
        return false;
    } 

    std::fstream fin(this->file_in, std::ios_base::in);
    std::string line;
    std::string group;
    
    while (std::getline(fin, line)) {
        if (line.empty()) {
            continue;
        }

        std::string event = str_trim(line);

        if (event.empty() || event[0] == '#') {
            continue;
        } 

        if (event == ";") {
            if (!group.empty()) {
                this->egroups.push_back(group);
            }
            group.clear();
        } else {
            group += group.empty() ? event : "," + event;
        }
    }

    return true;
}

void options::parse_general(int argc, char **argv)
{
    //
    // general options for perfm is optional
    //

    if (argc < 0 || (argc > 0 && !argv)) {
        this->error = true;
        return;
    }

    const char *opts = "hvV?";

    const struct option longopts[] = {
        {"help",     no_argument, NULL, 'h'},
        {"version",  no_argument, NULL, 'V'},
        {"verbose",  no_argument, NULL, 'v'},
        {"list-pmu", no_argument, NULL,  1 },
        { NULL,      no_argument, NULL,  0 },
    };

    char ch;
    while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
        switch(ch) {
        case '?':
        case 'h':
            this->usage = true;
            break;

        case 'V':
            this->version = true;
            break;

        case 'v':
            this->verbose = true;
            break;

        case  1:
            this->list_pmu = true;
            break;

        default:
            this->error = true;
            return;
        }
    }
}

void options::parse_monitor(int argc, char **argv)
{
    if (argc <= 0 || !argv) {
        this->error = true;
        return;
    }

    const char *opts= "l:t:e:i:o:c:m:p:";

    const struct option longopts[] = {
        {"loop",          required_argument, NULL, 'l'},
        {"time",          required_argument, NULL, 't'},
        {"event",         required_argument, NULL, 'e'},
        {"input",         required_argument, NULL, 'i'},
        {"output",        required_argument, NULL, 'o'},
        {"cpu",           required_argument, NULL, 'c'},
        {"processor",     required_argument, NULL, 'c'},
        {"plm",           required_argument, NULL, 'm'},
        {"pid",           required_argument, NULL, 'p'},
        {"incl-children", no_argument,       NULL,  1 },
        { NULL,           no_argument,       NULL,  0 },
    };

    char ch;
    while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
        switch(ch) {
        case 'l':
            try {
                this->loops = std::stoi(optarg);
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), optarg);
            }
            break;

        case 't':
            try {
                this->interval = std::stod(optarg);
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), optarg);
            }
            break;

        case 'e':
            this->egroups = str_split(optarg, ";", options::nr_group_max());
            break;

        case 'i':
            this->file_in = std::move(std::string(optarg));
            this->fp_in   = ::fopen(optarg, "r");
            if (!this->fp_in) {
                perfm_fatal("failed to open file %s, %s\n", optarg, strerror_r(errno, NULL, 0));
            }
            break;

        case 'o':
            this->file_out = std::move(std::string(optarg));
            this->fp_out   = ::fopen(optarg, "w");
            if (!this->fp_out) {
                perfm_fatal("failed to open file %s, %s\n", optarg, strerror_r(errno, NULL, 0));
            }
            break;

        case 'c':
            this->cpu_list = std::move(std::string(optarg));
            break;

        case 'm':
            this->plm = std::move(std::string(optarg));
         break;

        case 'p':
            try {
                this->pid = std::stoi(optarg);
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), optarg);
            }
            break;

        case 1:
            this->incl_children = true;
            break;

        default:
            this->error = true;
            return;
        }

        this->interval = this->interval > 0.01 ? this->interval : 0.01;
        this->loops = this->loops > 1 ? this->loops : 1;
    }

    if (this->file_in != "") {
        if (!parse_event_file()) {
            perfm_fatal("event parsing (%s) error, exit...\n", this->file_in.c_str());
        } 
    }
}

void options::parse_sample(int argc, char **argv)
{
    if (argc <= 0 || !argv) {
        this->error = true;
        return;
    }

    perfm_fatal("TODO\n");
}

void options::parse_analyze(int argc, char **argv)
{
    if (argc <= 0 || !argv) {
        this->error = true;
        return;
    }

    perfm_fatal("TODO\n");
}

void options::parse_top(int argc, char **argv)
{
    //
    // options for perfm.top is optional
    //

    if (argc < 0 || (argc > 0 && !argv)) {
        this->error = true;
        return;
    }

    const char *opts= "d:n:c:b";

    const struct option longopts[] = {
        {"delay",       required_argument, NULL, 'd'},
        {"max",         required_argument, NULL, 'n'},
        {"cpu",         required_argument, NULL, 'c'},
        {"processor",   required_argument, NULL, 'c'},
        {"batch-mode",  no_argument,       NULL, 'b'},
        { NULL,         no_argument,       NULL,  0 },
    };

    char ch;
    while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
        switch(ch) {
        case 'd':
            try {
                this->delay = std::stod(optarg);
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), optarg);
            }
            break;

        case 'n':
            try {
                this->iter = std::stoi(optarg);
            } catch (const std::exception &e) {
                perfm_fatal("%s %s\n", e.what(), optarg);
            }
            break;

        case 'c':
            this->cpu_list = std::move(std::string(optarg));
            break;

        case 'b':
            this->batch_mode = true;
            break;

        default:
            this->error = true;
            return;
        }

        if (this->delay < 0.01) {
            this->delay = 0.01; /* 10ms */
        }
    }
}

void options::parse(int argc, char **argv) 
{
    int  perfm_switch_id = 0;  /* whether the "command switch option" was provided by the user,
                                * - if provided correctly, a positive value which show it's position in argv
                                * - else, zero
                                */

    // step 1. parse command switch
    for (int s = 0; s < PERFM_MAX; ++s) {
        int id = str_find(argv, argc, perfm_switch_str[s]);
        if (id != -1) {
            this->perfm_switch = static_cast<perfm_switch_t>(s);
            perfm_switch_id    = id;
            break;
        }
    }

    // step 2. parse general options
    parse_general(perfm_switch_id ? perfm_switch_id : argc, argv);
    if (this->usage || this->version || this->error) {
        return;
    }

    // step 3. parse command-line options
    switch (this->perfm_switch) {
    case PERFM_MONITOR:
        parse_monitor(argc - perfm_switch_id, argv + perfm_switch_id);
        break;

    case PERFM_SAMPLE:
        parse_sample(argc - perfm_switch_id, argv + perfm_switch_id);
        break;

    case PERFM_ANALYZE:
        parse_analyze(argc - perfm_switch_id, argv + perfm_switch_id);
        break;
    
    case PERFM_TOP:
        parse_top(argc - perfm_switch_id, argv + perfm_switch_id);
        break;

    case PERFM_MAX:
        break;

    default:
        perfm_fatal("%s\n", "this should never happen!");    
    }
}

void options::print() const
{
    FILE *fp = stdout;
    int rmod = this->perfm_switch;

    switch (rmod) {
        case PERFM_MONITOR: {
            fprintf(fp, "-------------------------------------------------------\n");
            fprintf(fp, "- perfm will run in mode: %-24s    -\n", perfm_switch_str[rmod]);
            fprintf(fp, "-------------------------------------------------------\n");
            fprintf(fp, "- time that an event set is monitored   : %.2f(s)\n", this->interval);
            fprintf(fp, "- # of times each event set is monitored: %d\n",      this->loops);
            fprintf(fp, "- process/thread to monitor (pid/tid)   : %s\n",      this->pid == -1 ? "any" : std::to_string(this->pid).c_str());
            fprintf(fp, "- processor to monitor                  : %s\n",      this->cpu_list.empty() ? "any" : this->cpu_list.c_str());
            fprintf(fp, "- event config file                     : %s\n",      this->fp_in  ? this->file_in.c_str() : "none");
            fprintf(fp, "- output result file                    : %s\n",      this->fp_out ? this->file_out.c_str() : "stdout");
            fprintf(fp, "- privilege level mask                  : %s\n",      this->plm.c_str());
            fprintf(fp, "- # of event groups to monitor          : %lu\n",     this->nr_group());
            fprintf(fp, "-------------------------------------------------------\n");

            int i = 0;
            for (const auto &grp : egroups) {
                auto ev_list = str_split(grp, ",", options::sz_group_max()); 
                
                fprintf(fp, "- Event Group #%d (%lu events)\n", i++, ev_list.size());
                for (const auto &ev : ev_list) {
                    fprintf(fp, "\t%s\n", ev.c_str());
                }

                fprintf(fp, "\n");
            }

            if (!this->nr_group()) {
                fprintf(fp,
                        "\n"
                        "-------------------------------------------------------\n"
                        "- You *must* specify at least one event to moitor...  -\n"
                        "-------------------------------------------------------\n"
                        "\n"
                        );
                exit(EXIT_FAILURE);
            }

            break;
        }

        case PERFM_TOP: {
            fprintf(fp, "-------------------------------------------------------\n");
            fprintf(fp, "- perfm will run in mode: %-24s    -\n", perfm_switch_str[rmod]);
            fprintf(fp, "-------------------------------------------------------\n");
            break;                
        }

        case PERFM_MAX:
            break;
        
        default:
            perfm_fatal("un-known running mode, exit.\n");
    }
}

} /* namespace perfm */
