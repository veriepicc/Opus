# Language Overview

Opus is designed to stay expressive at the source level while keeping the underlying compiler and runtime model understandable.

## Core ideas

- direct native code generation
- EXE by default, DLL with `--dll`
- `--run` builds and runs a temporary EXE
- semicolons are optional
- multiple declaration styles can coexist in one file
- a module-first standard library is preferred for new code
- the implementation aims to stay honest about what it is doing

## Entry point

```c
function int main() {
    print("running\n")
    return 0
}
```

## Function declaration styles

### Modern `function`

```c
function int add(int left, int right) {
    return left + right
}
```

### `fn`

```c
fn add(left: int, right: int) -> int {
    return left + right
}
```

### Type-first

```c
int add(int left, int right) {
    return left + right
}
```

### English

```c
define function add with left as i64, right as i64 returning i64
    return left + right
end function
```

### Expression bodies

```c
function int add(int left, int right) => left + right
fn add(left: int, right: int) -> int => left + right
int add(int left, int right) => left + right
```

This mix is deliberate: the language accepts multiple declaration styles without treating them as separate sub-languages.

## Variables

```c
let answer = 42
var counter = 0
auto total = add(10, 20)
int local_copy = answer
```

- `let` is immutable
- `var` is mutable
- `auto` is mutable with inference
- type-first local declarations also work

## Structs, classes, and enums

```c
struct Point {
    int x
    y: int
}
```

```c
class Counter {
    int value

    fn bump(delta: int) -> int => self.value + delta
}
```

```c
enum State {
    Idle,
    Running,
    Dead,
}
```

Literals:

```c
let p = Point { x: 10, y: 20 }
let pair = Pair { left, right }
```

## Imports and projects

```c
import math
```

```c
project Demo {
    entry: "app/main.op"
    output: "Demo.exe"
    mode: "exe"
    include: ["modules"]
}
```

## Memory

For new code, prefer the `mem` module:

```c
import mem

function int main() {
    let buf = malloc(16)
    mem.write32(buf, 123)
    let value = mem.read32(buf)
    free(buf)
    return value
}
```

## Concurrency

Supported surface:

- `spawn func(args)`
- `await handle`
- `parallel for i in range(start, end)`
- `atomic_load`, `atomic_store`, `atomic_add`, `atomic_cas`

```c
function int work(int x) {
    return x * x
}

function int main() {
    let handle = spawn work(7)
    let result = await handle
    print_int(result)
    return 0
}
```

## FFI

```c
using GetPidFn = fn() -> int

function int main() {
    let kernel32 = get_module("kernel32.dll")
    let get_pid = get_proc(kernel32, "GetCurrentProcessId") as GetPidFn
    print_int(get_pid())
    return 0
}
```

## Parser quality

The parser now handles many common mistakes more gracefully than it used to, including:

- declaration starter typos
- English keyword typos
- statement keyword typos
- missing commas in common list forms

The goal is not to accept everything; it is to reject bad code in a way that is still understandable and useful.
