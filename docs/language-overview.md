# Language Overview

Opus is a compiled systems programming language targeting Windows x64. It compiles `.op` source files directly to native machine code — no LLVM, no bytecode, no VM. The compiler generates valid PE executables and DLLs from scratch.

## At a Glance

- Compiles to native x64 machine code
- Two output modes: standalone EXE or injectable DLL
- JIT mode for quick testing (`--run`)
- Flexible syntax: C-style, Rust-style, and English can coexist in one file
- Semicolons are optional
- Manual memory management (malloc/free), no garbage collector
- Direct Windows API access through FFI
- ~7ms compile times

## Entry Point

Every Opus program needs a `main` function:

```opus
function int main() {
    alloc_console()
    print("running# Getting Started
    
    This guide covers building the Opus compiler from source, writing your first program, and the full CLI reference.
    
    ## Prerequisites
    
    - **Windows 10/11 x64**
    - **Visual Studio 2022** (v17.12+) with C++23 support and the "Desktop development with C++" workload
      - The compiler is built from C++20 modules, so you need a recent MSVC toolchain
    
    ## Building the Compiler
    
    Clone the repo and build with MSBuild:
    
    ```bash
    MSBuild.exe Opus.sln /p:Configuration=Release /p:Platform=x64
    ```
    
    Or open `Opus.sln` in Visual Studio and build the Release x64 configuration.
    
    The output binary lands at:
    
    ```
    bin/Release/opus.exe
    ```
    
    Add that to your PATH or reference it directly.
    
    ## Your First Program
    
    Create a file called `hello.op`:
    
    ```opus
    function int main() {
        alloc_console()
        print("Hello, World!\n")
        return 0
    }
    ```
    
    A few things to note:
    - `function int main()` is the entry point — the compiler looks for `main`
    - `alloc_console()` creates a console window (required for EXE mode to see output)
    - `print()` does not add a newline automatically — use `\n` explicitly
    - Semicolons are optional
    
    Compile it:
    
    ```
    > opus hello.op
    Generated EXE: hello.exe (3072 bytes)
      Entry point: exe startup -> calls main()
    Functions:
      main @ 0x0
    ```
    
    Run it:
    
    ```
    > hello.exe
    Hello, World!
    ```
    
    ## Compilation Modes
    
    ### Standalone EXE (default)
    
    ```
    > opus hello.op
    ```
    
    Produces `hello.exe` — a standalone Windows executable. No runtime dependencies, no DLLs needed. The generated PE imports from `kernel32.dll` for system functions.
    
    When writing EXE programs, call `alloc_console()` at the start of `main()` so you get a console window for output.
    
    ### Windows DLL
    
    ```
    > opus --dll hello.op
    ```
    
    Produces `hello.dll` — a valid Windows DLL that calls `main()` on `DLL_PROCESS_ATTACH`. Can be loaded with `LoadLibrary` or injected into a running process.
    
    In DLL mode, `alloc_console()` creates a new console window attached to the host process.
    
    ### JIT Mode (compile and run)
    
    ```
    > opus --run hello.op
    ```
    
    Compiles to native code in memory and executes immediately. No file is written to disk. Great for quick testing and iteration.
    
    ## Multi-File Projects
    
    For projects with multiple source files, create an `opus.project` file:
    
    ```opus
    project MyApp {
        entry: "main.op"
        output: "myapp.dll"
        mode: dll
        debug: true
        healing: "auto"
        include: [
            "src/",
            "lib/"
        ]
    }
    ```
    
    Build it:
    
    ```
    > opus build
    Loading project: opus.project
    Project: MyApp (dll)
    Sources: 3 files
      - main.op
      - utils.op
      - math.op
    
    Build successful!
    Output: myapp.dll (4096 bytes)
    ```
    
    You can also import individual files directly:
    
    ```opus
    // main.op
    import my_library
    
    function int main() {
        alloc_console()
        let result = add_nums(10, 20)
        print_dec(result)
        print("\n")
        return 0
    }
    ```
    
    ```opus
    // my_library.op
    function int add_nums(int a, int b) {
        return a + b
    }
    ```
    
    The compiler resolves `import my_library` to `my_library.op` in the same directory.
    
    ## Project File Reference
    
    | Field | Description | Default |
    |-------|-------------|---------|
    | `entry` | Main source file | required |
    | `output` | Output filename | `<project_name>.<ext>` |
    | `mode` | `dll` or `exe` | `dll` |
    | `debug` | Include source maps and debug info | `false` |
    | `healing` | Crash recovery mode: `"auto"`, `"freeze"`, `"off"` | based on `debug` |
    | `include` | Directories to scan for `.op` files | `[]` |
    
    ## CLI Reference
    
    ```
    Opus Compiler v0.1.0
    
    Usage: opus [options] <file.op>
           opus repl
           opus build [path]
    ```
    
    ### Compilation
    
    | Flag | Description |
    |------|-------------|
    | `opus <file.op>` | Compile to standalone EXE (default) |
    | `opus --dll <file.op>` | Compile to Windows DLL |
    | `opus --run <file.op>` | Compile and run immediately (JIT) |
    | `opus -r <file.op>` | Short form of `--run` |
    | `-o <file>` | Set output filename |
    | `-c` | Compile only (don't link) |
    | `-S` | Output assembly listing |
    | `-O<level>` | Optimization level (0-3) |
    
    ### Project Commands
    
    | Command | Description |
    |---------|-------------|
    | `opus build` | Build project using `opus.project` in current directory (searches parent dirs) |
    | `opus build <path>` | Build using a specific project file |
    
    ### Other
    
    | Flag | Description |
    |------|-------------|
    | `opus repl` | Start the interactive REPL |
    | `--help` / `-h` | Show help |
    
    ## VS Code Extension
    
    There is a VS Code extension for Opus syntax highlighting in the `vscode-extension/` directory. Install the `.vsix` file:
    
    ```
    code --install-extension vscode-extension/opus-lang-1.0.0.vsix
    ```
    
    This gives you syntax highlighting for `.op` files.
    
    ## Next Steps
    
    - [Language Overview](language-overview.md) — understand the syntax styles and type system
    - [Language Reference](REFERENCE.md) — complete syntax reference
    - [Built-in Functions](BUILTINS.md) — everything in the standard library
    - [Examples](EXAMPLES.md) — real code to learn from
    !\n")
    return 0
}
```

In EXE mode, call `alloc_console()` at the top of `main()` to get a console window. In DLL mode, it creates a console attached to the host process.

## Syntax Styles

Opus supports multiple syntax styles. You can pick one and stick with it, or mix them freely in the same file — the lexer handles everything.

### C-Style (primary, v2.0)

The recommended style. Familiar if you know C, C++, Java, or JavaScript:

```opus
function int add(int a, int b) {
    return a + b
}

function int main() {
    alloc_console()
    let result = add(10, 20)
    print_dec(result)
    print("\n")
    return 0
}
```

### Short Forms

`func` and `fn` are aliases for `function`. `ret` is an alias for `return`:

```opus
func int square(int x) {
    ret x * x
}

fn int cube(int x) {
    return x * x * x
}
```

### Rust-Style (legacy)

The `fn` keyword with colon-typed parameters and arrow return types also works:

```opus
fn add(a: i32, b: i32) -> i32 {
    return a + b
}
```

### Type-First (no keyword)

You can omit the function keyword entirely and lead with the return type:

```opus
int add(int a, int b) {
    return a + b
}
```

### English Syntax

For the adventurous. Uses natural language keywords:

```opus
define function add with parameter a as i32, parameter b as i32 returning i32
    return a + b
end function
```

### Mixing Styles

All styles can coexist in one file. The lexer auto-detects per token:

```opus
// c-style function
function int compute(int x) {
    return x * x + 1
}

// short form
fn int helper(int n) {
    ret n + 1
}

// type-first
int add(int a, int b) {
    return a + b
}

function int main() {
    alloc_console()
    let a = compute(5)
    let b = helper(a)
    let c = add(a, b)
    print_dec(c)
    print("\n")
    return 0
}
```

## Variables

```opus
// immutable (cannot be reassigned)
let x = 42
let name = "opus"

// mutable (can be reassigned)
var counter = 0
counter = counter + 1

// typed declaration
var p: Player = malloc(8)

// auto type inference
let result = add(10, 20)
```

| Keyword | Meaning |
|---------|---------|
| `let` | Immutable variable |
| `var` | Mutable variable |
| `const` | Constant |
| `auto` | Type-inferred (C++ style) |

### Global Variables

Top-level `let` and `var` declarations work with literal initializers:

```opus
let MAX_HEALTH = 100
let GRAVITY = 10

function int main() {
    alloc_console()
    print_dec(MAX_HEALTH)
    print("\n")
    return 0
}
```

## Type System

### Primitive Types

| Type | Description | Size |
|------|-------------|------|
| `int` | Signed 64-bit integer | 8 bytes |
| `bool` | Boolean (`true`/`false`) | 1 byte |
| `void` | No value | 0 bytes |
| `ptr` | Raw pointer | 8 bytes |
| `str` | String (pointer to null-terminated data) | 8 bytes |

### Sized Integer Types (Rust-style)

| Signed | Unsigned | Size |
|--------|----------|------|
| `i8` | `u8` | 1 byte |
| `i16` | `u16` | 2 bytes |
| `i32` | `u32` | 4 bytes |
| `i64` | `u64` | 8 bytes |
| `i128` | `u128` | 16 bytes |

`isize` and `usize` are aliases for `i64` and `u64` respectively.

### Floating Point

| Type | Size |
|------|------|
| `f32` | 4 bytes |
| `f64` | 8 bytes |

### Number Literals

```opus
let decimal = 42
let hex = 0xFF
let binary = 0b1010
let float_val = 3.14
```

### Boolean Literals

```opus
let a = true
let b = false
let c = yes       // alias for true
let d = no        // alias for false
```

## Control Flow

### If / Else

No parentheses required around the condition:

```opus
if x > 0 {
    print("positive\n")
} else if x < 0 {
    print("negative\n")
} else {
    print("zero\n")
}
```

### While Loop

```opus
var i = 0
while i < 10 {
    print_dec(i)
    print("\n")
    i = i + 1
}
```

### For Loop with Range

```opus
// range(start, end) - iterates from start to end-1
for i in range(0, 10) {
    print_dec(i)
    print("\n")
}

// range(n) - shorthand for range(0, n)
for i in range(5) {
    print_dec(i)
    print("\n")
}

// bounds can be variables or expressions
let n = 100
for i in range(0, n) {
    // ...
}
```

### Infinite Loop

```opus
loop {
    if done {
        break
    }
}
```

### Break and Continue

```opus
for i in range(0, 100) {
    if i == 50 {
        break          // exit the loop
    }
    if i % 2 == 0 {
        continue       // skip to next iteration (alias: cont)
    }
    print_dec(i)
    print("\n")
}
```

## Operators

### Arithmetic
`+`, `-`, `*`, `/`, `%`

### Comparison
`==`, `!=`, `<`, `>`, `<=`, `>=`

### Logical
`&&` (or `and`), `||` (or `or`), `!` (or `not`)

### Bitwise
`&`, `|`, `^`, `~`, `<<`, `>>`

### Assignment
`=`, `+=`, `-=`, `*=`, `/=`

### Increment / Decrement
`++`, `--` (both prefix and postfix)

## Strings

Strings are null-terminated C strings. `print()` does not add a newline — use `\n`:

```opus
print("Hello, World!\n")
print("Tab:\tValue\n")
print("Line 1\nLine 2\n")
```

### Escape Sequences

| Sequence | Character |
|----------|-----------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |
| `\0` | Null byte |

### String Operations

```opus
let len = string_length("hello")              // 5
let joined = string_append("foo", "bar")      // "foobar" (heap allocated)
let eq = string_equals("a", "a")              // 1
let sub = string_substring("hello", 1, 3)     // "ell" (heap allocated)
let s = int_to_string(42)                     // "42" (heap allocated)
```

Heap-allocated strings from `string_append`, `string_substring`, and `int_to_string` must be freed by the caller.

## Comments

```opus
// single line comment

/* multi-line
   comment */
```

## Structs and Classes

Structs are plain data. Classes can have methods:

```opus
struct Point {
    x: int,
    y: int,
}

class Player {
    health: int,
    speed: int,

    function void takeDamage(int amount) {
        self.health = self.health - amount
    }

    function int getHealth() {
        return self.health
    }
}
```

Instances are heap-allocated:

```opus
var p: Player = malloc(16)
p.health = 100
p.speed = 50

p.takeDamage(30)
let hp = p.getHealth()

free(p)
```

Each field is 8 bytes. Methods compile to global functions (`Player_takeDamage`, `Player_getHealth`) with the instance pointer passed implicitly via `self`.

See [Classes & Structs](CLASSES.md) for the full reference.

## Enums

```opus
enum Direction {
    North,        // 0
    South,        // 1
    East,         // 2
    West = 10,    // 10
    Northwest,    // 11
}

function int main() {
    alloc_console()
    let dir = Direction.West
    print_dec(dir)
    print("\n")

    if dir == 10 {
        print("going west\n")
    }
    return 0
}
```

Values auto-increment from 0. Explicit values are supported, and subsequent variants continue from the last explicit value.

## Memory Model

Opus uses manual memory management. There is no garbage collector (planned for future versions with configurable modes).

### Allocation

```opus
// allocate raw bytes
let ptr = malloc(64)

// write and read 8-byte values
mem_write(ptr, 0xDEADBEEF)
let val = mem_read(ptr)

// sized reads/writes
mem_write_i32(ptr, 42)
let small = mem_read_i32(ptr)

// free when done
free(ptr)
```

### Arrays

Heap-allocated arrays with a 16-byte header (length + capacity). Elements are 8 bytes each:

```opus
let arr = array_new(10)
array_set(arr, 0, 42)
array_set(arr, 1, 100)

let v = arr[0]              // index syntax works for reads
let n = array_len(arr)      // 2

array_free(arr)
```

## Concurrency

### Spawn / Await

Fire off a function on a new thread and wait for the result:

```opus
function int compute(int x) {
    return x * x + 1
}

function int main() {
    let handle = spawn compute(7)
    print("thread spawned\n")

    let result = await handle
    print("result: ")
    print_dec(result)
    print("\n")
    return 0
}
```

### Parallel For

Split loop iterations across CPU cores automatically:

```opus
// allocate shared memory
let sum_ptr = malloc(8)
mem_write(sum_ptr, 0)

parallel for i in range(0, 100) {
    let val = mem_read(arr + i * 8)
    atomic_add(sum_ptr, val)
}

let total = mem_read(sum_ptr)
```

Use `atomic_add` for safe concurrent writes to shared memory.

## FFI (Windows API)

Opus can call Windows API functions directly:

```opus
// get a module handle
let kernel32 = get_module("kernel32.dll")

// resolve a function
let proc = get_proc(kernel32, "GetCurrentProcessId")

// call it
let pid = ffi_call0(proc)
print_dec(pid)
print("\n")

// or use the built-in shortcut
let pid2 = get_current_process_id()
```

Higher-level builtins like `msgbox`, `virtual_protect`, and `load_library` wrap common Win32 calls. See [Built-in Functions](BUILTINS.md) for the full list.

## Hex String Literals

For embedding raw bytes (useful for shellcode, patterns, signatures):

```opus
let pattern = hex"48 89 5C 24 08 57 48 83 EC 20"
```

This creates a byte buffer containing the literal hex values.

## Imports

Split code across files with `import`:

```opus
// math.op
function int square(int x) {
    return x * x
}
```

```opus
// main.op
import math

function int main() {
    alloc_console()
    let r = square(5)
    print_dec(r)
    print("\n")
    return 0
}
```

For larger projects, use an `opus.project` file to manage multiple source directories. See [Getting Started](getting-started.md#multi-file-projects).

## REPL

Start an interactive session:

```
> opus repl

   ____
  / __ \____  __  _______
 / / / / __ \/ / / / ___/
/ /_/ / /_/ / /_/ (__  )
\____/ .___/\__,_/____/
    /_/

A Simple Programming Language
===================================
```

Type expressions and statements to evaluate them immediately.

## Keyword Aliases

Opus is flexible about keywords. Many have short forms or alternative spellings:

| Primary | Aliases |
|---------|---------|
| `function` | `func`, `fn` |
| `return` | `ret` |
| `continue` | `cont` |
| `struct` | `structure` |
| `extern` | `external` |
| `malloc` | `alloc` |
| `true` | `yes` |
| `false` | `no` |
| `spawn` | — |
| `await` | — |
| `thread` | `async` |

## What's Next

- [Language Reference](REFERENCE.md) — complete syntax details
- [Built-in Functions](BUILTINS.md) — the full standard library
- [Classes & Structs](CLASSES.md) — OOP features
- [DLL Mode](DLL.md) — generating injectable DLLs
- [Debugger](DEBUGGER.md) — crash handler and self-healing runtime
