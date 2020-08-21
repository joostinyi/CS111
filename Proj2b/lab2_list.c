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

int opt, numThreads = 1, numIterations = 1, numLists = 1, opt_yield = 0, numOps;
unsigned long diff, lockTime = 0;
char lockOpt = 0;
struct timespec begin, end;
char testName[15];
char *yieldString;
pthread_t *threads;
unsigned long *wait_time;
pthread_mutex_t *locks;
int *spinlocks, *listNum;
SortedList_t *listheads;
SortedListElement_t *elements;
void sighandler(int signum);

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"threads=", 1, NULL, 't'},
        {"iterations=", 1, NULL, 'i'},
        {"yield=", 1, NULL, 'y'},
        {"sync=", 1, NULL, 's'},
        {"lists=", 1, NULL, 'l'},
        {0, 0, 0, 0}};

void processOpts(int argc, char *argv[]);
static inline unsigned long get_nanosec_from_timespec(struct timespec *spec);
void *thread_worker(void *arg);
void sighandler(int signum);
void init();
void insert_wrapped(SortedListElement_t *elem, int thread_num);
int length_wrapped(int thread_num);
SortedListElement_t *lookup_wrapped(SortedListElement_t *elem, int thread_num);
void delete_wrapped(SortedListElement_t *elem, int thread_num);
int hash(const char *key);

int main(int argc, char *argv[])
{
    signal(SIGSEGV, sighandler);
    processOpts(argc, argv);
    init();
    numOps = 3 * numIterations * numThreads;
    int i;
    threads = malloc(numThreads * sizeof(pthread_t));
    int t_id[numThreads];
    wait_time = malloc(numThreads * sizeof(unsigned long));

    clock_gettime(CLOCK_MONOTONIC, &begin);
    for (i = 0; i < numThreads; i++)
    {
        t_id[i] = i;
        if (pthread_create(&threads[i], NULL, thread_worker, &t_id[i]) != 0)
        {
            fprintf(stderr, "error creating threads\n");
            exit(2);
        }
    }
    for (i = 0; i < numThreads; i++)
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "error joining threads\n");
            exit(2);
        }

    clock_gettime(CLOCK_MONOTONIC, &end);
    diff = get_nanosec_from_timespec(&end) - get_nanosec_from_timespec(&begin);
    for (i = 0; i < numLists; i++)
    {
        if (SortedList_length(&listheads[i]) != 0)
        {
            fprintf(stderr, "Final nonzero list length\n");
            exit(2);
        }
    }
    if (lockOpt)
        for (i = 0; i < numThreads; i++)
            lockTime += wait_time[i];

    if (fprintf(stdout, "%s,%d,%d,%d,%d,%lu,%lu,%lu\n", testName, numThreads, numIterations, numLists,
                numOps, diff, diff / numOps, lockTime / (4 * numThreads * numIterations)) < 0)
    {
        fprintf(stderr, "error writing to csv file\n");
        exit(2);
    }

    switch (lockOpt)
    {
    case 'm':
        for (int i = 0; i < numLists; i++)
            pthread_mutex_destroy(&locks[i]);
        free(locks);
        break;
    case 's':
        free(spinlocks);
        break;
    }
    free(listheads);
    free(elements);
    free(threads);
    free(wait_time);
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
        case 'l':
            numLists = atoi(optarg);
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
    if (lockOpt)
        strcat(testName, lockString);
    else
        strcat(testName, "-none");
    if (opt_yield)
        free(yieldString);
}

static inline unsigned long get_nanosec_from_timespec(struct timespec *spec)
{
    unsigned long ret = spec->tv_sec;
    ret = ret * 1000000000 + spec->tv_nsec;
    return ret;
}

// void insert_wrapped(SortedListElement_t *elem, int thread_num)
// {
//     struct timespec start_time, end_time;
//     int listnum = hash(elem->key);
//     switch (lockOpt)
//     {
//     case 'm':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         pthread_mutex_lock(&locks[listnum]);
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         SortedList_insert(&listheads[listnum], elem);
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         pthread_mutex_unlock(&locks[listnum]);
//         break;
//     case 's':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         while (__sync_lock_test_and_set(&spinlocks[listnum], 1))
//             ;
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         SortedList_insert(&listheads[listnum], elem);
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         __sync_lock_release(&spinlocks[listnum]);
//         break;
//     default:
//         SortedList_insert(&listheads[listnum], elem);
//     }
// }

// int length_wrapped(int thread_num)
// {
//     struct timespec start_time, end_time;
//     int length = 0;
//     switch (lockOpt)
//     {
//     case 'm':
//         for (int i = 0; i < numLists; i++)
//         {
//             clock_gettime(CLOCK_MONOTONIC, &start_time);
//             pthread_mutex_lock(&locks[i]);
//             clock_gettime(CLOCK_MONOTONIC, &end_time);
//             int tempLength;
//             if ((tempLength = SortedList_length(&listheads[i])) < 0)
//             {
//                 fprintf(stderr, "Corrupted List\n");
//                 exit(2);
//             }
//             length += tempLength;
//             wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//             pthread_mutex_unlock(&locks[i]);
//         }
//         break;
//     case 's':
//         for (int i = 0; i < numLists; i++)
//         {
//             clock_gettime(CLOCK_MONOTONIC, &start_time);
//             while (__sync_lock_test_and_set(&spinlocks[i], 1))
//                 ;
//             clock_gettime(CLOCK_MONOTONIC, &end_time);
//             int tempLength;
//             if ((tempLength = SortedList_length(&listheads[i])) < 0)
//             {
//                 fprintf(stderr, "Corrupted List\n");
//                 exit(2);
//             }
//             length += tempLength;
//             wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//             __sync_lock_release(&spinlocks[i]);
//         }
//         break;
//     default:
//         for (int i = 0; i < numLists; i++)
//         {
//             int tempLength;
//             if ((tempLength = SortedList_length(&listheads[i])) < 0)
//             {
//                 fprintf(stderr, "Corrupted List\n");
//                 exit(2);
//             }
//             length += tempLength;
//         }
//     }
//     return length;
// }

// SortedListElement_t *lookup_wrapped(SortedListElement_t *elem, int thread_num)
// {
//     struct timespec start_time, end_time;
//     SortedListElement_t *lookup;
//     int listnum = hash(elem->key);
//     switch (lockOpt)
//     {
//     case 'm':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         pthread_mutex_lock(&locks[listnum]);
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         if (!(lookup = SortedList_lookup(&listheads[listnum], elem->key)))
//         {
//             fprintf(stderr, "Lookup Failed\n");
//             exit(2);
//         }
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         pthread_mutex_unlock(&locks[listnum]);
//         break;
//     case 's':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         while (__sync_lock_test_and_set(&spinlocks[listnum], 1))
//             ;
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         if (!(lookup = SortedList_lookup(&listheads[listnum], elem->key)))
//         {
//             fprintf(stderr, "Lookup Failed\n");
//             exit(2);
//         }
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         __sync_lock_release(&spinlocks[listnum]);
//         break;
//     default:
//         if (!(lookup = SortedList_lookup(&listheads[listnum], elem->key)))
//         {
//             fprintf(stderr, "Lookup Failed\n");
//             exit(2);
//         }
//     }
//     return lookup;
// }

// void delete_wrapped(SortedListElement_t *elem, int thread_num)
// {
//     struct timespec start_time, end_time;
//     int listnum = hash(elem->key);
//     switch (lockOpt)
//     {
//     case 'm':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         pthread_mutex_lock(&locks[listnum]);
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         if (SortedList_delete(elem))
//         {
//             fprintf(stderr, "Delete Failed\n");
//             exit(2);
//         }
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         pthread_mutex_unlock(&locks[listnum]);
//         break;
//     case 's':
//         clock_gettime(CLOCK_MONOTONIC, &start_time);
//         while (__sync_lock_test_and_set(&spinlocks[listnum], 1))
//             ;
//         clock_gettime(CLOCK_MONOTONIC, &end_time);
//         if (SortedList_delete(elem))
//         {
//             fprintf(stderr, "Delete Failed\n");
//             exit(2);
//         }
//         wait_time[thread_num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
//         __sync_lock_release(&spinlocks[listnum]);
//         break;
//     default:
//         if (SortedList_delete(elem))
//         {
//             fprintf(stderr, "Delete Failed\n");
//             exit(2);
//         }
//     }
// }

// void *thread_worker(void *id)
// {
//     int thread_num = *((int *)id);
//     fprintf(stderr,"Thread %d created\n",thread_num);
//     int startIndex = thread_num * numIterations;
//     for (int i = startIndex; i < startIndex + numIterations; i++)
//         insert_wrapped(&elements[i], thread_num);
//     fprintf(stderr,"Thread %d, Length %d\n",thread_num, length_wrapped(thread_num));
//     for (int i = startIndex; i < startIndex + numIterations; i++)
//     {
//         SortedListElement_t *lookup = lookup_wrapped(&elements[i], thread_num);
//         delete_wrapped(lookup, thread_num);
//     }
//     fprintf(stderr,"Thread %d exiting\n",thread_num);
//     return NULL;
// }

void init()
{
    listheads = malloc(sizeof(SortedList_t) * numLists);
    for (int i = 0; i < numLists; i++)
    {
        listheads[i].key = NULL;
        listheads[i].prev = &listheads[i];
        listheads[i].next = &listheads[i];
    }

    elements = malloc(sizeof(SortedListElement_t) * numThreads * numIterations);
    listNum = malloc(sizeof(int) * numThreads * numIterations);
    srand(time(NULL));
    for (int i = 0; i < numThreads * numIterations; i++)
    {
        char *str = malloc(2 * sizeof(char));
        str[0] = 'a' + rand() % 26;
        str[1] = '\0';
        elements[i].key = str;
        listNum[i] = hash(elements[i].key);
    }

    switch (lockOpt)
    {
    case 'm':
        locks = malloc(numLists * sizeof(pthread_mutex_t));
        for (int i = 0; i < numLists; i++)
            pthread_mutex_init(&locks[i], NULL);
        break;
    case 's':
        spinlocks = calloc(numLists, sizeof(int));
    }
}

int hash(const char *key)
{
    return (key[0] % numLists);
}

void sighandler(int signum)
{
    if (signum == SIGSEGV)
    {
        fprintf(stderr, "Segmentation Fault\n");
        exit(2);
    }
}

void *thread_worker(void *id)
{
    struct timespec start_time, end_time;
    int i;
    int num = *((int *)id);
    for (i = num; i < numThreads * numIterations; i += numThreads)
    {
        switch (lockOpt)
        {
        case 'm':
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            pthread_mutex_lock(&locks[listNum[i]]);
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            SortedList_insert(&listheads[listNum[i]], &elements[i]);
            pthread_mutex_unlock(&locks[listNum[i]]);
            break;
        case 's':
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            while (__sync_lock_test_and_set(&spinlocks[listNum[i]], 1))
                ;
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            SortedList_insert(&listheads[listNum[i]], &elements[i]);
            __sync_lock_release(&spinlocks[listNum[i]]);
            break;
        default:
            SortedList_insert(&listheads[listNum[i]], &elements[i]);
        }
    }
    int length = 0;
    int j;
    switch (lockOpt)
    {
    case 'm':
        for (j = 0; j < numLists; j++)
        {
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            pthread_mutex_lock(&locks[j]);
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            int subLength = SortedList_length(&listheads[j]);
            if (subLength < 0)
            {
                fprintf(stderr, "negative length\n");
                exit(2);
            }
            length += subLength;
            pthread_mutex_unlock(&locks[j]);
        }
        break;
    case 's':
        for (j = 0; j < numLists; j++)
        {
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            while (__sync_lock_test_and_set(&spinlocks[j], 1))
                ;
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            int subLength = SortedList_length(&listheads[j]);
            if (subLength < 0)
            {
                fprintf(stderr, "negative length\n");
                exit(2);
            }
            length += subLength;
            __sync_lock_release(&spinlocks[j]);
        }
        break;
    default:
        for (j = 0; j < numLists; j++)
        {
            int subLength = SortedList_length(&listheads[j]);
            if (subLength < 0)
            {
                fprintf(stderr, "negative length\n");
                exit(2);
            }
            length += subLength;
        }
    }
    SortedListElement_t *lookup;
    num = *((int *)id);
    for (i = num; i < numThreads * numIterations; i += numThreads)
    {
        int ret;
        switch (lockOpt)
        {
        case 'm':
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            pthread_mutex_lock(&locks[listNum[i]]);
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            lookup = SortedList_lookup(&listheads[listNum[i]], elements[i].key);
            if (lookup == NULL)
            {
                fprintf(stderr, "lookup error\n");
                exit(2);
            }
            ret = SortedList_delete(lookup);
            if (ret == 1)
            {
                fprintf(stderr, "delete error\n");
                exit(2);
            }
            pthread_mutex_unlock(&locks[listNum[i]]);
            break;
        case 's':
            if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            while (__sync_lock_test_and_set(&spinlocks[listNum[i]], 1))
                ;
            if (clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
            {
                fprintf(stderr, "clock error\n");
                exit(2);
            }
            wait_time[num] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
            lookup = SortedList_lookup(&listheads[listNum[i]], elements[i].key);
            if (lookup == NULL)
            {
                fprintf(stderr, "lookup error\n");
                exit(2);
            }
            ret = SortedList_delete(lookup);
            if (ret == 1)
            {
                fprintf(stderr, "delete error\n");
                exit(2);
            }
            __sync_lock_release(&spinlocks[listNum[i]]);
            break;
        default:
            lookup = SortedList_lookup(&listheads[listNum[i]], elements[i].key);
            if (lookup == NULL)
            {
                fprintf(stderr, "lookup error\n");
                exit(2);
            }
            ret = SortedList_delete(lookup);
            if (ret == 1)
            {
                fprintf(stderr, "delete error\n");
                exit(2);
            }
        }
    }
    return NULL;
}