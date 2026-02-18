# Opus Language Reference

The definitive syntax reference for the Opus programming language (v2.0).

Opus is a multi-paradigm systems language that compiles to x86-64 machine code. It supports
multiple syntax styles — C-like, Rust-like, and even English-like — that can be mixed freely
within the same file.

---

## Table of Contents

- [Types](#types)
- [Variables](#variables)
- [Functions](#functions)
- [Control Flow](#control-flow)
- [Operators](#operators)
- [Literals](#literals)
- [Comments](#comments)
- [Structs](#structs)
- [Classes](#classes)
- [Enums](#enums)
- [Imports](#imports)
- [Extern Declarations](#extern-declarations)
- [Casting](#casting)

---

## Types

### Primitive Types

| Type | Size (bytes) | Description |
|------|:------------:|-------------|
| `void` | 0 | No value / nothing |
| `bool` | 1 | Boolean (`true` or `false`) |
| `i8` | 1 | Signed 8-bit integer |
| `u8` | 1 | Unsigned 8-bit integer |
| `i16` | 2 | Signed 16-bit integer |
| `u16` | 2 | Unsigned 16-bit integer |
| `i32` | 4 | Signed 32-bit integer |
| `u32` | 4 | Unsigned 32-bit integer |
| `i64` | 8 | Signed 64-bit integer |
| `u64` | 8 | Unsigned 64-bit integer |
| `i128` | 16 | Signed 128-bit integer |
| `u128` | 16 | Unsigned 128-bit integer |
| `f32` | 4 | 32-bit floating point |
| `f64` | 8 | 64-bit floating point |
| `ptr` | 8 | Raw pointer |
| `str` | 8+ | String (pointer to null-terminated bytes) |
| `char` | 4 | Unicode character |

### Friendly Aliases

Opus provides familiar aliases so you can write in whatever style feels natural:

| Alias | Resolves To |
|-------|:-----------:|
| `int` / `integer` | `i32` |
| `long` | `i64` |
| `short` | `i16` |
| `byte` | `i8` |
| `uint` | `u32` |
| `ulong` | `u64` |
| `ushort` | `u16` |
| `ubyte` | `u8` |
| `float` | `f32` |
| `double` / `real` | `f64` |
| `string` / `String` | `str` |
| `pointer` / `addr` / `address` | `ptr` |
| `bool` / `boolean` | `bool` |

```opus
// all of these are the same type
int x = 42
i32 y = 42
integer z = 42
```

### Implementation Note

Internally, the code generator uses 64-bit `mov` instructions everywhere. Struct fields are
forced to 8 bytes each regardless of declared type. This simplifies codegen at the cost of
some memory density.

---

## Variables

### Declaration Keywords

| Keyword | Mutability | Type Inference |
|---------|:----------:|:--------------:|
| `let` | Immutable | Yes |
| `var` | Mutable | Yes |
| `const` | Immutable (constant) | Yes |
| `auto` | Mutable | Yes |
| `mut` | Mutable | Yes |

### Syntax Styles

```opus
// with optional type annotation
let x: int = 42
let name: str = "opus"

// type-inferred
let x = 42
var counter = 0

// c++ style — type first, mutable by default
int x = 42
float speed = 3.14
str greeting = "hello"

// auto keyword — mutable with inference
auto result = compute()

// mut keyword — explicitly mutable
mut counter = 0
counter = counter + 1
```

### Global Variables

Both `let` and `var` work at the top level with literal initializers:

```opus
let MAX_HEALTH = 100
let TITLE = "My Game"
var global_counter = 0
```

> **Note:** Global initializers must be literals (numbers, strings). Function calls or
> expressions in global initializers are not yet supported.

### Semicolons

Semicolons are optional. Both styles work and can be mixed:

```opus
let x = 42
let y = 100;
var z = x + y
```

---

## Functions

Opus supports several function declaration styles. They all compile to the same thing.

### Primary Style (v2.0)

```opus
function int add(int a, int b) {
    return a + b
}

function void greet(str name) {
    print("hello ")
    print(name)
    print("\n")
}

function int main() {
    alloc_console()
    greet("world")
    return 0
}
```

### Keyword Aliases

`func` and `fn` are aliases for `function`. `ret` is an alias for `return`.

```opus
// all equivalent
function int square(int x) { return x * x }
func int square(int x) { ret x * x }
fn int square(int x) { return x * x }
```

### Type-First (No Keyword)

If you omit the keyword entirely, Opus treats the return type as the start of the declaration:

```opus
int add(int a, int b) {
    return a + b
}

void do_stuff() {
    print("doing stuff\n")
}
```

### Legacy Rust-Style

For those coming from Rust, the arrow-return syntax works too:

```opus
fn add(a: i32, b: i32) -> i32 {
    return a + b
}

fn greet(name: str) -> void {
    print(name)
    print("\n")
}
```

### English Style

Yes, really:

```opus
define function add with parameter a as i32 and parameter b as i32 returning i32
    return a + b
end function
```

### Parameter Mutability

In v2.0 style (`function`/`func`/`fn` with type-first params), parameters are mutable by
default. You can reassign them freely:

```opus
function int clamp(int val, int lo, int hi) {
    if val < lo { val = lo }
    if val > hi { val = hi }
    return val
}
```

### Stack Space

Every function gets 1024 bytes of stack space. This is a fixed allocation — keep large
buffers on the heap.

---

## Control Flow

### if / else if / else

No parentheses needed around the condition. Braces are required.

```opus
function void classify(int x) {
    if x > 0 {
        print("positive\n")
    } else if x < 0 {
        print("negative\n")
    } else {
        print("zero\n")
    }
}
```

### while

```opus
function void countdown(int n) {
    while n > 0 {
        print_int(n)
        n = n - 1
    }
    print("liftoff!\n")
}
```

### for ... in range

Iterates from `start` to `end - 1` (exclusive upper bound):

```opus
// prints 0 through 9
for i in range(0, 10) {
    print_int(i)
}
```

### loop

Infinite loop. Use `break` to exit:

```opus
loop {
    let input = read_input()
    if input == 0 {
        break
    }
    process(input)
}
```

### break and continue

```opus
for i in range(0, 100) {
    if i % 2 == 0 {
        continue       // skip even numbers
    }
    if i > 50 {
        break          // stop after 50
    }
    print_int(i)
}
```

`cont` is an alias for `continue`.

### parallel for

Splits loop iterations across CPU cores:

```opus
// each iteration may run on a different thread
parallel for i in range(0, 1000) {
    process_item(i)
}
```

> **Warning:** The loop body must be thread-safe. No shared mutable state without atomics.

---

## Operators

### Precedence Table (Highest to Lowest)

| Prec | Category | Operators | Associativity |
|:----:|----------|-----------|:-------------:|
| — | Postfix | `()` call, `[]` index, `.` field, `as` cast, `++` `--` | Left-to-right |
| — | Prefix | `-` neg, `!`/`not`, `~` bitnot, `*` deref, `&` addrof, `++` `--` | Right-to-left |
| 10 | Multiplicative | `*`  `/`  `%` | Left-to-right |
| 9 | Additive | `+`  `-` | Left-to-right |
| 8 | Shift | `<<`  `>>` | Left-to-right |
| 7 | Relational | `<`  `>`  `<=`  `>=` | Left-to-right |
| 6 | Equality | `==`  `!=` | Left-to-right |
| 5 | Bitwise AND | `&` | Left-to-right |
| 4 | Bitwise XOR | `^` | Left-to-right |
| 3 | Bitwise OR | `\|` | Left-to-right |
| 2 | Logical AND | `&&` / `and` | Left-to-right |
| 1 | Logical OR | `\|\|` / `or` | Left-to-right |
| 0 | Assignment | `=`  `+=`  `-=`  `*=`  `/=` | Right-to-left |

### Arithmetic

```opus
let sum = a + b
let diff = a - b
let prod = a * b
let quot = a / b
let rem = a % b
```

### Comparison

```opus
if x == y { print("equal\n") }
if x != y { print("not equal\n") }
if x < y  { print("less\n") }
if x >= y { print("greater or equal\n") }
```

### Logical

```opus
// symbol or english — your choice
if a > 0 && b > 0 { print("both positive\n") }
if a > 0 and b > 0 { print("both positive\n") }

if x == 0 || y == 0 { print("at least one zero\n") }
if x == 0 or y == 0 { print("at least one zero\n") }

if !done { print("still going\n") }
if not done { print("still going\n") }
```

### Bitwise

```opus
let masked = value & 0xFF
let combined = flags | 0x01
let flipped = bits ^ 0xFF
let inverted = ~bits
let shifted_left = x << 4
let shifted_right = x >> 4
```

### Increment / Decrement

```opus
var i = 0
i++          // post-increment
i--          // post-decrement
++i          // pre-increment
--i          // pre-decrement
```

### Compound Assignment

```opus
var x = 10
x += 5       // x = 15
x -= 3       // x = 12
x *= 2       // x = 24
x /= 4       // x = 6
```

---

## Literals

### Integers

```opus
let decimal = 42
let hex = 0xFF
let binary = 0b1010
let big = 1000000
```

### Floats

```opus
let pi = 3.14
let half = 0.5
```

### Strings

Strings are null-terminated byte sequences. `print()` does not append a newline — add `\n`
explicitly.

```opus
let greeting = "Hello, World!\n"
let path = "C:\\Users\\opus\\file.txt"
let tab_separated = "name\tvalue\n"
```

#### Escape Sequences

| Sequence | Character |
|----------|-----------|
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |
| `\0` | Null byte |

### Hex Strings

Raw byte buffers from hex notation:

```opus
// produces a buffer with those exact bytes
let shellcode = hex"48 89 5C 24 08 48 89 6C 24 10"
```

### Characters

```opus
let ch = 'A'
let newline = '\n'
```

### Booleans

```opus
let a = true
let b = false
let c = yes       // same as true
let d = no        // same as false
```

---

## Comments

```opus
// single-line comment

/* multi-line
   comment that spans
   several lines */

let x = 42  // inline comment
```

---

## Structs

### C-Style Definition

```opus
struct Point {
    int x;
    int y;
}
```

### Rust-Style Definition

```opus
struct Point {
    x: int,
    y: int,
}
```

### Mixed Syntax

Both field styles can coexist in the same struct:

```opus
struct Entity {
    int id;
    name: str,
    float speed;
}
```

### Struct Literals

Create instances using the struct name followed by braces. The compiler recognizes this
pattern when the name starts with an uppercase letter:

```opus
let origin = Point { x: 0, y: 0 }
let player_pos = Point { x: 100, y: 200 }
```

### Field Access

```opus
let px = origin.x
let py = origin.y
print_int(px)
```

### Memory Layout

- Each field occupies 8 bytes regardless of declared type
- Structs are heap-allocated via `HeapAlloc`
- You must call `free(p)` when done — there is no automatic cleanup

```opus
let p = Point { x: 10, y: 20 }
print_int(p.x)
print_int(p.y)
free(p)    // dont forget this
```

---

## Classes

Classes are structs with methods.

### Definition

```opus
class Player {
    health: int,
    speed: int,

    function void takeDamage(int amount) {
        self.health = self.health - amount
    }

    function int getHealth() {
        return self.health
    }

    function bool isAlive() {
        return self.health > 0
    }
}
```

### Creating Instances

```opus
var p = Player { health: 100, speed: 50 }
```

### Method Calls

```opus
p.takeDamage(30)
let hp = p.getHealth()    // 70
```

### The `self` Keyword

Inside methods, `self` refers to the instance. Use it to read and write fields:

```opus
class Enemy {
    health: int,
    damage: int,

    function void heal(int amount) {
        self.health = self.health + amount
    }

    function void attack(ptr target) {
        // self.damage reads this enemys damage field
        let d = self.damage
        // apply to target...
    }
}
```

### How Methods Compile

Methods compile to global functions with the instance as the first argument:

```opus
// this call:
p.takeDamage(30)

// compiles to:
Player_takeDamage(p, 30)
```

### Field Modification

```opus
var p = Player { health: 100, speed: 50 }
p.health = 200
p.speed += 10
```

### Chained Field Access

```opus
struct Inner { int value; }
struct Outer { ptr inner; }

let i = Inner { value: 42 }
let o = Outer { inner: i }
print_int(o.inner.value)    // 42
```

---

## Enums

### Definition

```opus
enum Direction {
    North,
    South,
    East,
    West,
}
```

Values auto-increment from 0: `North = 0`, `South = 1`, `East = 2`, `West = 3`.

### Explicit Values

```opus
enum TokenKind {
    Ident,          // 0
    IntLit,         // 1
    StringLit,      // 2
    Operator = 10,  // 10
    Keyword,        // 11
    Eof,            // 12
}
```

After an explicit value, subsequent variants continue incrementing from there.

### Usage

Access variants with dot syntax:

```opus
let dir = Direction.North

if dir == Direction.North {
    print("heading north\n")
}
```

Enum values are compile-time constants — they get inlined directly.

---

## Imports

### Basic Import

```opus
import math          // loads math.op from same directory
```

### Nested Paths

```opus
import utils.strings  // loads utils/strings.op
```

### Aliased Import

```opus
import graphics.renderer as gfx
```

### How It Works

1. `import module_name` resolves to `module_name.op` relative to the current file
2. `import foo.bar` resolves to `foo/bar.op`
3. Compilation is two-pass: all function signatures are collected first, then code is generated
4. Circular imports are detected and prevented

```opus
// file: math.op
function int square(int x) {
    return x * x
}

// file: main.op
import math

function int main() {
    alloc_console()
    let result = square(5)
    print_int(result)
    return 0
}
```

---

## Extern Declarations

Declare external functions (from DLLs or other object files):

```opus
extern fn MessageBoxA(ptr hwnd, str text, str caption, int flags) -> int;
extern fn GetTickCount() -> int;
```

This tells the compiler the function exists but is defined elsewhere. The linker resolves it
at load time.

---

## Casting

Use the `as` keyword to cast between types:

```opus
let raw = malloc(64)
let value = raw as int

let addr = 0xDEADBEEF as ptr
let byte_val = big_number as i8
```

Casts are unchecked — you are responsible for making sure the conversion makes sense.

```opus
function void example() {
    let p = malloc(8)
    mem_write(p, 42)
    let val = mem_read(p) as int
    print_int(val)
    free(p)
}
```

---

## Putting It All Together

A complete program that demonstrates multiple features:

```opus
import utils

struct Player {
    int health;
    int score;
}

enum GameState {
    Menu,
    Playing,
    GameOver,
}

function void print_player(ptr p) {
    print("health: ")
    print_int(p.health)
    print("score: ")
    print_int(p.score)
}

function int main() {
    alloc_console()

    var p = Player { health: 100, score: 0 }
    var state = GameState.Playing

    while state == GameState.Playing {
        p.score += 10
        p.health -= 1

        if p.health <= 0 {
            state = GameState.GameOver
        }
    }

    print("game over!\n")
    print_player(p)
    free(p)
    return 0
}
```
