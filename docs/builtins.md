# Opus Built-in Functions

Complete reference for all built-in functions available in Opus. All functions work in both
JIT and EXE/DLL mode unless noted otherwise.

---

## Table of Contents

- [Console Output](#console-output)
- [Arrays](#arrays)
- [Strings](#strings)
- [Memory Operations](#memory-operations)
- [Mathematics](#mathematics)
- [Timing](#timing)
- [File I/O](#file-io)
- [Windows API (FFI)](#windows-api-ffi)
- [Concurrency](#concurrency)
- [Buffer Operations](#buffer-operations)
- [Utility](#utility)
- [Namespace Syntax](#namespace-syntax)

---

## Console Output

| Function | Description |
|----------|-------------|
| `print(str)` | Print string to stdout. No newline appended. |
| `println(str)` | Print string + newline. |
| `print_int(n)` | Print integer as decimal, followed by newline. |
| `print_dec(n)` | Alias for `print_int`. |
| `print_hex(n)` | Print integer as hex with `0x` prefix, followed by newline. |
| `print_char(ch)` | Print a single ASCII character. |
| `alloc_console()` | Allocate a console window. **Required at the start of `main()` for EXE programs.** |
| `set_title(str)` | Set the console window title. |

### Examples

```c
function int main() {
    alloc_console()

    print("hello ")          // no newline
    print("world\n")         // manual newline
    println("this has a newline automatically")

    print_int(42)            // prints "42\n"
    print_hex(255)           // prints "0xFF\n"
    print_char(65)           // prints "A"

    set_title("my opus program")
    return 0
}
```

> **Important:** `print()` does NOT add a newline. You must include `\n` in your string or
> use `println()` instead.

> **Important:** EXE programs must call `alloc_console()` at the start of `main()` or
> console output will go nowhere.

---

## Arrays

Heap-allocated dynamic arrays with a 16-byte header.

**Memory layout:**
- Offset 0: length (8 bytes) — tracks highest index written + 1
- Offset 8: capacity (8 bytes) — total slots allocated
- Offset 16+: elements — 8 bytes each

| Function | Description |
|----------|-------------|
| `array_new(capacity)` | Allocate a new array with the given capacity. Returns a pointer. |
| `array_get(arr, index)` | Read the element at `index`. |
| `array_set(arr, index, value)` | Write `value` at `index`. Updates length if needed. |
| `array_len(arr)` | Get the current length (max index written + 1). |
| `array_free(arr)` | Free the array and its backing memory. |

### Index Syntax

`arr[i]` is syntactic sugar for `array_get(arr, i)`.

### Examples

```c
function int main() {
    alloc_console()

    let arr = array_new(10)

    // fill it up
    for i in range(0, 10) {
        array_set(arr, i, i * i)
    }

    // read with index syntax
    print_int(arr[0])     // 0
    print_int(arr[3])     // 9
    print_int(arr[9])     // 81

    let len = array_len(arr)
    print_int(len)        // 10

    array_free(arr)
    return 0
}
```

```c
// building an array incrementally
function ptr collect_evens(int limit) {
    let result = array_new(limit / 2)
    var idx = 0
    for i in range(0, limit) {
        if i % 2 == 0 {
            array_set(result, idx, i)
            idx++
        }
    }
    return result
}
```

---

## Strings

Strings are null-terminated C strings. Functions that create new strings (append, substring,
int_to_string) allocate on the heap — the caller must `free()` them.

### Core Functions

| Function | Description |
|----------|-------------|
| `string_length(str)` | Length of a null-terminated string (not counting the null). |
| `string_append(a, b)` | Concatenate two strings. Returns a new heap-allocated string. |
| `string_equals(a, b)` | Compare two strings. Returns `1` if equal, `0` otherwise. |
| `string_substring(str, start, len)` | Extract `len` characters starting at `start`. Returns heap string. |
| `int_to_string(n)` | Convert integer to its decimal string representation. Returns heap string. |

### Legacy Aliases

These shorter names do the same thing:

| Alias | Maps To |
|-------|---------|
| `strlen(str)` | `string_length` |
| `concat(a, b)` | `string_append` |
| `streq(a, b)` | `string_equals` |
| `substr(str, start, len)` | `string_substring` |
| `itoa(n)` | `int_to_string` |
| `atoi(str)` | String to integer |

### Character Functions

| Function | Description |
|----------|-------------|
| `char_at(str, idx)` | Get the ASCII code of the character at `idx`. |
| `starts_with(str, prefix)` | Returns `1` if `str` starts with `prefix`. |
| `is_alpha(ch)` | Returns `1` if `ch` is a letter (A-Z, a-z). |
| `is_digit(ch)` | Returns `1` if `ch` is a digit (0-9). |
| `is_alnum(ch)` | Returns `1` if `ch` is a letter or digit. |
| `is_whitespace(ch)` | Returns `1` if `ch` is a space, tab, newline, etc. |

### Examples

```c
function int main() {
    alloc_console()

    let len = string_length("hello")
    print_int(len)                        // 5

    let full = string_append("foo", "bar")
    print(full)                           // foobar
    print("\n")
    free(full)

    let eq = string_equals("abc", "abc")
    print_int(eq)                         // 1

    let sub = string_substring("hello world", 6, 5)
    print(sub)                            // world
    print("\n")
    free(sub)

    let num_str = int_to_string(12345)
    print(num_str)                        // 12345
    print("\n")
    free(num_str)

    return 0
}
```

```c
// character inspection
function void analyze(str text) {
    let len = string_length(text)
    for i in range(0, len) {
        let ch = char_at(text, i)
        if is_digit(ch) {
            print("digit: ")
            print_char(ch)
            print("\n")
        }
    }
}
```

---

## Memory Operations

Low-level memory access. These map directly to x86 mov instructions of the appropriate size.

### Allocation

| Function | Description |
|----------|-------------|
| `malloc(size)` | Allocate `size` bytes. Returns pointer. |
| `alloc(size)` | Alias for `malloc`. |
| `free(ptr)` | Free previously allocated memory. |

### Read Functions

| Function | Reads |
|----------|-------|
| `mem_read(addr)` | 8 bytes (64-bit) |
| `mem_read_i32(addr)` | 4 bytes (32-bit) |
| `mem_read_i16(addr)` | 2 bytes (16-bit) |
| `mem_read_i8(addr)` | 1 byte (8-bit) |

### Write Functions

| Function | Writes |
|----------|--------|
| `mem_write(addr, val)` | 8 bytes (64-bit) |
| `mem_write_i32(addr, val)` | 4 bytes (32-bit) |
| `mem_write_i16(addr, val)` | 2 bytes (16-bit) |
| `mem_write_i8(addr, val)` | 1 byte (8-bit) |

### Bulk Operations

| Function | Description |
|----------|-------------|
| `memcpy(dst, src, len)` | Copy `len` bytes from `src` to `dst`. |
| `memset(ptr, val, len)` | Fill `len` bytes at `ptr` with `val`. |
| `memcmp(a, b, len)` | Compare `len` bytes. Returns `0` if equal. |

### PascalCase Namespace

All memory functions are also available under the `Mem` namespace:

```c
// these pairs are equivalent
mem_read(addr)          Mem.Read(addr)
mem_write(addr, val)    Mem.Write(addr, val)
mem_read_i32(addr)      Mem.ReadI32(addr)
mem_write_i8(addr, v)   Mem.WriteI8(addr, v)
```

### Examples

```c
function int main() {
    alloc_console()

    // allocate a small buffer
    let buf = malloc(64)

    // write different sizes
    mem_write(buf, 0xDEADBEEF)
    mem_write_i8(buf + 8, 42)

    // read them back
    let big = mem_read(buf)
    let small = mem_read_i8(buf + 8)
    print_hex(big)       // 0xDEADBEEF
    print_int(small)     // 42

    free(buf)
    return 0
}
```

```c
// zero out a buffer
function void clear_buffer(ptr buf, int size) {
    memset(buf, 0, size)
}

// copy a struct manually
function void clone_data(ptr dst, ptr src, int size) {
    memcpy(dst, src, size)
}

// compare two memory regions
function bool buffers_equal(ptr a, ptr b, int len) {
    return memcmp(a, b, len) == 0
}
```

---

## Mathematics

### Basic Math

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value. |
| `pow(base, exp)` | Integer exponentiation. |
| `sqrt(x)` | Integer square root. |
| `floor(x)` | Round down (for float values). |
| `ceil(x)` | Round up (for float values). |

### Trigonometry

| Function | Description |
|----------|-------------|
| `sin(rad)` | Sine of angle in radians. |
| `cos(rad)` | Cosine of angle in radians. |
| `tan(rad)` | Tangent of angle in radians. |
| `atan2(y, x)` | Two-argument arctangent. |

> **Note:** Trig functions take integer radians and return integer results (truncated).
> For precision work, scale your values (e.g., multiply by 1000 before passing, divide after).

### Examples

```c
function int main() {
    alloc_console()

    print_int(abs(-42))       // 42
    print_int(pow(2, 10))     // 1024
    print_int(sqrt(144))      // 12

    // distance calculation
    let dx = 30
    let dy = 40
    let dist = sqrt(dx * dx + dy * dy)
    print_int(dist)           // 50

    return 0
}
```

---

## Timing

| Function | Description |
|----------|-------------|
| `sleep(ms)` | Pause execution for `ms` milliseconds. |
| `get_tick_count()` | Milliseconds since system boot. Useful for benchmarking. |

### Examples

```c
function int main() {
    alloc_console()

    let start = get_tick_count()

    // do some work
    var sum = 0
    for i in range(0, 1000000) {
        sum += i
    }

    let elapsed = get_tick_count() - start
    print("took ")
    print_int(elapsed)
    print(" ms\n")

    return 0
}
```

```c
// simple frame limiter
function void wait_for_frame(int target_ms, int frame_start) {
    let elapsed = get_tick_count() - frame_start
    if elapsed < target_ms {
        sleep(target_ms - elapsed)
    }
}
```

---

## File I/O

| Function | Description |
|----------|-------------|
| `read_file(path)` | Read entire file contents as a string. |
| `write_file(path, content)` | Write a string to a file (overwrites). |
| `write_bytes(path, buffer, len)` | Write `len` raw bytes from `buffer` to a file. |

### Examples

```c
function int main() {
    alloc_console()

    // write a file
    write_file("output.txt", "hello from opus!\n")

    // read it back
    let contents = read_file("output.txt")
    print(contents)

    // write raw bytes
    let buf = malloc(4)
    mem_write_i8(buf, 0x48)
    mem_write_i8(buf + 1, 0x69)
    mem_write_i8(buf + 2, 0x21)
    mem_write_i8(buf + 3, 0x0A)
    write_bytes("raw.bin", buf, 4)
    free(buf)

    return 0
}
```

---

## Windows API (FFI)

Direct access to the Windows API through dynamic loading and function pointer calls.

### Module Loading

| Function | Description |
|----------|-------------|
| `get_module(name)` | Get handle to already-loaded module (`GetModuleHandleA`). |
| `load_library(path)` | Load a DLL (`LoadLibraryA`). Returns module handle. |
| `get_proc(module, name)` | Get function address from module (`GetProcAddress`). |

### Function Pointer Calls

| Function | Description |
|----------|-------------|
| `ffi_call0(fn)` | Call function pointer with 0 arguments. |
| `ffi_call1(fn, a)` | Call with 1 argument. |
| `ffi_call2(fn, a, b)` | Call with 2 arguments. |
| `ffi_call3(fn, a, b, c)` | Call with 3 arguments. |
| `ffi_call4(fn, a, b, c, d)` | Call with 4 arguments. |
| `ffi_call5(fn, a, b, c, d, e)` | Call with 5 arguments. |
| `ffi_call6(fn, a, b, c, d, e, f)` | Call with 6 arguments. |

### Convenience Wrappers

| Function | Description |
|----------|-------------|
| `msgbox(title, msg, flags)` | Show a `MessageBoxA` dialog. |
| `get_current_process_id()` | Returns the current process ID. |
| `virtual_protect(addr, size, prot)` | Change memory protection (`VirtualProtect`). |

### Examples

```c
function int main() {
    alloc_console()

    // show a message box
    msgbox("Opus", "Hello from Opus!", 0)

    // dynamic api call
    let user32 = load_library("user32.dll")
    let msg_beep = get_proc(user32, "MessageBeep")
    ffi_call1(msg_beep, 0x40)    // play asterisk sound

    // get our own pid
    let pid = get_current_process_id()
    print("pid: ")
    print_int(pid)

    return 0
}
```

```c
// make memory executable for jit-style usage
function void make_executable(ptr code, int size) {
    virtual_protect(code, size, 0x40)   // PAGE_EXECUTE_READWRITE
}
```

---

## Concurrency

Opus has built-in threading primitives.

### Thread Management

| Function | Description |
|----------|-------------|
| `spawn func(args)` | Launch `func` on a new thread. Returns a thread handle. |
| `await handle` | Block until the thread finishes. Returns the thread's result. |

### Atomics

| Function | Description |
|----------|-------------|
| `atomic_add(ptr, val)` | Lock-free atomic addition. Returns previous value. |
| `atomic_cas(ptr, expected, desired)` | Compare-and-swap. Returns `1` on success. |
| `atomic_load(ptr)` | Atomic read of 64-bit value. |
| `atomic_store(ptr, val)` | Atomic write of 64-bit value. |

### Examples

```c
function int worker(int id) {
    print("worker ")
    print_int(id)
    print(" running\n")
    sleep(100)
    return id * 10
}

function int main() {
    alloc_console()

    // spawn 4 threads
    let h1 = spawn worker(1)
    let h2 = spawn worker(2)
    let h3 = spawn worker(3)
    let h4 = spawn worker(4)

    // wait for all of them
    let r1 = await h1
    let r2 = await h2
    let r3 = await h3
    let r4 = await h4

    let total = r1 + r2 + r3 + r4
    print("total: ")
    print_int(total)    // 100

    return 0
}
```

```c
// shared counter with atomics
function int increment_counter(ptr counter) {
    for i in range(0, 1000) {
        atomic_add(counter, 1)
    }
    return 0
}

function int main() {
    alloc_console()

    let counter = malloc(8)
    mem_write(counter, 0)

    let h1 = spawn increment_counter(counter)
    let h2 = spawn increment_counter(counter)

    await h1
    await h2

    let final_val = atomic_load(counter)
    print_int(final_val)    // 2000 — no data races

    free(counter)
    return 0
}
```

---

## Buffer Operations

Simple byte buffer for building up binary data.

| Function | Description |
|----------|-------------|
| `buffer_new(capacity)` | Create a new byte buffer with the given capacity. |
| `buffer_push(buf, byte)` | Append a single byte to the buffer. |
| `buffer_len(buf)` | Get the current number of bytes in the buffer. |

### Examples

```c
function int main() {
    alloc_console()

    let buf = buffer_new(256)

    // build up some bytes
    buffer_push(buf, 0x48)
    buffer_push(buf, 0x65)
    buffer_push(buf, 0x6C)
    buffer_push(buf, 0x6C)
    buffer_push(buf, 0x6F)

    let len = buffer_len(buf)
    print_int(len)    // 5

    // write buffer to file
    write_bytes("out.bin", buf, len)

    return 0
}
```

---

## Utility

| Function | Description |
|----------|-------------|
| `exit(code)` | Immediately terminate the program with the given exit code. |
| `range(start, end)` | Create a range for use in `for` loops. Iterates `start` to `end - 1`. |

### Examples

```c
function int main() {
    alloc_console()

    for i in range(1, 11) {
        print_int(i)
    }
    // prints 1 through 10

    if something_bad() {
        print("fatal error\n")
        exit(1)
    }

    return 0
}
```

---

## Namespace Syntax

Most built-in categories support PascalCase namespace access as an alternative to snake_case.
Both styles compile to the same thing.

| Namespace Call | Equivalent |
|----------------|------------|
| `Mem.Read(addr)` | `mem_read(addr)` |
| `Mem.Write(addr, val)` | `mem_write(addr, val)` |
| `Mem.ReadI32(addr)` | `mem_read_i32(addr)` |
| `Mem.WriteI32(addr, val)` | `mem_write_i32(addr, val)` |
| `Mem.ReadI16(addr)` | `mem_read_i16(addr)` |
| `Mem.WriteI16(addr, val)` | `mem_write_i16(addr, val)` |
| `Mem.ReadI8(addr)` | `mem_read_i8(addr)` |
| `Mem.WriteI8(addr, val)` | `mem_write_i8(addr, val)` |
| `Array.New(cap)` | `array_new(cap)` |
| `Array.Get(arr, i)` | `array_get(arr, i)` |
| `Array.Set(arr, i, v)` | `array_set(arr, i, v)` |
| `Array.Len(arr)` | `array_len(arr)` |
| `Array.Free(arr)` | `array_free(arr)` |
| `String.Length(s)` | `string_length(s)` |
| `String.Append(a, b)` | `string_append(a, b)` |
| `String.Equals(a, b)` | `string_equals(a, b)` |

```c
function int main() {
    alloc_console()

    // namespace style
    let buf = Mem.Read(some_addr)
    Mem.Write(some_addr, 0)

    let arr = Array.New(10)
    Array.Set(arr, 0, 42)
    let val = Array.Get(arr, 0)
    Array.Free(arr)

    return 0
}
```
