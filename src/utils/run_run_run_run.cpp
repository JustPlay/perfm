#include <cstdio>
#include <cstdlib>

#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{

    printf("------------------------------------------\n");
    printf("- PID: %d\n", getpid());
    printf("------------------------------------------\n");

    double scalar = 3.1415926;

    size_t nr_siz = 5000;

    double *a = new double[nr_siz];
    double *b = new double[nr_siz];
    double *c = new double[nr_siz];

    while (1) {
        for (size_t i = 0; i < nr_siz; ++i) {
            a[i] = b[i] + scalar * c[i];
        }
    }
}
