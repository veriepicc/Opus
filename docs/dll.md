# DLL Mode Reference

Complete guide to compiling Opus programs as Windows DLLs.

---

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Console Output in DLL Mode](#console-output-in-dll-mode)
- [DLL vs EXE Mode](#dll-vs-exe-mode)
- [Project File Mode](#project-file-mode)
- [Debug Mode](#debug-mode)
- [Self-Healing](#self-healing)
- [File Size](#file-size)
- [Examples](#examples)

---

## Overview

Opus can compile any `.op` file to a Windows DLL with a single flag:

```
> opus --dll myapp.op
Generated DLL: myapp.dll (3072 bytes)
```

The generated DLL is a valid PE file with a `DllMain` entry point. On `DLL_PROCESS_ATTACH`,
it calls your `main()` function automatically. The DLL is completely standalone — no runtime
dependencies beyond `kernel32.dll`.

Key points:
- Works with `LoadLibrary` or any DLL injector
- No external linker or build tools needed
- Produces tiny binaries (2-10 KB typical)
- Full access to all Opus builtins: console, memory, FFI, concurrency, etc.

---

## How It Works

Opus generates a full PE DLL from scratch. The PE builder (`pe.ixx`) creates everything
in-memory and writes it out as a single file:

### PE Structure

| Section | Contents |
|---------|----------|
| DOS header | Standard MZ stub |
| PE headers | COFF + optional headers, section table |
| `.text` | Compiled x64 machine code |
| `.rdata` | String literals, constant data |
| `.idata` | Import Address Table (IAT) |
| `.src` | Source code (debug mode only) |
| `.srcmap` | Line map (debug mode only) |

### Import Table

The IAT links to `kernel32.dll` for all system functions:

| Function | Used For |
|----------|----------|
| `GetStdHandle` | Console output handle |
| `WriteConsoleA` | Writing text to console |
| `AllocConsole` | Creating console windows |
| `SetConsoleTitleA` | Setting console title |
| `GetProcessHeap` | Heap handle for allocation |
| `HeapAlloc` | Memory allocation (`malloc`) |
| `HeapFree` | Memory deallocation (`free`) |
| `ExitProcess` | Program termination |
| `CreateThread` | `spawn` / `parallel for` |
| `WaitForSingleObject` | `await` / thread join |
| `Sleep` | `sleep()` builtin |
| `GetTickCount64` | `get_tick_count()` timing |
| `VirtualProtect` | `virtual_protect()` FFI |
| `GetModuleHandleA` | `get_module()` FFI |
| `GetProcAddress` | `get_proc()` FFI |
| `LoadLibraryA` | `load_library()` FFI |

### Startup Sequence

**DLL mode:**
1. Windows calls `DllMain(hModule, reason, reserved)`
2. On `DLL_PROCESS_ATTACH`: save module base, call `main()`
3. Return `TRUE` to the loader

**EXE mode:**
1. PE entry point runs startup code
2. Calls `main()` directly
3. Calls `ExitProcess` with the return value

---

## Console Output in DLL Mode

Since DLLs run inside a host process that may not have a console, you need to create one:

```opus
function int main() {
    // create a console window attached to the host process
    alloc_console()
    
    // optionally set the window title
    set_title("My DLL Tool")
    
    // now print works
    print("DLL loaded!\n")
    print_int(42)
    print_hex(0xDEAD)
    
    return 0
}
```

All output functions route through the IAT:
- `print(str)` — string output, no automatic newline
- `println(str)` — string output with newline
- `print_int(n)` / `print_dec(n)` — integer as decimal + newline
- `print_hex(n)` — integer as hex with `0x` prefix + newline
- `print_char(ch)` — single ASCII character

---

## DLL vs EXE Mode

| Feature | DLL Mode (`--dll`) | EXE Mode (default) |
|---------|-------------------|---------------------|
| Entry point | `DllMain` | exe startup stub |
| Console | `alloc_console()` creates new | `alloc_console()` creates new |
| Output format | `.dll` | `.exe` |
| Can be injected | Yes | No |
| Can be LoadLibrary'd | Yes | No |
| Standalone | Yes | Yes |
| Runtime deps | `kernel32.dll` only | `kernel32.dll` only |
| All builtins available | Yes | Yes |

Both modes produce completely standalone PE files. The only difference is the entry point
mechanism and the PE characteristics flag.

---

## Project File Mode

For multi-file DLL projects, use an `opus.project` file:

```opus
project MyApp {
    entry: "main.op"
    output: "myapp.dll"
    mode: dll
    debug: true
    healing: "auto"
    include: ["src/", "lib/"]
}
```

Build with:

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

### Project File Fields

| Field | Description | Default |
|-------|-------------|---------|
| `entry` | Main source file | required |
| `output` | Output filename | `<name>.<ext>` |
| `mode` | `dll` or `exe` | `dll` |
| `debug` | Embed source and line maps | `false` |
| `healing` | Crash recovery: `"auto"`, `"freeze"`, `"off"` | based on `debug` |
| `include` | Directories to scan for `.op` files | `[]` |

---

## Debug Mode

When `debug: true` is set in the project file (or will be available as a CLI flag):

### What Gets Embedded

- **`.src` section** — full source code is embedded in the PE as a new section
- **`.srcmap` section** — a line map (instruction offset → source line number) is embedded

### What This Enables

- Crash handler can show the exact source line that crashed
- Source context (surrounding lines) is displayed on crash
- Stack traces include source locations instead of just raw addresses

```opus
project DebugApp {
    entry: "main.op"
    output: "debug_app.dll"
    mode: dll
    debug: true
}
```

The debug sections add the size of your source code plus the line map to the final binary.
For a 500-line program, expect roughly 10-15 KB of extra data.

---

## Self-Healing

Opus includes a VEH-based (Vectored Exception Handler) crash recovery system. Three modes
are available:

### `"auto"` — Auto-Recovery

```opus
project MyApp {
    healing: "auto"
}
```

- Detects crash patterns (access violations, illegal instructions, etc.)
- Attempts to apply a fix automatically (skip the faulting instruction, patch the call, etc.)
- Logs the crash and the fix applied
- Keeps the program running

### `"freeze"` — Freeze on Crash

```opus
project MyApp {
    healing: "freeze"
}
```

- On crash, freezes execution at the crash point
- Shows source context around the crash (requires `debug: true`)
- Displays register state and crash info
- This is the default when `debug: true` is set

### `"off"` — Normal Crash Behavior

```opus
project MyApp {
    healing: "off"
}
```

- No crash interception
- Program terminates normally on unhandled exceptions
- Smallest overhead

---

## File Size

Opus DLLs are tiny because there is no runtime bloat:

| Program | Approximate Size |
|---------|:----------------:|
| Minimal (empty main) | ~2.5 KB |
| Console hello world | ~3 KB |
| Medium program (100-200 lines) | 4-6 KB |
| Large program with FFI | 6-10 KB |
| With debug info (500 lines) | +10-15 KB |

Compare that to a minimal C++ DLL compiled with MSVC (~50+ KB) or a Rust DLL (~150+ KB).

---

## Examples

### Minimal DLL

```opus
function int main() {
    alloc_console()
    print("hello from dll\n")
    return 0
}
```

```
> opus --dll minimal.op
Generated DLL: minimal.dll (2560 bytes)
```

### Full Application DLL

```opus
function int main() {
    alloc_console()
    set_title("Opus Scanner")
    
    print("=== Opus Scanner v1.0 ===\n")
    
    // get the host process module base
    let base = get_module(0)
    print("module base: ")
    print_hex(base)
    
    // get our process id
    let pid = get_current_process_id()
    print("process id: ")
    print_int(pid)
    
    // read PE header to get image size
    let e_lfanew = mem_read_i32(base + 0x3C)
    let image_size = mem_read_i32(base + e_lfanew + 0x50)
    print("image size: ")
    print_hex(image_size)
    
    // scan for a byte pattern
    let result = scan(base, image_size, "48 89 5C 24 ? 55 56 57")
    if result != 0 {
        print("pattern found at: ")
        print_hex(result)
    } else {
        print("pattern not found\n")
    }
    
    return 0
}
```

### DLL with Threading

```opus
function int worker(int id) {
    print("worker ")
    print_int(id)
    print(" started\n")
    sleep(500)
    print("worker ")
    print_int(id)
    print(" done\n")
    return id * 100
}

function int main() {
    alloc_console()
    set_title("Threaded DLL")
    
    print("spawning workers...\n")
    
    let h1 = spawn worker(1)
    let h2 = spawn worker(2)
    let h3 = spawn worker(3)
    
    let r1 = await h1
    let r2 = await h2
    let r3 = await h3
    
    print("total: ")
    print_int(r1 + r2 + r3)
    
    return 0
}
```

### DLL with FFI

```opus
function int main() {
    alloc_console()
    set_title("FFI Demo")
    
    // load user32.dll and call MessageBoxA
    let user32 = load_library("user32.dll")
    let msgbox_fn = get_proc(user32, "MessageBoxA")
    
    // MessageBoxA(hwnd, text, caption, flags)
    ffi_call4(msgbox_fn, 0, "Hello from Opus DLL!", "Opus", 0x40)
    
    // get a function from ntdll
    let ntdll = get_module("ntdll.dll")
    let nt_query = get_proc(ntdll, "NtQuerySystemInformation")
    print("NtQuerySystemInformation: ")
    print_hex(nt_query)
    
    return 0
}
```

### DLL with Debug Project

```opus
project Scanner {
    entry: "scanner.op"
    output: "scanner.dll"
    mode: dll
    debug: true
    healing: "freeze"
    include: ["src/"]
}
```

```opus
// scanner.op
import utils

function int main() {
    alloc_console()
    set_title("Debug Scanner")
    
    print("scanner loaded with debug info\n")
    print("crash handler active (freeze mode)\n")
    
    // if this crashes, youll see the exact source line
    let base = get_module(0)
    let data = mem_read(base + 0x1000)
    print_hex(data)
    
    return 0
}
```

---

## See Also

- [Getting Started](getting-started.md) — CLI reference and compilation modes
- [Built-in Functions](builtins.md) — all available builtins
- [Debugger & Self-Healing](debugger.md) — crash handler details
- [Examples](examples.md) — more code examples
