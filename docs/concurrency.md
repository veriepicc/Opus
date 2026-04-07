# Concurrency

Concurrency is a real part of the current language surface:

- `spawn`
- `await`
- `parallel for`
- atomics

It is backed by compiler/runtime behavior and integration coverage.

## Why it matters

Threading and data-parallel execution are where a language has to prove that its runtime model is real.

```c
function int compute(int x) {
    return x * x
}

function int main() {
    let handle = spawn compute(7)
    let result = await handle
    print_int(result)
    return 0
}
```

That is part of the language today.

## `spawn` / `await`

`spawn` launches a function call on a new thread and returns a handle.

```c
function int worker(int id) {
    sleep(25)
    return id * 10
}

function int main() {
    let h1 = spawn worker(1)
    let h2 = spawn worker(2)
    let r1 = await h1
    let r2 = await h2
    print_int(r1 + r2)
    return 0
}
```

Rules:

- `spawn` expects a call shape like `spawn work(1, 2)`
- `await` blocks for the handle and yields the result
- this works through the native-image path, including `--run`

## `parallel for`

This is the data-parallel side of the model: loop-level parallel structure directly in the language.

```c
parallel for i in range(0, 100) {
    // thread-safe work
}
```

## Shared state with atomics

```c
import mem

function int main() {
    let sum = malloc(8)
    mem.write(sum, 0)

    parallel for i in range(1, 101) {
        atomic_add(sum, i)
    }

    print_int(mem.read(sum))
    free(sum)
    return 0
}
```

Available atomic operations:

- `atomic_load`
- `atomic_store`
- `atomic_add`
- `atomic_cas`

## What makes this practical

- the parser understands the surface directly
- the runtime and codegen paths support it directly
- invalid usage is rejected instead of being silently miscompiled
- integration coverage exercises both positive and negative concurrency paths

This is compiler-tested surface area, not demo-only syntax.

## Constraints

- do not mutate captured locals directly inside `parallel for`
- do not mutate globals directly inside `parallel for`
- prefer atomics or disjoint output ranges
- use `spawn` when explicit handles and results are useful

Those constraints reflect the runtime model rather than hiding it behind unsafe convenience.

## Practical split

Use `spawn` when:

- explicit handles are useful
- a task result is needed back
- the code is naturally task-oriented

Use `parallel for` when:

- the problem is loop-parallel
- work splits naturally by range or index
- the body can be kept thread-safe

## Bottom line

The concurrency model is intentionally direct: explicit task spawning when handles are needed, explicit data parallelism when loop-level work sharing is the right shape, and atomics when shared state is unavoidable.
