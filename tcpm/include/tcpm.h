#ifndef __TCPM__H__
#define __TCPM__H__

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
