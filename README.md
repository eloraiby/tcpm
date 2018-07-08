# tcpm
Tiny Cooperative Process Management Library (C11), using **lockfree** bounded message queues and green/lightweight processes.

### WARNING
since **TCPM** is written in **C**, a lot of garantees that exist in **erlang** vanish! The semantics are also different and require more head scratching, namely:
* bounded queues instead of unbounded queues (erlang uses ubounded queues)
* sending message can fail (if the bounded message queue is full)
* no preemption: we are using C and pthreads under the hood, we have no preemption capabilities by default, and if we want to do that, it would require a lot of lowlevel assembly hacks.
* managing processes lifetime becomes tricky due to the lack of garbage collection: especially when other processes hold references.
* it uses **C** (by default, safety and productiviy is out of the window as you, now, officialy live in the debugger)

Use only if and only if (all must be checked):
1. You have implemented your system in erlang or F#
2. **AND** you have optimized your system
3. **AND** you have tried harder to reoptimize your system
4. **AND** no further optimization is possible
5. **AND** you know what you are doing.

**YOU HAVE BEEN WARNED!!!**

## Concepts
- **Cycle**: Number of messages to be processed when a process lands in the executing worker thread.
- **Process**: A lightweight thread. A process can yield execution to other threads either using special coded return values. Practically, a process is a re-entrant callback function. To keep the code simple, a process doesn't have any kind of priority. When a process is spawned, the maximum number of messages to process per process cycle must be specified. This is the closest thing to priorities.
- **Message Box**: Each spawned process has a message box that can accept a limited number of messages. This number is specified when the process is created by its parent process.
- **Process Queue**: The structure that holds the processes as they are processed in-order. Worker threads take processes from this queue and consume them. If a process is preempted, it's puhed back into the queue.

## API
#### ProcessQueue
Every process belongs to a dispatcher queue.
* `ProcessQueue* ProcessQueue_init(uint32_t procCap, uint32_t threadCount)`: create a new process queue, with a maximum number of process `procCap` that can be alive at the same time, and the number of process working threads `threadCount`. Ideally, `threadCount` should match the number of logical cores you have on your CPU.

* `void ProcessQueue_release(ProcessQueue* dq)`: set the termination flags and join the worker threads until they finish.

* `Process* ProcessQueue_spawn(ProcessQueue* dq, ProcessSpawnParameters* parameters)`: spawn a new process on the process queue with the appropriate parameters. This will return `NULL` if the maximum number of live process is reached. All parameters passed in `parameters` are owned by the process queue, as such even on creation failure, the processqueue will release all the associated objects.

#### Process
* `Process* Process_parent(Process* proc)`: get the process parent (could be `NULL` if the process is the root process).

* `SendResult Process_sendMessage(Process* dest, void* message)`: send a message to another process. The destination process owns the message if the send was successfull, otherwise the message is released using the destination process message release function.

* `void* Process_receiveMessage(ProcessQueue* dq)`: receive a message. This could be `NULL` if no message is available. The receiving process has the responsibility to release the message data.

* `Process* Process_self(ProcessQueue* dq)`: return the current process handle (cannot be `NULL`)
