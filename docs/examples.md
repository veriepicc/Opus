# Examples

The `examples/` folder is meant to compile against the current compiler surface, not an older draft of the language.

Important note:

- normal EXE examples do not need `alloc_console()`
- the DLL scanner example still uses `alloc_console()` on purpose because it is meant to run inside a host process

## Quick compile commands

```text
opus examples/hello.op
opus --run examples/fibonacci.op
opus examples/classes.op
opus --dll examples/scanner.op
```

## Example map

| File | Focus |
|------|-------|
| `examples/hello.op` | smallest valid program |
| `examples/fibonacci.op` | recursion and loops |
| `examples/strings.op` | string helpers and character inspection |
| `examples/arrays.op` | arrays, indexing, sorting |
| `examples/structs.op` | struct literals and nested field access |
| `examples/classes.op` | classes, methods, `self` |
| `examples/enums.op` | enum states and control flow |
| `examples/memory.op` | `mem` module reads/writes |
| `examples/concurrency.op` | `spawn`, `await`, `parallel for`, atomics |
| `examples/benchmark.op` | sequential vs parallel timing |
| `examples/ffi.op` | typed FFI with `get_module` / `get_proc` |
| `examples/scanner.op` | DLL-oriented memory scanning |

## Hello

```c
function int main() {
    print("Hello, World!\n")
    return 0
}
```

## Functions

From `examples/fibonacci.op`:

```c
function int fib(int n) {
    if n <= 1 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}
```

## Struct and class literals

From `examples/structs.op` and `examples/classes.op`:

```c
let p = Vec2 { x: 10, y: 20 }
var warrior = Player { health: 100, armor: 60, name: "warrior" }
```

Struct/class field shorthand also works when the names match:

```c
let left = 10
let right = 20
let pair = Pair { left, right }
```

## Memory surface

For new examples, prefer the `mem` module:

```c
import mem

function int main() {
    let buf = malloc(16)
    mem.write32(buf, 123)
    if mem.read32(buf) != 123 {
        return 1
    }
    free(buf)
    return 0
}
```

## Concurrency

From `examples/concurrency.op`:

```c
import mem

function int compute(int x) {
    sleep(50)
    return x * x
}

function int main() {
    let h1 = spawn compute(3)
    let h2 = spawn compute(7)
    let r1 = await h1
    let r2 = await h2

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

## FFI

From `examples/ffi.op`:

```c
using GetPidFn = fn() -> int
using MessageBoxAFn = fn(ptr, str, str, int) -> int

function int main() {
    let kernel32 = get_module("kernel32.dll")
    let get_pid = get_proc(kernel32, "GetCurrentProcessId") as GetPidFn
    let pid = get_pid()
    print_int(pid)
    return 0
}
```

## DLL example

`examples/scanner.op` is intentionally DLL-oriented:

- compile with `opus --dll examples/scanner.op`
- it calls `alloc_console()` because a DLL host may not already have a console

## Related pages

- [getting-started.md](getting-started.md)
- [language-overview.md](language-overview.md)
- [reference.md](reference.md)
- [concurrency.md](concurrency.md)
- [dll.md](dll.md)
