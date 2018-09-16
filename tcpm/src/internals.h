#ifndef INTERNALS__H
#define INTERNALS__H

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

#include <stdbool.h>
#include <stdatomic.h>
#include <tcpm.h>

#define _GNU_SOURCE
#include <pthread.h>

typedef _Atomic uint32_t atomic_uint32_t;
typedef _Atomic uint64_t atomic_uint64_t;

////////////////////////////////////////////////////////////////////////////////
// Lock-free bounded queue
//
// This lock-free data structure is lock-free in the sense, it garanties
// that at least either of the producers or the consumers can continue their
// work while the other one is pushing/poping
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    atomic_uint32_t     seq;
    void*               data;
} Element;

typedef void            (*ElementRelease)   (void* element);

typedef struct {
    atomic_uint32_t     first;
    atomic_uint32_t     last;
    uint32_t            cap;
    Element*            elements;
    ElementRelease      elementRelease;
} BoundedQueue;

BoundedQueue*   BoundedQueue_init   (BoundedQueue* bq, uint32_t cap, ElementRelease elementRelease);
void            BoundedQueue_release(BoundedQueue* bq);
bool            BoundedQueue_push   (BoundedQueue* bq, void* data);
void*           BoundedQueue_pop    (BoundedQueue* bq); // up to the receiver to free the message

////////////////////////////////////////////////////////////////////////////////
// Process Management
////////////////////////////////////////////////////////////////////////////////

typedef struct Process              Process;

typedef enum {
    PS_RUNNING,
    PS_WAITING,         // waiting on a message
} ProcessRunningState;

struct Process {
    atomic_bool         releaseLock;
    uint64_t            id;                 // index
    atomic_uint64_t     gen;                // generation
    void*               state;
    uint32_t            maxMessagePerCycle;
    BoundedQueue        messageQueue;
    ProcessRunningState runningState;
    ProcessHandler      handler;
    ProcessReleaseState releaseState;
    ProcessQueue*       processQueue;
    Process*            parent;
};

typedef enum {
    DQS_RUNNING,
    DQS_STOPPED,
} ProcessQueueState;

struct ProcessQueue {
    BoundedQueue        runQueue;   // running process queue
    BoundedQueue        procPool;   // process pool
    uint32_t            threadCount;
    pthread_t*          threads;
    uint32_t            processCap;
    ProcessQueueState   state;
    atomic_uint32_t     procCount;
    pthread_key_t       currentProcess;   // (TLS) per thread, current running process
    Process*            processes;  // Process array
};

#endif
