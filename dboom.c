/* dboom is an HTTP load generator written in C using libdill coroutines */
#include <libdill.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include "dboom.h"

int main(int argc, char **argv) {

    srand(time(NULL));

    char *requests = NULL;
    char *concurr = NULL;
    char *timeout = NULL;

    int c;
    while((c = getopt(argc, argv, "n:c:t:")) != -1) {
        switch(c) {
        case 'n':
            requests = optarg;
            break;
        case 'c':
            concurr = optarg;
            break;
        case 't':
            timeout = optarg;
            break;
        default:
            usage();
            break;  // Unreachable
        }
    }

    /* Exit if no url provided */
    if(optind == argc) usage();
    /* TODO: Accept > 1 URL */
    const char* url = argv[optind];
    /* Validate option args */
    int nreqs = getRequests(requests, DEFAULT_REQUESTS);
    int nconcurr = getConcurrentReqs(concurr, DEFAULT_CONCURR);
    int ntimeout = getTimeout(timeout, DEFAULT_TIMEOUT);

    printf("Running dboom\n\
        Url: %s\n\
        Total Requests: %d\n\
        Concurrent Requests: %d\n\
        Timeout: %d ms\n", url, nreqs, nconcurr, ntimeout);
    
    /* Each boom() coroutine writes to this channel when done */
    int done_ch = channel(sizeof(int), 0);
    /* Each boom() coroutine uses this channel to send stats. */
    /* TODO: stats channel should include more than response time. */
    int stats_ch = channel(sizeof(int), 0);
    /* Each stats() coroutine and main() uses this channel to control
       shutdown. */
    int stop_ch = channel(sizeof(int), 0);

    if(done_ch < 0 || stats_ch < 0 || stop_ch < 0) {
        perror("main() - channel() failed");
        exit(EXIT_FAILURE);
    }

    int rc = 0;
    /* Launch coroutine that captures stats */
    rc = go(stats(stats_ch, stop_ch));
    if(rc < 0) {
        perror("main() - go() failed");
        exit(EXIT_FAILURE);
    }

    /* Launch nconcurr coroutines, each one sending nreqs/nconcurr requests. */
    for(int i; i < nconcurr; ++i) {
        rc = go(boom(url, nreqs/nconcurr, ntimeout, done_ch, stats_ch));
        if(rc < 0) {
            perror("main() - go() failed");
            exit(EXIT_FAILURE);
        }
    }
    /* Wait for boom() coroutines to end */
    int done = 0;
    for(int i = nconcurr; i > 0; --i) {
        rc = chrecv(done_ch, &done, sizeof(done), -1);
        if(rc != 0) {
            perror("main() - chrecv() failed");
            exit(EXIT_FAILURE);
        }
    }
    /* Tell stats coroutine to end */
    int stop = 1;
    rc = chsend(stop_ch, &stop, sizeof(stop), -1);
    if(rc != 0) perror("main() - chsend() failed");
    /* Wait for stats to end */
    rc = chrecv(stop_ch, &stop, sizeof(stop), -1);
    if(rc != 0) perror("main() - chrecv() failed");

    exit(EXIT_SUCCESS);
}

coroutine void boom(const char* url, int nreqs, int timeout,
                    int done_ch, int stats_ch) {
    int rc = 0;
    int resptime = 0;
    /* Send requests until no more requests */
    for(int i = nreqs; i > 0; --i) {
        /* Do work... */
        msleep(now() + (1000 + (rand() % 3000)));
        resptime = rand() % 1000;
        rc = chsend(stats_ch, &resptime, sizeof(resptime), -1);
        if(rc != 0) perror("boom() - chsend() failed");
    }
    /* clean up and signal done */
    int done = 1;
    rc = chsend(done_ch, &done, sizeof(done), -1);
    if(rc != 0) perror("boom() - chsend() failed");
}

coroutine void stats(int stats_ch, int stop_ch)
{
    int rc = 0;
    int nrequests = 0;
    int stop = 0;
    int resptime = 0;
    int total = 0;

    struct chclause clauses[] = {
        {CHRECV, stop_ch, &stop, sizeof(stop)},
        {CHRECV, stats_ch, &resptime, sizeof(resptime)}
    };
    
    while(stop == 0) {
        rc = choose(clauses, 2, -1);
        if(rc < 0) {
            perror("stats() - choose() failed");
            break;
        }
        if(rc == 1) {
            /* Stats for a request available */
            nrequests++;
            printf("request: %d\tms: %d\n", nrequests, resptime);
            total += resptime;
        }
    }
    /* Display stats and signal done */
    printf("Avg response time for %d requests: %d\n", nrequests, total/nrequests);

    rc = chsend(stop_ch, &stop, sizeof(stop), -1);
    if(rc != 0) perror("stats() - chsend() failed");
}

void usage()
{
    fprintf(stderr, "Usage: dboom [-n nreqs] [-c nconcurr] [-t timeoutms] URL.\n");
    exit(EXIT_FAILURE);
}

int getRequests(const char *requests, int defaultval)
{
    int nrequests = requests ? atoi(requests) : defaultval;
    return nrequests;
}

int getConcurrentReqs(const char *concurr, int defaultval)
{
    return defaultval;
}

int getTimeout(const char *timeout, int defaultval)
{
    return defaultval;
}