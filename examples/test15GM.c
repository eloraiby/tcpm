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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "../tcpm/src/internals.h"


void*
produce(void* _bq) {
    BoundedQueue*   bq = (BoundedQueue*)_bq;
    fprintf(stderr, "producer started\n");
    for( size_t i = 0; i < 15000000000; ++i ) {
        while(BoundedQueue_push(bq, (void*)(i + 1)) != true);
        if( i % 1000000 == 0) {
            fprintf(stderr, "%lu\n", i);
        }
    }
    return NULL;
}

void consume(void* _bq) {
    BoundedQueue*   bq = (BoundedQueue*)_bq;
    for( size_t i = 0; i < 15000000000; ++i ) {
        size_t res = 0;
        do {
            res = (size_t)BoundedQueue_pop(bq);
            if( res != i + 1 && res != 0 ) {
                fprintf(stderr, "MEGA ERROR\n");
                exit(1);
            }
        } while(res == 0);
    }
}


int
main(int argc, char* argv[]) {
    pthread_t prod;

    BoundedQueue bq = { 0 };
    BoundedQueue_init(&bq, 2, NULL);
    if( pthread_create(&prod, NULL, produce, &bq) != 0 ) {
        fprintf(stderr, "Fatal Error: unable to create thread!\n");
        exit(1);
    }

    consume(&bq);
    pthread_join(prod, NULL);
    fprintf(stderr, "DONE\n");
    BoundedQueue_release(&bq);
    return 0;
}
