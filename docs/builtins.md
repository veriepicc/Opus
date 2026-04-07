# Builtins

This page covers the low-level builtins that are part of the runtime/compiler surface.

For new code, prefer the standard-library modules when a module surface exists. In particular:

- prefer `import mem` over legacy raw memory helper names
- prefer module helpers like `text`, `fmt`, `fs`, `json`, `path`, `process`, `rand`, `time`, `vec`, and `algo` for day-to-day code

## Output and console

| Builtin | Meaning |
|--------|---------|
| `print(str)` | print a string without a newline |
| `println(str)` | print a string and newline |
| `print_int(n)` | print an integer line |
| `print_hex(n)` | print a hex line |
| `print_char(ch)` | print one character |
| `alloc_console()` | allocate/attach a console explicitly |
| `set_title(str)` | set the console title |

Important:

- normal EXE output already has a console
- `alloc_console()` is mostly for DLL tools or detached contexts

## Strings

| Builtin | Meaning |
|--------|---------|
| `string_length(str)` | length |
| `string_append(a, b)` | returns a new heap string |
| `string_equals(a, b)` | string equality |
| `string_substring(str, start, len)` | returns a new heap string |
| `int_to_string(n)` | returns a new heap string |
| `char_at(str, index)` | character lookup |
| `starts_with(str, prefix)` | prefix test |
| `is_alpha(ch)` | alpha test |
| `is_digit(ch)` | digit test |
| `is_alnum(ch)` | alnum test |
| `is_whitespace(ch)` | whitespace test |

If a builtin returns a fresh heap string, free it when you are done.

## Arrays

| Builtin | Meaning |
|--------|---------|
| `array_new(cap)` | allocate an array buffer |
| `array_get(arr, index)` | load element |
| `array_set(arr, index, value)` | store element |
| `array_len(arr)` | current length |
| `array_free(arr)` | free array storage |

## Memory and raw interop

Low-level allocation:

| Builtin | Meaning |
|--------|---------|
| `malloc(size)` | allocate bytes |
| `free(ptr)` | free bytes |
| `memcpy(dst, src, size)` | copy memory |
| `memset(dst, byte, size)` | fill memory |
| `memcmp(a, b, size)` | compare memory |

Recommended memory surface for new code:

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

## FFI and process helpers

| Builtin | Meaning |
|--------|---------|
| `get_module(name)` | get module handle |
| `load_library(name)` | load DLL |
| `get_proc(module, name)` | resolve symbol |
| `get_current_process_id()` | current PID |
| `virtual_protect(...)` | protection changes |

Typed usage usually looks like:

```c
using GetPidFn = fn() -> int

function int main() {
    let kernel32 = get_module("kernel32.dll")
    let get_pid = get_proc(kernel32, "GetCurrentProcessId") as GetPidFn
    print_int(get_pid())
    return 0
}
```

## Concurrency

| Builtin | Meaning |
|--------|---------|
| `spawn work(args)` | start a thread from a call |
| `await handle` | wait for a spawned thread |
| `parallel for i in range(a, b)` | parallel loop |
| `atomic_load(ptr)` | atomic load |
| `atomic_store(ptr, value)` | atomic store |
| `atomic_add(ptr, value)` | atomic add |
| `atomic_cas(ptr, expected, desired)` | compare-and-swap |

## Timing

| Builtin | Meaning |
|--------|---------|
| `sleep(ms)` | sleep current thread |
| `get_tick_count()` | tick count in milliseconds |

## Recommended rule of thumb

- Use builtins directly for core runtime operations.
- Use stdlib modules for nicer surfaces.
- Use `alloc_console()` only when you truly need to create/attach a console yourself.
