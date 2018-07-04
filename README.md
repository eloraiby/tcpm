# tcpm
Tiny Cooperative Process Management Library (C11), using **lockfree** bounded message queues and green/lightweight processes.

### WARNING
since **TCPM** is written in **C**, a lot of garantees that exist in **erlang** vanish! The semantics are also different and require more head scratching, namely:
* bounded queues instead of unbounded queues (erlang uses ubounded queues)
* sending message can fail (if the bounded message queue is full)
* no preemption: we are using C and pthreads under the hood, we have no preemption capabilities by default, and if we want to do that would require a lot of lowlevel assembly hacks.
* it uses **C** (by default safety and productiviy is out of the window - You live in the debugger)


Use only if you need performance **AND** you know what you are doing. **YOU HAVE BEEN WARNED!!!**



