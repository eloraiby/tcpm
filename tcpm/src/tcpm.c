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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "internals.h"

////////////////////////////////////////////////////////////////////////////////
//
//         Lock-free Bounded Queue (heavily inspired from 1024cores.net)
//
////////////////////////////////////////////////////////////////////////////////


BoundedQueue*
BoundedQueue_init(BoundedQueue* bq, uint32_t cap, ElementRelease elementRelease) {
    memset(bq, 0, sizeof(BoundedQueue));
    bq->elementRelease  = elementRelease;
    bq->cap             = cap;
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
    while(true) {
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
    uint32_t            threadId;
    ProcessQueue*    queue;
} WorkerState;

static
void
processRelease(void* proc_) {
    Process*    proc    = (Process*)proc_;

    if( proc->releaseState ) {
        proc->releaseState(proc->state);
    }

    BoundedQueue_release(&proc->messageQueue);
    free(proc);
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
    WorkerState*        workerState = (WorkerState*)workerState_;
    ProcessQueue*    dq          = workerState->queue;

    while( atomic_load_explicit((atomic_int*)&dq->state, memory_order_acquire) == DQS_RUNNING ) {
        Process*    proc = (Process*)BoundedQueue_pop(&dq->processQueue);
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
                while( BoundedQueue_push(&dq->processQueue, proc) == false ) {
                    pthread_yield();
                }
            } else {
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
    BoundedQueue_init(&dq->processQueue, procCap, processRelease);
    pthread_key_create(&dq->currentProcess, NULL);
    dq->state       = DQS_RUNNING;
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
        BoundedQueue_release(&dq->processQueue);
    }
}

SendResult
Process_sendMessage(Process* dest, void* message) {
    if( BoundedQueue_push(&dest->messageQueue, message) ) {
        return SEND_SUCCESS;
    } else {
        return SEND_FAIL;
    }
}

void*
Process_receiveMessage(ProcessQueue* dq) {
    Process*    proc    = (Process*)pthread_getspecific(dq->currentProcess);
    return BoundedQueue_pop(&proc->messageQueue);
}

Process*
Process_self(ProcessQueue* dq) {
    return (Process*)pthread_getspecific(dq->currentProcess);
}

Process*
ProcessQueue_spawn(ProcessQueue* dq, ProcessSpawnParameters* parameters) {
    uint32_t    procCount   = atomic_fetch_add((atomic_uint32_t*)&dq->procCount, 1);
    if( procCount < dq->processCap ) {
        Process*    proc    = (Process*)calloc(1, sizeof(Process));
        Process*    parent  = (Process*)pthread_getspecific(dq->currentProcess);

        proc->parent        = parent;
        proc->processQueue  = dq;
        proc->handler       = parameters->handler;
        proc->releaseState  = parameters->releaseState;
        proc->state         = parameters->initialState;
        proc->runningState  = PS_RUNNING;
        proc->maxMessagePerCycle   = (parameters->messageCap > parameters->maxMessagePerCycle) ? parameters->maxMessagePerCycle :  parameters->messageCap;
        BoundedQueue_init(&proc->messageQueue, parameters->messageCap, parameters->messageRelease);

        while( BoundedQueue_push(&dq->processQueue, proc) == false ) {
            // other threads are hanging before writing the el->seq, yield
            //pthread_yield();
        }

        return proc;
    } else {
        atomic_fetch_sub(&dq->procCount, 1);
        if( parameters->releaseState ) {
            parameters->releaseState(parameters->initialState);
        }
        return NULL;
    }
}
