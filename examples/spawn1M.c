/*
    Tiny Cooperative Process Management library
    Copyright (C) 2018  Wael El Oraiby

    All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

#include <tcpm.h>

#define MAX_ACTOR_COUNT 1000000

ProcessContinuation
actorHandler(ProcessQueue* dq, void* state_, void* msg) {
    uint32_t* state   = (uint32_t*)state_;
    if( msg == NULL ) {
        // send a message to self (wakeup)
        PID self    = Process_self(dq);
        if( Process_sendMessage(self, (void*)-1, MA_KEEP) == SEND_SUCCESS ) {
            return PCT_WAIT_MESSAGE;
        }

        return PCT_CONTINUE;
    }
/*
    if( *state % 1000 == 0 ) {
        fprintf(stderr, "-> %u <-\n", *state);
    }
*/
    atomic_fetch_add(state, 1);
    return PCT_STOP;
}

struct timespec
timespec_diff(struct timespec end, struct timespec start) {
    struct timespec diff;
    if( end.tv_nsec < start.tv_nsec ) {
        /* If nanoseconds in t1 are larger than nanoseconds in t2, it
           means that something like the following happened:
           t1.tv_sec = 1000    t1.tv_nsec = 100000
           t2.tv_sec = 1001    t2.tv_nsec = 10
           In this case, less than a second has passed but subtracting
           the tv_sec parts will indicate that 1 second has passed. To
           fix this problem, we subtract 1 second from the elapsed
           tv_sec and add one second to the elapsed tv_nsec. See
           below:
        */
        diff.tv_sec  = end.tv_sec  - start.tv_sec  - 1;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = end.tv_sec  - start.tv_sec;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return diff;
}

int
main() {
    while(1) {
    fprintf(stderr, "spawning 1,000,000 actors\n");
    ProcessQueue*  dq  = ProcessQueue_init(1024, 8);

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    uint32_t        sum = 0;

    for( uint32_t a = 0; a < MAX_ACTOR_COUNT; ++a ) {
        PID    ac = { 0 };
        while( ac.pq == NULL ) {
            ProcessSpawnParameters  sp;
            sp.handler          = actorHandler;
            sp.messageCap       = 2;
            sp.maxMessagePerCycle   = 1;
            sp.initialState     = (void*)&sum;//state;
            sp.messageRelease   = NULL;
            sp.releaseState     = NULL;
            ac  = ProcessQueue_spawn(dq, &sp);
        }

        if( (a + 1) % 1000 == 0 ) {
            //fprintf(stderr, "spawned %u actors\n", a + 1);
            //usleep(10000);
        }

    }

    while((atomic_load(&sum)) < MAX_ACTOR_COUNT) {
        usleep(1000);
        fprintf(stderr, "-->> %u <<--\n", sum);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    struct timespec diff    = timespec_diff(end, start);

    ProcessQueue_release(dq);
    fprintf(stderr, "sum: %u - %u actors executed and finished in %.15f seconds\n", sum, MAX_ACTOR_COUNT, diff.tv_sec + (double) diff.tv_nsec / 1000000000L);
    }
    return 0;
}
