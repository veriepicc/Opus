# Language Reference

This is the concise syntax reference for the current parser and compiler surface.

## Programs

```c
function int main() {
    return 0
}
```

`main` is the entry point for normal EXE and DLL-native-image output.

## Functions

### `function`

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

Return type inference is supported for `fn` declarations when the body makes the type obvious:

```c
fn add(left: int, right: int) => left + right
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

### Expression body

```c
function int add(int left, int right) => left + right
fn add(left: int, right: int) -> int => left + right
fn add(left: int, right: int) => left + right
int add(int left, int right) => left + right
```

## Parameters

Supported parameter forms:

```c
function int add(int left, int right) { ... }
function int add(left: int, right: int) { ... }
fn add(left: int, right: int) -> int { ... }
```

Trailing commas are accepted in high-touch places such as parameter lists and call arguments.

## Variables

```c
let x = 1
var y = 2
auto z = x + y
int copy = z
```

## Constants And Resolved Values

Top-level compile-time constants:

```c
constant answer: int = 41
constant total: int = answer + 1
```

Top-level load-time resolved values:

```c
resolved cache_value: int = 42
```

Load-time cached functions:

```c
resolved function int kernel32_handle() {
    return 123
}
```

Meaning:

- `constant` is evaluated at compile time and binds as a typed literal
- `resolved` is evaluated once at startup through the global init path
- `resolved function` is evaluated once at startup and later calls read the cached result
- imported constants can be used in other top-level constants

## Visibility

Declarations can be marked with:

- `public`
- `internal`

Examples:

```c
public function int exposed() {
    return 7
}

internal function int hidden() {
    return 9
}
```

Notes:

- `internal` declarations stay inside the defining module
- `public` declarations are visible through normal imports
- current default visibility is public

## Statements

### Return

```c
return
return value
```

### If / else

```c
if x > 0 {
    return 1
} else {
    return 0
}
```

Single-statement bodies also work in the v2-style surface:

```c
if x > 0
    return 1
```

English:

```c
if x > 0 then
    return 1
else if x < 0 then
    return -1
end if
```

### Switch / case / else

V2 / mixed style:

```c
switch state {
case State.Idle:
    return 1
case State.Running:
    return 2
else:
    return 3
}
```

C-style:

```c
switch (value) {
case 1:
    return 10;
case 2:
    return 20;
else:
    return 0;
}
```

Notes:

- `else` is the default branch
- there is no fallthrough between cases
- switch currently targets non-float values

### While

```c
while x < 10 {
    x = x + 1
}
```

or:

```c
while x < 10
    x = x + 1
```

English:

```c
while x < 10 do
    x = x + 1
end while
```

### For

```c
for i in range(0, 10) {
    print_int(i)
}

for i in 0..10 {
    print_int(i)
}

for x in [10, 20, 30] {
    print_int(x)
}

for ch in "sable" {
    print_int(ch)
}
```

### Defer

Block-scoped LIFO cleanup runs on scope exit, `break`, `continue`, and `return`:

```c
let buf = malloc(64)
defer free(buf)
```

### F-strings

```c
let msg = f"count={n + 1}, ok={flag}"
defer free(msg)
```

### Loop

```c
loop {
    break
}
```

### Parallel for

```c
parallel for i in range(0, 100) {
    // thread-safe work only
}
```

## Expressions

### Calls and indexing

```c
let total = add(10, 20)
let item = arr[3]
let value = obj.field
```

### Casts

```c
let p = raw_value as ptr
```

### Arrays

```c
let values = [1, 2, 3]
let more = [1, 2, 3,]
```

### Struct/class literals

```c
let p = Point { x: 10, y: 20 }
let pair = Pair { left, right }
let acc = counter { value: 0 }
```

### Unsupported expression forms

These are intentionally rejected today:

- `if` expressions
- block expressions used as values

## Types

Common primitive and friendly names:

| Name | Meaning |
|------|---------|
| `bool` | boolean |
| `byte` | 8-bit signed |
| `short` | 16-bit signed |
| `int` / `integer` / `long` / `isize` | 64-bit signed |
| `ubyte` | 8-bit unsigned |
| `ushort` | 16-bit unsigned |
| `uint` | 32-bit unsigned |
| `ulong` / `usize` | 64-bit unsigned |
| `float` | 32-bit float |
| `double` / `real` | 64-bit float |
| `str` / `string` | string |
| `char` | character |
| `ptr` / `address` / `pointer` | raw pointer |
| `i8`..`i128`, `u8`..`u128`, `f32`, `f64` | exact-width / legacy numeric aliases |

Function types:

```c
using Worker = fn(int, int) -> int
```

Pointer types:

```c
let p: *int = 0
let mp: *mut int = 0
```

Array types:

```c
let buf: [int; 16]
```

## Structs

```c
struct Point {
    int x
    y: int
}
```

## Views

Views are pointer-backed overlays for raw memory layouts and runtime blocks.

Use them when the code is really describing:

- a hook/runtime state block
- a reverse-engineered engine layout
- a packed memory region
- a PE/header overlay

Syntax:

```c
view RuntimeState {
    state: ptr @ 0
    hits: int @ 8
    ready: bool @ 12
}
```

Named offsets are also valid:

```c
enum RuntimeOffset {
    State = 0,
    Hits = 8,
    Ready = 12,
}

constant marker_offset: int = 16

view RuntimeState {
    state: ptr @ RuntimeOffset.State
    hits: int @ RuntimeOffset.Hits
    ready: bool @ RuntimeOffset.Ready
    marker: long @ marker_offset
}
```

Use a raw pointer as the backing storage and cast it to the view type:

```c
let block = malloc(16)
let rt = block as RuntimeState

rt.hits = 42
if rt.ready {
    print_int(rt.hits)
}
```

Notes:

- view fields use explicit byte offsets
- offsets may be integer literals, top-level `constant`s, or enum variants
- the view value itself is a pointer-like handle, not an inline struct value
- field reads and writes go directly to the backing memory
- views are meant to replace wrapper spam like `read_ptr(block, Slot.X)` or `function ptr state()`

## Classes

```c
class Counter {
    int value

    fn bump(delta: int) -> int => self.value + delta
    function int mix(int a, int b) => self.value + a + b
    int raw(int x) => self.value + x
}
```

## Enums

```c
enum State {
    Idle,
    Running,
    Dead,
}
```

## Imports

```c
import math
import math as maths
import api.entry
```

## Extern declarations

```c
extern fn Sleep(int ms) -> int
extern function int GetTickCount()
```

## Useful notes

- Semicolons are optional in most day-to-day code.
- The parser accepts mixed syntax in one file.
- Error recovery is typo-aware in many common cases now, including English keywords and missing commas.
