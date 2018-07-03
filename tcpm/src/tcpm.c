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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdatomic.h>

#include "internals.h"

////////////////////////////////////////////////////////////////////////////////
//
//                      Lock-free Bounded Queue
//
////////////////////////////////////////////////////////////////////////////////


BoundedQueue*
BoundedQueue_init(BoundedQueue* bq, uint32_t cap, ElementRelease elementRelease) {
    memset(bq, 0, sizeof(BoundedQueue));
    bq->elementRelease  = elementRelease;
    bq->cap             = cap;
    bq->elements        = calloc(cap, sizeof(Element));
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
    atomic_store_explicit(&el->data, data, memory_order_release);
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

    data    = atomic_load_explicit(&el->data, memory_order_acquire);
    atomic_store_explicit(&el->seq, first + bq->cap, memory_order_release);
    return data;
}

