# tcpm
Tiny Cooperative Process Management Library (C11), using **lockfree** bounded message queues and green/lightweight processes.

### WARNING
since **TCPM** is written in **C**, a lot of garantees that exist in **erlang** vanish! The semantics are also different and require more head scratching, namely:
* bounded queues instead of unbounded queues (erlang uses ubounded queues)
* sending message can fail (if the bounded message queue is full)
* no preemption: we are using C and pthreads under the hood, we have no preemption capabilities by default, and if we want to do that, it would require a lot of lowlevel assembly hacks.
* it uses **C** (by default, safety and productiviy is out of the window as you, now, officialy live in the debugger)

Use only if and only if (all must be checked):
1. You have implemented your system in erlang or F#
2. **AND** you have optimized your system
3. **AND** you have tried harder to reoptimize your system
4. **AND** you need more performance
5. **AND** you know what you are doing.

**YOU HAVE BEEN WARNED!!!**

## Concepts
- **Cycle**: Number of messages to be processed when a process lands in the executing worker thread.
- **Process**: A lightweight thread. A process can yield execution to other threads either using special coded return values. Practically, a process is a re-entrant callback function. To keep the code simple, a process doesn't have any kind of priority. When a process is spawned, the maximum number of messages to process per process cycle must be specified. This is the closest thing to priorities.
- **Message Box**: Each spawned process has a message box that can accept a limited number of messages. This number is specified when the process is created by its parent process.
- **Dispatcher Queue**: The structure that holds the processes as they are processed in-order. Worker threads take processes from this queue and consume them. If a process is preempted, it's puhed back into the queue.


