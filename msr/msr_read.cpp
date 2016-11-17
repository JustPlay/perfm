#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <vector>
#include <string>

#include <inttypes.h>
#include <sys/types.h>

#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <libgen.h>

#include <errno.h>
#include <fcntl.h>

#include "msr_version.hpp"

#define program "msr_read"

#define msr_warn(fmt, ...) fprintf(stderr, program ": " fmt, ##__VA_ARGS__) 

namespace msr {

const struct option long_options[] = {
    {"help",      no_argument,       NULL, 'h'},
    {"version",   no_argument,       NULL, 'v'},
    {"all",       no_argument,       NULL, 'a'},
    {"processor", required_argument, NULL, 'p'},
    {"cpu",       required_argument, NULL, 'p'},
    {NULL,        no_argument,       NULL,  0 }
};

const char shot_options[] = "hvap:";

typedef struct {
    int cpu;
    uint32_t reg;
} options_t;

options_t options;

void usage()
{
    fprintf(stderr,
            "Usage:\n"
            "    %s [options] <register-id> <register-value> [...]\n"
            "Options:\n"
            "    --help         -h  print this help\n"
            "    --version      -v  print current version\n"
            "    --all          -a  all processors\n"
            "    --processor #  -p  select processor number (default 0)\n",
            program
        );
}

inline void version()
{
    fprintf(stderr, "%s: version %s\n", program, MSR_VERSION_STRING);
}

int is_cpu(const struct dirent *dirp)
{
    return std::isdigit(dirp->d_name[0]); /* /dev/cpu/<cpu> */
}

bool is_onln(int cpu)
{
    if (cpu < 0) {
        return false;
    }

    // the boot cpu do *not* support hotplug (x86 platform, linux-2.6.x, linux-4.7.x)
    if (cpu == 0) {
        return true;
    }

    // if online, the cache directory exist, otherwise not (x86 platform, linux-2.6.x, linux-4.7.x)
    std::string cpu_path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache"; 

    if (::access(cpu_path.c_str(), F_OK) != 0) {
        return false;
    }

    return true;
}

/**
 * msr_print - print value in the given format
 *
 * @val  value to print
 *
 * FIXME:
 *
 */
void msr_print(uint64_t val)
{
    FILE *fp = stdout;

    fprintf(fp, "%zx\n", val);
}

/** 
 * msr_read - read values from @cpu's msr register specified by @reg
 *
 * @cpu   which processor
 * @reg   the register id
 *
 * Return:
 *     true  - read succ
 *     false - read fail
 */
bool msr_read(int cpu, uint32_t reg)
{
    if (!is_onln(cpu)) {
        msr_warn("cpu %2d does not online\n", cpu);
        return true;
    }

    std::string msr_path = "/dev/cpu/" + std::to_string(cpu) + "/msr";

    int fd = ::open(msr_path.c_str(), O_RDONLY);
    if (fd == -1) {
        switch(errno) {

        // the file is a device special file and no corresponding device exists
        case ENXIO:
            msr_warn("cpu %2d does not exist\n", cpu);
            return false;

        //
        case EIO:
            msr_warn("cpu %2d does not support MSR\n", cpu);
            return false;

        default:
            msr_warn("failed on open MSR file for cpu %d, %s\n", cpu, strerror_r(errno, NULL, 0));
            return false;
        }
    }
    
    uint64_t val = 0;

    if (::pread(fd, &val, sizeof(val), reg) != sizeof(val)) {
        msr_warn("failed to read MSR file for cpu %d, %s\n", cpu, strerror_r(errno, NULL, 0));
        ::close(fd);
        return false;
    }

    ::close(fd);

    msr_print(val);

    return true;
}


/** 
 * msr_read - read values from all (online) cpus' msr register specified by @reg
 *
 * @reg   the register id
 *
 * Return:
 *     true  - read succ
 *     false - read fail
 */
bool msr_read(uint32_t reg)
{
    struct dirent **namelist;
    size_t nr_dirent = 0;

    nr_dirent = ::scandir("/dev/cpu", &namelist, is_cpu, 0);

    for (size_t cpu = 0; cpu < nr_dirent; ++cpu) {
        msr_read(std::stoi(namelist[cpu]->d_name), reg);
    }

    for (size_t cpu = 0; cpu < nr_dirent; ++cpu) {
        free(namelist[cpu]);
    }

    free(namelist);

    return true;
}

} /* namespace msr */


int main(int argc, char **argv)
{
    char ch;
    while ((ch = getopt_long(argc, argv, msr::shot_options, msr::long_options, NULL)) != -1) {
        switch (ch) {
        case 'h':
            msr::usage();
            exit(EXIT_SUCCESS);

        case 'v':
            msr::version();
            exit(EXIT_SUCCESS);

        case 'a':
            msr::options.cpu = -1;  
            break;

        case 'p':
            msr::options.cpu = std::stoi(optarg);
            break;
            
        default:
            msr::usage();
            exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 > argc) {
        msr::usage();
        exit(EXIT_FAILURE);
    }

    if (geteuid() != 0) {
        msr_warn("you need root privilege to run\n");
        exit(EXIT_FAILURE);
    }

    msr::options.reg = std::stoul(argv[optind++], 0, 16);

    if (msr::options.cpu == -1) {
        msr::msr_read(msr::options.reg);
    } else {
        msr::msr_read(msr::options.cpu, msr::options.reg);
    }
}
