#ifndef TCPM__H
#define TCPM__H

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

#include <stdint.h>

typedef enum {
    PCT_STOP,
    PCT_CONTINUE,
    PCT_WAIT_MESSAGE,
} ProcessContinuation;

typedef struct ProcessQueue         ProcessQueue;
typedef ProcessContinuation         (*ProcessHandler)       (ProcessQueue*, void* localState, void* msg);
typedef void                        (*ProcessReleaseState)  (void* state);
typedef void                        (*MessageRelease)       (void* message);

typedef struct {
    ProcessQueue*       pq;
    uint64_t            id;
    uint64_t            gen;
} PID;

typedef enum {
    ACTOR_IS_DEAD   = -1,
    SEND_FAIL       = 0,
    SEND_SUCCESS    = 1,
} SendResult;

// on send failure, what to do ?
typedef enum {
    MA_KEEP,
    MA_REMOVE,
} MessageAction;

typedef struct {
    void*           initialState;
    uint32_t        maxMessagePerCycle;
    uint32_t        messageCap;
    ProcessHandler  handler;
    ProcessReleaseState     releaseState;
    MessageRelease  messageRelease;
} ProcessSpawnParameters;

////////////////////////////////////////////////////////////////////////////////
// API
////////////////////////////////////////////////////////////////////////////////
ProcessQueue*       ProcessQueue_init       (uint32_t procCap, uint32_t threadCount);
void                ProcessQueue_release    (ProcessQueue* dq);
SendResult          Process_sendMessage     (PID dest, void* message, MessageAction ma);
void*               Process_receiveMessage  (ProcessQueue* dq);
PID                 Process_self            (ProcessQueue* dq);
PID                 ProcessQueue_spawn      (ProcessQueue* dq, ProcessSpawnParameters* parameters);

#endif
