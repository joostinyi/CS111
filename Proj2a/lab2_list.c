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
#include <signal.h>
#include "SortedList.h"

int opt, numThreads = 1, numIterations = 1, opt_yield = 0, syncOpt = 0, numOps, length;
unsigned long diff;
char lockOpt = 0;
struct timespec begin, end;
char testName[15];
char* yieldString;
pthread_t *threads;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
long lock = 0;
SortedList_t *list;
SortedListElement_t *elements, *lookup;

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"threads=", 1, NULL, 't'},
        {"iterations=", 1, NULL, 'i'},
        {"yield=", 1, NULL, 'y'},
        {"sync=", 1, NULL, 's'},
        {0, 0, 0, 0}};

void processOpts(int argc, char *argv[]);
static inline unsigned long get_nanosec_from_timespec(struct timespec *spec);
void *thread_worker(void *arg);
void sighandler(int signum);
void init();
void insert_wrapped(SortedListElement_t *elem);
void length_wrapped();
void lookupAndDelete_wrapped(SortedListElement_t *elem);

int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    init();
    numOps = 3 * numIterations * numThreads;
    int i;
    threads = malloc(numThreads * sizeof(pthread_t));
    int position[numThreads];
    for (i = 0; i < numThreads; i++)
        position[i] = numIterations * i;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    for (i = 0; i < numThreads; i++)
        if (pthread_create(&threads[i], NULL, thread_worker, (void *)&position[i]) != 0)
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
    if (SortedList_length(list) != 0)
    {
        fprintf(stderr, "nonzero list length\n");
        exit(2);
    }
    if (fprintf(stdout, "%s,%d,%d,%d,%d,%lu,%lu\n", testName, numThreads, numIterations, 1,
                numOps, diff, diff / numOps) < 0)
    {
        fprintf(stderr, "error writing to csv file\n");
        exit(2);
    }
    if (lockOpt == 'm')
        pthread_mutex_destroy(&mutex);
    free(elements);
    free(threads);
    free(list);
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
            if (strlen(optarg) > 3)
            {
                fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield=[idl]] [--sync=opt]\n", argv[0]);
                exit(1);
            }
            yieldString = malloc(sizeof(char) * 6);
            yieldString = strdup(optarg);
            for (char *yield = optarg; *yield != '\0'; yield++)
            {
                switch (*yield)
                {
                case 'i':
                    opt_yield |= INSERT_YIELD;
                    break;
                case 'd':
                    opt_yield |= DELETE_YIELD;
                    break;
                case 'l':
                    opt_yield |= LOOKUP_YIELD;
                    break;
                default:
                    fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield=[idl]] [--sync=opt]\n", argv[0]);
                    exit(1);
                }
            }
            break;
        case 's':
            syncOpt = 1;
            switch (*optarg)
            {
            case 'm':
                lockOpt = 'm';
                break;
            case 's':
                lockOpt = 's';
                break;
            default:
                fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield=[idl]] [--sync=opt]\n", argv[0]);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "usage: %s [--threads=#] [--iterations=#] [--yield=[idl]] [--sync=opt]\n", argv[0]);
            exit(1);
        }
    }
    char lockString[3];
    lockString[0] = '-';
    lockString[1] = lockOpt;
    lockString[2] = '\0';

    char *tempString = "list-";
    strcpy(testName, tempString);
    if (!opt_yield)
        strcat(testName, "none");
    else
        strcat(testName, yieldString);
    if (syncOpt)
        strcat(testName, lockString);
    else
        strcat(testName, "-none");
    free(yieldString);
}

static inline unsigned long get_nanosec_from_timespec(struct timespec *spec)
{
    unsigned long ret = spec->tv_sec;
    ret = ret * 1000000000 + spec->tv_nsec;
    return ret;
}

void insert_wrapped(SortedListElement_t *elem)
{
    switch (lockOpt)
    {
    case 'm':
        pthread_mutex_lock(&mutex);
        SortedList_insert(list, elem);
        pthread_mutex_unlock(&mutex);
        break;
    case 's':
        while (__sync_lock_test_and_set(&lock, 1))
            ;
        SortedList_insert(list, elem);
        __sync_lock_release(&lock);
        break;
    default:
        SortedList_insert(list, elem);
    }
}

void length_wrapped()
{
    switch (lockOpt)
    {
    case 'm':
        pthread_mutex_lock(&mutex);
        length = SortedList_length(list);
        pthread_mutex_unlock(&mutex);
        break;
    case 's':
        while (__sync_lock_test_and_set(&lock, 1))
            ;
        length = SortedList_length(list);
        __sync_lock_release(&lock);
        break;
    default:
        length = SortedList_length(list);
    }
    if (length < 0)
    {
        fprintf(stderr, "Negative list length\n");
        exit(2);
    }
}

void lookupAndDelete_wrapped(SortedListElement_t *elem)
{
    switch (lockOpt)
    {
    case 'm':
        pthread_mutex_lock(&mutex);
        if (!(lookup = SortedList_lookup(list, elem->key)))
        {
            fprintf(stderr, "Lookup Failed\n");
            exit(2);
        }
        if (SortedList_delete(lookup))
        {
            fprintf(stderr, "Delete Failed\n");
            exit(2);
        }
        pthread_mutex_unlock(&mutex);
        break;
    case 's':
        while (__sync_lock_test_and_set(&lock, 1))
            ;
        if (!(lookup = SortedList_lookup(list, elem->key)))
        {
            fprintf(stderr, "Lookup Failed\n");
            exit(2);
        }
        if (SortedList_delete(lookup))
        {
            fprintf(stderr, "Delete Failed\n");
            exit(2);
        }
        __sync_lock_release(&lock);
        break;
    default:
        if (!(lookup = SortedList_lookup(list, elem->key)))
        {
            fprintf(stderr, "Lookup Failed\n");
            exit(2);
        }
        if (SortedList_delete(lookup))
        {
            fprintf(stderr, "Delete Failed\n");
            exit(2);
        }
    }
}

void *thread_worker(void *arg)
{
    int i;
    int pos = *((int *)arg);
    for (i = pos; i < pos + numIterations; i++)
        insert_wrapped(&elements[i]);
    length_wrapped();
    for (i = pos; i < pos + numIterations; i++)
    {
        lookupAndDelete_wrapped(&elements[i]);
    }
    return NULL;
}

void sighandler(int signum)
{
    if (signum == SIGSEGV)
    {
        fprintf(stderr, "Segmentation Fault: %s\n", strerror(errno));
        exit(2);
    }
}

void init()
{
    list = malloc(sizeof(SortedList_t));
    list->key = NULL;
    list->next = list;
    list->prev = list;

    elements = malloc(sizeof(SortedListElement_t) * numThreads * numIterations);
    for (int i = 0; i < numThreads * numIterations; i++)
    {

        char str[10];

        for (int j = 0; j < 10; j++)
            str[j] = 97 + rand() % 25; // lowercase letters

        elements[i].key = str;
    }
}