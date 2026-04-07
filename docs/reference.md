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
for i in values {
    print_int(i)
}
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
| `int` / `integer` | 32-bit signed |
| `long` / `isize` | 64-bit signed |
| `uint` / `usize` | 64-bit or 32-bit friendly unsigned aliases as defined by the compiler |
| `i8`, `i16`, `i32`, `i64`, `i128` | exact-width signed |
| `u8`, `u16`, `u32`, `u64`, `u128` | exact-width unsigned |
| `float` / `f32` | 32-bit float |
| `double` / `real` / `f64` | 64-bit float |
| `str` / `string` | string |
| `char` | character |
| `ptr` / `address` / `pointer` | raw pointer |

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
let buf: [i32; 16]
```

## Structs

```c
struct Point {
    int x
    y: int
}
```

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
