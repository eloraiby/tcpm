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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "internals.h"


////////////////////////////////////////////////////////////////////////////////
//
//         spinlock
//
////////////////////////////////////////////////////////////////////////////////

static inline
void
spinLock(atomic_bool* lock) {
    bool expected   = false;
    while( !atomic_compare_exchange_weak(lock, &expected, true) ) {
        expected    = false;
    }
}

static inline
void
unlock(atomic_bool* lock) {
    bool expected   = true;
    bool current    = atomic_load(lock);
    if( expected != current ) {
        fprintf(stderr, "current: %u - expected: %u\n", current, expected);
        assert(expected == current);
    }
    if( !atomic_compare_exchange_strong(lock, &expected, false) ) {
        fprintf(stderr, "atomic_compare_exchange_strong failed in unlock! will die!\n");
        exit(1);
    }
}

static inline
bool
tryLock(atomic_bool* lock) {
    bool expected   = false;
    return atomic_compare_exchange_strong(lock, &expected, true);
}

////////////////////////////////////////////////////////////////////////////////
//
//         Lock-free Bounded Queue (heavily inspired from 1024cores.net)
//
////////////////////////////////////////////////////////////////////////////////


BoundedQueue*
BoundedQueue_init(BoundedQueue* bq, uint32_t cap, ElementRelease elementRelease) {
    memset(bq, 0, sizeof(BoundedQueue));
    bq->elementRelease  = elementRelease;
    bq->cap             = cap < 2 ? 2 : cap;
    bq->elements        = (Element*)calloc(cap, sizeof(Element));
    for( uint32_t i = 0; i < cap; ++i ) {
        atomic_store_explicit(&bq->elements[i].seq, i, memory_order_release);
    }
    atomic_store_explicit(&bq->first, 0, memory_order_release);
    atomic_store_explicit(&bq->last, 0, memory_order_release);
    return bq;
}

void
BoundedQueue_release(BoundedQueue* bq) {
    void* element = NULL;
    if( bq->elementRelease ) {
        while( (element = BoundedQueue_pop(bq)) ) {
            bq->elementRelease(element);
        }
    }
    free(bq->elements);
    bq->elements    = NULL;
}

bool
BoundedQueue_push(BoundedQueue* bq, void* data) {
    Element*    el  = NULL;
    uint32_t    last    = atomic_load_explicit(&bq->last, memory_order_acquire);

    while(true) {
        el  = &bq->elements[last % bq->cap];
        uint32_t seq  = atomic_load_explicit(&el->seq, memory_order_acquire);
        int32_t diff  = (int32_t)(seq) - (int32_t)(last);
        if( diff == 0 && atomic_compare_exchange_weak(&bq->last, &last, last + 1) ) {
            break;
        } else if( diff < 0 ) { return false; }
        last    = atomic_load_explicit(&bq->last, memory_order_acquire);
    }

    // Past this point, any preemption will cause all other consumers
    // to spin-lock waiting for it to finish, IF AND ONLY IF they
    // reach the end. Normal case: Producers are ahead
    atomic_store_explicit((atomic_size_t*)&el->data, (size_t)data, memory_order_release);
    atomic_store_explicit(&el->seq, last + 1, memory_order_release);
    return true;
}

void*
BoundedQueue_pop(BoundedQueue* bq) {
    Element*    el      = NULL;
    void*       data    = NULL;
    uint32_t    first   = atomic_load_explicit(&bq->first, memory_order_acquire);

    while( true ) {
        el  = &bq->elements[first % bq->cap];
        uint32_t seq  = atomic_load_explicit(&el->seq, memory_order_acquire);
        int32_t diff  = (int32_t)(seq) - (int32_t)((first + 1));
        if( diff == 0 && atomic_compare_exchange_weak(&bq->first, &first, first + 1) ) {
            break;
        } else if( diff < 0 ) {
            return NULL;
        }

        first  = atomic_load_explicit(&bq->first, memory_order_acquire);
    }

    data    = (void*)atomic_load_explicit((atomic_size_t*)&el->data, memory_order_acquire);
    atomic_store_explicit(&el->seq, first + bq->cap, memory_order_release);
    return data;
}


////////////////////////////////////////////////////////////////////////////////
//
//                      Process Dispatcher Queue
//
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    uint32_t         threadId;
    ProcessQueue*    queue;
} WorkerState;

static
void
processRelease(Process* proc) {
    spinLock(&proc->releaseLock);
    atomic_fetch_add(&proc->gen, 1);

    if( proc->releaseState ) {
        proc->releaseState(proc->state);
    }

    BoundedQueue_release(&proc->messageQueue);

    // BugFix: always unlock before pusing back to processQueue
    unlock(&proc->releaseLock);

    // push back to the pool
    while( BoundedQueue_push(&proc->processQueue->procPool, proc) == false );
}

static
bool
handleProcess(ProcessQueue* dq, Process* proc, void* msg) {
    pthread_setspecific(dq->currentProcess, proc); // set the current running actor
    assert( proc == pthread_getspecific(dq->currentProcess) );
    switch( proc->handler(dq, proc->state, msg) ) {
    case PCT_STOP:
        processRelease(proc);
        return false;
    case PCT_WAIT_MESSAGE:
        proc->runningState  = PS_WAITING;
        return true;
    case PCT_CONTINUE:
        proc->runningState  = PS_RUNNING;
        return true;
    }
}

static
void*
threadWorker(void* workerState_) {
    WorkerState*     workerState = (WorkerState*)workerState_;
    ProcessQueue*    dq          = workerState->queue;

    while( atomic_load_explicit((atomic_int*)&dq->state, memory_order_acquire) == DQS_RUNNING ) {
        Process*    proc = (Process*)BoundedQueue_pop(&dq->runQueue);
        if( proc == NULL ) {
            pthread_yield();
        } else {
            bool        pushActorBack   = true;
            uint32_t    msgCount        = 0;
            while( msgCount < proc->maxMessagePerCycle && pushActorBack ) {
                if( proc->runningState == PS_RUNNING ) {
                    pushActorBack       = handleProcess(dq, proc, NULL);
                } else {
                    assert( proc->runningState == PS_WAITING );
                    void*   msg         = BoundedQueue_pop(&proc->messageQueue);
                    if( msg ) {
                        pushActorBack   = handleProcess(dq, proc, msg);
                    } else {
                        break;
                    }
                }
                ++msgCount;
            }
            if( pushActorBack ) {
                while( BoundedQueue_push(&dq->runQueue, proc) == false ) {
                    pthread_yield();
                }
            } else {    // actor died
                atomic_fetch_sub(&dq->procCount, 1);
            }
        }
    }

    free(workerState);
    return NULL;
}


ProcessQueue*
ProcessQueue_init(uint32_t procCap, uint32_t threadCount) {
    ProcessQueue*    dq  = (ProcessQueue*)calloc(1, sizeof(*dq));
    dq->processCap  = procCap;
    dq->threadCount = threadCount;
    dq->threads     = (pthread_t*)calloc(threadCount, sizeof(pthread_t));
    BoundedQueue_init(&dq->runQueue, procCap, (ElementRelease)processRelease);
    pthread_key_create(&dq->currentProcess, NULL);
    dq->processes   = (Process*)calloc(procCap, sizeof(Process));
    dq->state       = DQS_RUNNING;
    BoundedQueue_init(&dq->procPool, procCap, NULL);

    for( uint32_t p = 0; p < procCap; ++p ) {
        dq->processes[p].id = p;
        atomic_store_explicit(&dq->processes[p].gen, 0, memory_order_release);
        BoundedQueue_push(&dq->procPool, &dq->processes[p]);
    }

    atomic_store(&dq->procCount, 0);
    for( uint32_t threadId = 0; threadId < threadCount; ++threadId ) {

        WorkerState*    ws  = (WorkerState*)calloc(1, sizeof(WorkerState));
        ws->threadId    = threadId;
        ws->queue       = dq;
        if( pthread_create(&dq->threads[threadId], NULL, threadWorker, ws) != 0 ) {
            fprintf(stderr, "Fatal Error: unable to create thread!\n");
            exit(1);
        }
    }

    return dq;
}

void
ProcessQueue_release(ProcessQueue* dq) {
    if( atomic_load_explicit((atomic_int*)&dq->state, memory_order_acquire) == DQS_RUNNING ) {
        atomic_store_explicit((atomic_int*)&dq->state, DQS_STOPPED, memory_order_release);
        // wait on the threads to exit
        for( uint32_t threadId = 0; threadId < dq->threadCount; ++threadId ) {
            pthread_join(dq->threads[threadId], NULL);
        }

        // now free the actors/messages
        BoundedQueue_release(&dq->runQueue);
    }
    BoundedQueue_release(&dq->procPool);
    free(dq->threads);
    free(dq->processes);
    free(dq);
}

SendResult
Process_sendMessage(PID dest, void* message, MessageAction ma) {
    ProcessQueue*   destPQ      = dest.pq;
    Process*        destProc    = &destPQ->processes[dest.id];

    // We have to handle nasty situations here:
    //
    // 1. we are trying to write while the process is dying:
    //    X = genId
    //    actor dies
    //    push message
    //    actor revived
    //    new actor consumes wrong message
    //    send returns SUCCESS
    //
    // 2. we are trying to write while the process is dying:
    //    X = genId
    //    actor dies
    //    push message
    //    send returns SUCCESS, but message never processed (lesser evil)
    //
    // we need a release lock (until another better method is found)
    if( tryLock(&destProc->releaseLock) ) {
        if( dest.gen != destProc->gen ) {
            unlock(&destProc->releaseLock);
            return ACTOR_IS_DEAD;
        }

        if( BoundedQueue_push(&destProc->messageQueue, message) ) {
            unlock(&destProc->releaseLock);
            return SEND_SUCCESS;
        } else {
            switch(ma) {
            case MA_KEEP: break;
            case MA_REMOVE:
                destProc->messageQueue.elementRelease(message);
            }
            unlock(&destProc->releaseLock);
            return SEND_FAIL;
        }
    } else {
        //fprintf(stderr, ".");
        return SEND_FAIL;
    }
}

void*
Process_receiveMessage(ProcessQueue* dq) {
    Process*    proc    = (Process*)pthread_getspecific(dq->currentProcess);
    return BoundedQueue_pop(&proc->messageQueue);
}

PID
Process_self(ProcessQueue* dq) {
    Process* proc   = (Process*)pthread_getspecific(dq->currentProcess);
    return (PID){ .pq = dq, .id = proc->id, .gen = proc->gen };
}

PID
Process_parent(PID pid) {
    Process* proc   = &pid.pq->processes[pid.id];
    return (PID){ .pq = pid.pq, .id = proc->parent->id, .gen = proc->parent->gen };
}


PID
ProcessQueue_spawn(ProcessQueue* dq, ProcessSpawnParameters* parameters) {
    uint32_t    procCount   = atomic_fetch_add(&dq->procCount, 1);
    if( procCount < dq->processCap ) {
        Process*    proc    = NULL;

        // TODO: contention point
        while( (proc = (Process*)BoundedQueue_pop(&dq->procPool)) == NULL ) {
            pthread_yield();
        }

        Process*    parent  = (Process*)pthread_getspecific(dq->currentProcess);
        atomic_store(&proc->releaseLock, false);
        proc->parent        = parent;
        proc->processQueue  = dq;
        proc->handler       = parameters->handler;
        proc->releaseState  = parameters->releaseState;
        proc->state         = parameters->initialState;
        proc->runningState  = PS_RUNNING;
        proc->maxMessagePerCycle   = (parameters->messageCap > parameters->maxMessagePerCycle) ? parameters->maxMessagePerCycle :  parameters->messageCap;
        BoundedQueue_init(&proc->messageQueue, parameters->messageCap, parameters->messageRelease);

        // TODO: contention point
        while( BoundedQueue_push(&dq->runQueue, proc) == false ) {
            // other threads are hanging before writing the el->seq, yield
            pthread_yield();
        }

        return (PID){ .pq = dq, .id = proc->id, .gen = proc->gen };

    } else {
        atomic_fetch_sub(&dq->procCount, 1);
        if( parameters->releaseState ) {
            parameters->releaseState(parameters->initialState);
        }

        return (PID){ .pq = NULL, .id = 0, .gen = 0 };
    }
}
