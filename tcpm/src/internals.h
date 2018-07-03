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
#include <pthread.h>
#include <tcpm.h>

////////////////////////////////////////////////////////////////////////////////
// Lock-free bounded queue
//
// This lock-free data structure is lock-free in the sense, it garanties
// that at least either of the producers or the consumers can continue their
// work while the other one is pushing/poping
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    uint64_t    seq;
    void*       data;
} Element;

typedef void            (*ElementRelease)   (void* message);

typedef struct {
    uint32_t            first;
    uint32_t            last;
    uint32_t            cap;
    Element*            elements;
    ElementRelease      elementRelease;
} BoundedQueue;

BoundedQueue*   BoundedQueue_init   (BoundedQueue* bq, uint32_t cap, ElementRelease elementRelease);
void            BoundedQueue_release(BoundedQueue* bq);
bool            BoundedQueue_push   (BoundedQueue* bq, void* data);
void*           BoundedQueue_pop    (BoundedQueue* bq); // up to the receiver to free the message

#endif
