// NAME: Justin Yi
// EMAIL: joostinyi00@gmail.com
// ID: 905123893

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int opt, numThreads = 1, numIterations = 1, i, yield = 0, syncOpt = 0, numOps;
long long sum = 0;
unsigned long diff;
char lockOpt = 0;
struct timespec begin, end;
char testName[15];
pthread_t *threads;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
long lock = 0;

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"threads=", 1, NULL, 't'},
        {"iterations=", 1, NULL, 'i'},
        {"yield", 0, NULL, 'y'},
        {"sync=", 1, NULL, 's'},
        {0, 0, 0, 0}};

void processOpts(int argc, char *argv[]);
static inline unsigned long get_nanosec_from_timespec(struct timespec * spec);
void add(long long *pointer, long long value);
void add_wrapped(long long *pointer, long long value);
void *thread_worker();

int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    numOps = 2 * numIterations * numThreads;
    threads = malloc(numThreads * sizeof(pthread_t));
    clock_gettime(CLOCK_MONOTONIC, &begin);
    for (i = 0; i < numThreads; i++) 
        if (pthread_create(&threads[i], NULL, thread_worker, NULL) != 0)
            {
                fprintf(stderr, "error creating threads\n");
                exit(2);
            }
    for (i = 0; i < numThreads; i++)
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "error joining threads\n");
            exit(2);
        }
    clock_gettime(CLOCK_MONOTONIC, &end);
    diff = get_nanosec_from_timespec(&end) - get_nanosec_from_timespec(&begin);
    if (fprintf(stdout, "%s,%d,%d,%d,%lu,%lu,%lld\n", testName, numThreads, numIterations, 
        numOps, diff, diff/numOps, sum) < 0)
        {
            fprintf(stderr, "error writing to csv file\n");
            exit(2);
        }
    if (lockOpt == 'm')
        pthread_mutex_destroy(&mutex);
    free(threads);
}

void processOpts(int argc, char *argv[])
{
    while ((opt = getopt_long(argc, argv, "", args, NULL)) != -1)
    {
        switch (opt)
        {
            case 't':
                numThreads = atoi(optarg);
                break;
            case 'i':
                numIterations = atoi(optarg);
                break;
            case 'y':
                yield = 1;
                break;
            case 's':
                switch(*optarg)
                {
                    case 'm':
                        lockOpt = 'm';
                        break;
                    case 's':
                        lockOpt = 's';
                        break;
                    case 'c':
                        lockOpt = 'c';
                        break;
                    default:
                        fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield] [--sync=s/m/c]\n", argv[0]);
                        exit(1);
                }
                syncOpt = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield] [--sync=s/m/c]\n", argv[0]);
                exit(1);
        }
    }
    char lockString[3];
    lockString[0] = '-';
    lockString[1] = lockOpt;
    lockString[2] = '\0';

    char* tempString = "add";
    strcpy(testName, tempString);
    if(yield)
        strcat(testName, "-yield");
    if(syncOpt)
        strcat(testName, lockString);
    else
        strcat(testName, "-none");
}

static inline unsigned long get_nanosec_from_timespec(struct timespec * spec)
{
	unsigned long ret= spec->tv_sec;
	 ret = ret * 1000000000 + spec->tv_nsec;
	return ret;
}

void add(long long *pointer, long long value)
{
    long long sum = *pointer + value;
    if (yield)
        sched_yield();
    *pointer = sum;
}

void add_wrapped(long long *pointer, long long value)
{
    switch (lockOpt)
    {
        case 'm':
            pthread_mutex_lock(&mutex);
            add(pointer, value);
            pthread_mutex_unlock(&mutex);
            break;
        case 's':
            while(__sync_lock_test_and_set (&lock, 1))
                ;
            add(pointer, value);
            __sync_lock_release (&lock);
            break;
        case 'c': ;
            long long prev, sum;
            do {
                prev = *pointer;
                sum = prev + value;
                if (yield)
                    sched_yield();
            } while(__sync_val_compare_and_swap(pointer, prev, sum) != prev);
            break;
        default:
            add(pointer, value);
    }
}

void *thread_worker()
{
    int i;
    for( i = 0; i < numIterations; i++)
        add_wrapped(&sum, 1);
    for( i = 0; i < numIterations; i++)
        add_wrapped(&sum, -1);
    return NULL;
}
