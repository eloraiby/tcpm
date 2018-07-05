#ifndef __INTERNALS__H__
#define __INTERNALS__H__

/*
    Tiny Cooperative Process Management library
    Copyright (C) 2018  Wael El Oraiby

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdatomic.h>
#include <tcpm.h>

#define __USE_GNU
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
    atomic_uint64_t     seq;
    void*               data;
} Element;

typedef void            (*ElementRelease)   (void* message);

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


typedef enum {
    PS_RUNNING,
    PS_WAITING,         // waiting on a message
} ProcessRunningState;

struct Process {
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
    BoundedQueue        processQueue;
    uint32_t            threadCount;
    pthread_t*          threads;
    uint32_t            processCap;
    ProcessQueueState    state;
    atomic_uint32_t     procCount;
    pthread_key_t       currentProcess;   // (TLS) per thread, current running process
};

#endif
