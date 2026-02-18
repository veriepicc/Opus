# Concurrency Guide

Everything you need to know about threading, parallelism, and atomic operations in Opus.

---

## Table of Contents

- [Overview](#overview)
- [spawn / await](#spawn--await)
- [parallel for](#parallel-for)
- [Atomic Operations](#atomic-operations)
- [Thread Safety Rules](#thread-safety-rules)
- [Examples](#examples)

---

## Overview

Opus has built-in concurrency primitives that compile directly to Windows threading APIs.
No external libraries, no runtime overhead — just raw `CreateThread` and
`WaitForSingleObject` calls under the hood.

What you get:
- `spawn` / `await` — fire off a function on a new thread, get the result back
- `parallel for` — automatically split loop iterations across CPU cores
- Atomic operations — lock-free shared state for concurrent code

All of this works in EXE mode, DLL mode, and JIT mode.

---

## spawn / await

### Basic Usage

`spawn` launches a function on a new thread and returns a handle. `await` blocks until
that thread finishes and gives you the return value.

```opus
function int compute(int x) {
    // runs on a separate thread
    return x * x + 1
}

function int main() {
    alloc_console()
    
    let handle = spawn compute(7)
    print("thread is running...\n")
    
    // blocks until compute() returns
    let result = await handle
    print("result: ")
    print_int(result)    // 50
    
    return 0
}
```

### How It Works Under the Hood

1. `spawn func(args)` calls `CreateThread` with a wrapper that invokes your function
2. The wrapper passes arguments and stores the return value
3. Each spawned thread gets its own stack
4. `await handle` calls `WaitForSingleObject(handle, INFINITE)` to block until completion
5. The return value is retrieved from the thread's storage

### Rules

- The spawned function must return `int`
- You can pass arguments normally — they are captured at spawn time
- Each `spawn` creates a real OS thread (not a green thread or fiber)
- You must `await` every handle to avoid resource leaks
- Spawned threads can call any builtin: `print`, `malloc`, `sleep`, etc.

### Multiple Threads

```opus
function int worker(int id) {
    print("worker ")
    print_int(id)
    print(" started\n")
    
    // simulate work
    sleep(100)
    
    return id * 100
}

function int main() {
    alloc_console()
    
    // fire off 4 threads
    let h1 = spawn worker(1)
    let h2 = spawn worker(2)
    let h3 = spawn worker(3)
    let h4 = spawn worker(4)
    
    // collect results (order doesnt matter, await blocks per handle)
    let r1 = await h1
    let r2 = await h2
    let r3 = await h3
    let r4 = await h4
    
    let total = r1 + r2 + r3 + r4
    print("total: ")
    print_int(total)    // 1000
    
    return 0
}
```

### Thread with Shared State

If threads need to share data, use pointers and atomic operations:

```opus
function int counter_thread(ptr counter) {
    for i in range(0, 5000) {
        atomic_add(counter, 1)
    }
    return 0
}

function int main() {
    alloc_console()
    
    let counter = malloc(8)
    mem_write(counter, 0)
    
    let h1 = spawn counter_thread(counter)
    let h2 = spawn counter_thread(counter)
    let h3 = spawn counter_thread(counter)
    
    await h1
    await h2
    await h3
    
    let val = atomic_load(counter)
    print("counter: ")
    print_int(val)    // 15000
    
    free(counter)
    return 0
}
```

---

## parallel for

### Basic Usage

`parallel for` splits loop iterations across CPU cores automatically:

```opus
function int main() {
    alloc_console()
    
    // each iteration may run on a different core
    parallel for i in range(0, 100) {
        // do work with i
    }
    
    print("all iterations complete\n")
    return 0
}
```

### How It Works Under the Hood

1. `GetSystemInfo` is called to detect the number of CPU cores
2. Thread count is capped at `min(num_cores, range_size)`
3. The range is divided into contiguous chunks — each thread gets a slice
4. `CreateThread` is called for each chunk
5. The main thread calls `WaitForSingleObject` on each handle
6. Execution continues only after all threads finish

For example, `parallel for i in range(0, 100)` on an 8-core machine:
- Thread 0 handles i = 0..12
- Thread 1 handles i = 13..24
- Thread 2 handles i = 25..37
- ...and so on

The stack frame is extended to hold thread data (handles, range bounds, etc.).

### Parallel Sum

The classic use case — summing values across threads with atomic add:

```opus
function int main() {
    alloc_console()
    
    // build an array of values
    let arr = malloc(800)    // 100 elements * 8 bytes
    for i in range(0, 100) {
        mem_write(arr + i * 8, i + 1)
    }
    
    // shared accumulator
    let sum_ptr = malloc(8)
    mem_write(sum_ptr, 0)
    
    // parallel sum
    parallel for i in range(0, 100) {
        let val = mem_read(arr + i * 8)
        atomic_add(sum_ptr, val)
    }
    
    let total = mem_read(sum_ptr)
    print("sum 1..100 = ")
    print_int(total)    // 5050
    
    free(sum_ptr)
    free(arr)
    return 0
}
```

### Multiple parallel for in One Function

You can have multiple `parallel for` blocks — they execute sequentially (each one
completes before the next starts):

```opus
function int main() {
    alloc_console()
    
    // first parallel block
    parallel for i in range(0, 4) {
        print("phase 1\n")
    }
    
    print("--- between phases ---\n")
    
    // second parallel block
    parallel for j in range(0, 4) {
        print("phase 2\n")
    }
    
    print("done\n")
    return 0
}
```

### Code Before and After

Variables declared before a `parallel for` are accessible inside the body (captured by
value). Code after the block runs only when all threads are done:

```opus
function int main() {
    alloc_console()
    
    let multiplier = 10
    let results = malloc(80)    // 10 * 8 bytes
    
    parallel for i in range(0, 10) {
        // multiplier is captured from the outer scope
        mem_write(results + i * 8, i * multiplier)
    }
    
    // this runs after all threads finish
    for i in range(0, 10) {
        print_int(mem_read(results + i * 8))
    }
    
    free(results)
    return 0
}
```

---

## Atomic Operations

Opus provides four atomic operations that map to x86 locked instructions. Use these
whenever multiple threads access the same memory.

### atomic_add(ptr, val)

Atomically adds `val` to the 64-bit value at `ptr`. Returns the previous value.

Maps to `lock xadd` (InterlockedExchangeAdd64).

```opus
let counter = malloc(8)
mem_write(counter, 0)

// from any thread:
let old = atomic_add(counter, 1)    // old = previous value
// counter is now old + 1
```

### atomic_cas(ptr, expected, desired)

Compare-and-swap. If the value at `ptr` equals `expected`, replace it with `desired`.
Returns `1` on success, `0` on failure.

Maps to `lock cmpxchg` (InterlockedCompareExchange64).

```opus
let flag = malloc(8)
mem_write(flag, 0)

// try to set flag from 0 to 1
let success = atomic_cas(flag, 0, 1)
if success {
    print("we got the lock\n")
}
```

### atomic_load(ptr)

Atomically reads a 64-bit value with a memory fence. Guarantees you see the latest
value written by any thread.

```opus
let shared = malloc(8)
// ... other threads writing to shared ...

let current = atomic_load(shared)
print_int(current)
```

### atomic_store(ptr, val)

Atomically writes a 64-bit value with a memory fence. Guarantees other threads see
this write.

```opus
let shared = malloc(8)
atomic_store(shared, 42)
// all threads will now see 42
```

### Quick Reference

| Function | Operation | x86 Instruction |
|----------|-----------|-----------------|
| `atomic_add(ptr, val)` | `*ptr += val` | `lock xadd` |
| `atomic_cas(ptr, exp, des)` | if `*ptr == exp` then `*ptr = des` | `lock cmpxchg` |
| `atomic_load(ptr)` | read with fence | `mov` + `mfence` |
| `atomic_store(ptr, val)` | write with fence | `mov` + `mfence` |

---

## Thread Safety Rules

### The Golden Rules

1. **No shared mutable state without atomics.** If two threads can write to the same
   address, you must use `atomic_add`, `atomic_cas`, `atomic_store`, etc.

2. **Struct fields are NOT atomic by default.** Writing to `self.health` from two threads
   is a data race. Use a pointer + atomics instead.

3. **parallel for body must be thread-safe.** Different iterations should not write to
   the same memory location unless using atomics.

4. **Read-only shared data is fine.** Multiple threads can read the same memory without
   atomics, as long as nobody is writing.

### Safe Patterns

```opus
// SAFE: each thread writes to its own index
let results = malloc(80)
parallel for i in range(0, 10) {
    mem_write(results + i * 8, i * i)    // no overlap
}
```

```opus
// SAFE: shared counter with atomic_add
let counter = malloc(8)
mem_write(counter, 0)
parallel for i in range(0, 100) {
    atomic_add(counter, 1)
}
// counter == 100
```

```opus
// UNSAFE: data race! dont do this
var shared = 0
parallel for i in range(0, 100) {
    shared = shared + 1    // race condition, result is undefined
}
```

```opus
// UNSAFE: two threads writing same struct field
var p = Player { health: 100 }
let h1 = spawn damage_player(p)
let h2 = spawn damage_player(p)
// both writing self.health without atomics = bad
```

### Lock-Free Spinlock with atomic_cas

For more complex critical sections, you can build a spinlock:

```opus
function void lock(ptr mutex) {
    // spin until we swap 0 -> 1
    loop {
        let got_it = atomic_cas(mutex, 0, 1)
        if got_it {
            break
        }
        // spin (could add sleep(0) to yield)
    }
}

function void unlock(ptr mutex) {
    atomic_store(mutex, 0)
}

function int main() {
    alloc_console()
    
    let mutex = malloc(8)
    mem_write(mutex, 0)
    
    let shared_data = malloc(8)
    mem_write(shared_data, 0)
    
    // now you can protect critical sections:
    // lock(mutex)
    // ... do stuff with shared_data ...
    // unlock(mutex)
    
    free(mutex)
    free(shared_data)
    return 0
}
```

---

## Examples

### Basic spawn/await

```opus
function int double_it(int x) {
    return x * 2
}

function int main() {
    alloc_console()
    
    let h = spawn double_it(21)
    let result = await h
    
    print("21 * 2 = ")
    print_int(result)    // 42
    
    return 0
}
```

### Parallel Sum with atomic_add

```opus
function int main() {
    alloc_console()
    
    let sum = malloc(8)
    mem_write(sum, 0)
    
    parallel for i in range(1, 101) {
        atomic_add(sum, i)
    }
    
    print("sum 1..100 = ")
    print_int(mem_read(sum))    // 5050
    
    free(sum)
    return 0
}
```

### Producer/Consumer with atomic_cas

```opus
// simple single-slot channel using cas

function int producer(ptr slot) {
    for i in range(1, 11) {
        // wait until slot is empty (0), then write our value
        loop {
            let ok = atomic_cas(slot, 0, i)
            if ok { break }
            sleep(1)
        }
    }
    // signal done with -1
    loop {
        let ok = atomic_cas(slot, 0, -1)
        if ok { break }
        sleep(1)
    }
    return 0
}

function int consumer(ptr slot) {
    var total = 0
    loop {
        // wait until slot has a value (not 0)
        let val = atomic_load(slot)
        if val != 0 {
            if val == -1 {
                break    // producer is done
            }
            total = total + val
            // clear the slot so producer can write again
            atomic_store(slot, 0)
        } else {
            sleep(1)
        }
    }
    return total
}

function int main() {
    alloc_console()
    
    let slot = malloc(8)
    mem_write(slot, 0)
    
    let hp = spawn producer(slot)
    let hc = spawn consumer(slot)
    
    await hp
    let total = await hc
    
    print("consumer received total: ")
    print_int(total)    // 55 (sum of 1..10)
    
    free(slot)
    return 0
}
```

### Benchmark: Sequential vs Parallel

```opus
function int burn() {
    var x = 1
    for i in range(0, 10000000) {
        x = x + i * 3 + 7
    }
    return x
}

function int main() {
    alloc_console()
    print("=== sequential vs parallel ===\n\n")
    
    // sequential
    let t0 = get_tick_count()
    for i in range(0, 8) {
        burn()
    }
    let seq_ms = get_tick_count() - t0
    print("sequential 8x: ")
    print_int(seq_ms)
    print(" ms\n")
    
    // parallel
    let t1 = get_tick_count()
    parallel for i in range(0, 8) {
        burn()
    }
    let par_ms = get_tick_count() - t1
    print("parallel   8x: ")
    print_int(par_ms)
    print(" ms\n")
    
    if par_ms > 0 {
        let speedup = seq_ms / par_ms
        print("speedup: ")
        print_int(speedup)
        print("x\n")
    }
    
    return 0
}
```

### Parallel Array Processing

```opus
function int main() {
    alloc_console()
    
    let size = 1000
    let data = malloc(size * 8)
    let results = malloc(size * 8)
    
    // fill with test data
    for i in range(0, size) {
        mem_write(data + i * 8, i)
    }
    
    // process in parallel - each thread writes to its own slot
    parallel for i in range(0, size) {
        let val = mem_read(data + i * 8)
        let processed = val * val + val * 2 + 1
        mem_write(results + i * 8, processed)
    }
    
    // verify a few results
    print("results[0] = ")
    print_int(mem_read(results))           // 1
    print("results[10] = ")
    print_int(mem_read(results + 80))      // 121
    print("results[100] = ")
    print_int(mem_read(results + 800))     // 10201
    
    free(data)
    free(results)
    return 0
}
```

---

## See Also

- [Built-in Functions](builtins.md#concurrency) — atomic and thread function reference
- [Examples](examples.md#concurrency) — more concurrency examples
- [DLL Mode](dll.md) — threading works in DLL mode too
- [Language Reference](reference.md#control-flow) — `parallel for` syntax
