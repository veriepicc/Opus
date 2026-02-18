# Debugger & Self-Healing Runtime

Opus includes a built-in crash handler and self-healing system. No external debugger needed — the runtime catches crashes, shows you exactly what happened, and can even fix common issues automatically.

---

## Table of Contents

- [Overview](#overview)
- [Crash Handler](#crash-handler)
- [Source Context](#source-context)
- [Healing Modes](#healing-modes)
- [Auto-Recovery](#auto-recovery)
- [Debug Sections](#debug-sections)
- [Configuration](#configuration)
- [Roadmap](#roadmap)

---

## Overview

The debugger is built on Windows Vectored Exception Handling (VEH). When your program crashes, Opus intercepts the exception before Windows kills the process. Depending on your healing mode, it can:

- Show the exact source line that crashed
- Display surrounding code context
- Freeze execution for inspection
- Automatically fix common crash patterns and keep running

This works in both DLL and EXE mode. The crash handler is always present when `debug: true` is set in your project file.

---

## Crash Handler

The VEH handler catches all hardware exceptions:

| Exception | Description |
|-----------|-------------|
| `ACCESS_VIOLATION` | Read or write to invalid memory |
| `STACK_OVERFLOW` | Ran out of stack space |
| `INTEGER_DIVIDE_BY_ZERO` | Division by zero |
| `ILLEGAL_INSTRUCTION` | Invalid opcode executed |
| `BREAKPOINT` | INT3 hit (used internally) |

When a crash occurs, the handler:

1. Identifies the exception type
2. Looks up the faulting instruction in the source map
3. Displays the crash report with source context
4. Takes action based on the healing mode

### Crash Report

```
=== OPUS CRASH DETECTED ===
  Exception: ACCESS_VIOLATION (read of 0x0000000000000000)
  Location: line 47

  -------------------------------------------
  45 |     let player = scan(base, size, sig)
  46 |     let health_ptr = player + 0x180
> 47 |     let health = mem_read(health_ptr)
  48 |     print_dec(health)
  49 |     print("\n")
  -------------------------------------------
```

For access violations, the report includes whether it was a read or write and the target address. For null pointer access specifically, it flags the likely cause.

---

## Source Context

When `debug: true` is enabled, the full source code and a line map are embedded in the binary. On crash, the handler uses the instruction pointer to look up the source line and displays ±2 lines of context around the crash site.

The source line lookup works by:

1. Computing the offset of the faulting instruction from the code base
2. Searching the `.srcmap` section for the nearest line number
3. Reading the corresponding source from the `.src` section

This means you see your actual Opus code in the crash report, not raw addresses.

---

## Healing Modes

Three modes control what happens after a crash is detected.

### `"freeze"` — Freeze on Crash

The default when `debug: true` is set. Freezes execution at the crash point so you can inspect the state.

```c
project MyApp {
    debug: true
    healing: "freeze"
}
```

- Shows full crash report with source context
- Process stays alive (infinite loop at crash site)
- You can attach an external debugger if needed
- Useful during development

### `"auto"` — Auto-Recovery

Attempts to fix the crash and keep running. Best for production DLLs where you want resilience.

```c
project MyApp {
    healing: "auto"
}
```

- Detects the crash pattern
- Applies an automatic fix (see [Auto-Recovery](#auto-recovery))
- Logs the crash and fix
- Resumes execution

### `"off"` — No Crash Handling

Standard behavior. The process terminates on unhandled exceptions.

```c
project MyApp {
    healing: "off"
}
```

- No VEH handler installed
- Smallest overhead
- Normal Windows crash behavior

---

## Auto-Recovery

When healing is set to `"auto"`, the runtime applies fixes based on the crash pattern:

### Null Pointer Read

```
[SELF-HEAL] line 47: mem_read(0x0) → null pointer
[FIX] Result set to 0 (default value)
[NOTE] Variable was NULL - check initialization
Continuing execution...
```

Strategy: set the destination register to 0 in the CONTEXT struct, advance RIP past the faulting instruction.

### Null Pointer Write

```
[SELF-HEAL] line 52: mem_write(0x0, 100) → null pointer write
[FIX] Skipped write operation
Continuing execution...
```

Strategy: skip the faulting instruction entirely by advancing RIP.

### Division by Zero

```
[SELF-HEAL] line 30: x / 0 → division by zero
[FIX] Result set to 0
Continuing execution...
```

Strategy: set RAX to 0 (quotient), RDX to 0 (remainder), advance RIP past the DIV/IDIV instruction.

### Invalid Memory Access (non-null)

```
[SELF-HEAL] line 47: mem_read(0xDEAD) → access violation
[FIX] Result set to 0, address may be invalid or freed
Continuing execution...
```

Strategy: same as null read — set result to 0, advance RIP.

### Healing Log

Every auto-fix is logged. At the end of execution (or on demand), the healing log summarizes what happened:

```
=== OPUS HEALING LOG ===
[1] line 47: null read → returned 0 (3 occurrences)
[2] line 52: null write → skipped (1 occurrence)
[3] line 30: div by zero → returned 0 (1 occurrence)
Total: 5 auto-heals, 0 unrecoverable
```

---

## Debug Sections

When `debug: true` is set, two extra PE sections are added to the binary:

### `.src` Section

Contains the full source code of your program, embedded as raw text. This is what the crash handler reads to display source context.

### `.srcmap` Section

Contains a line map — a table mapping instruction offsets to source line numbers. Format is a flat array of `(offset, line)` pairs.

### Size Impact

| Program Size | Debug Overhead |
|-------------|:-------------:|
| 100 lines | ~2-3 KB |
| 500 lines | ~10-15 KB |
| 1000 lines | ~20-30 KB |

The overhead is roughly the size of your source code plus the line map. For most programs this is negligible.

---

## Configuration

### Project File

```c
project MyApp {
    entry: "main.op"
    output: "myapp.dll"
    mode: dll
    debug: true
    healing: "auto"
}
```

### Healing Mode Reference

| Mode | Behavior | Default When |
|------|----------|-------------|
| `"freeze"` | Freeze at crash, show source | `debug: true` (default) |
| `"auto"` | Auto-fix and continue | Must be set explicitly |
| `"off"` | Normal crash, process dies | `debug: false` |

### Example: Debug DLL with Auto-Healing

```c
project Scanner {
    entry: "scanner.op"
    output: "scanner.dll"
    mode: dll
    debug: true
    healing: "auto"
    include: ["src/"]
}
```

```c
// scanner.op
function int main() {
    alloc_console()
    set_title("Scanner")

    let base = get_module(0)
    let result = scan(base, 0x1000000, "48 89 5C 24 ? 55")

    // if scan returns 0, this would normally crash
    // with auto-healing, mem_read returns 0 instead
    let value = mem_read(result + 0x100)
    print_hex(value)

    return 0
}
```

---

## Roadmap

Features planned for future versions of the debugger:

### Rich Crash Context
- Full register dump (RAX through R15, RIP, RFLAGS)
- Stack trace via RBP chain walking
- Execution trace ring buffer (last N lines executed before crash)

### REPL Debugger
- Interactive console at crash point
- Evaluate expressions, inspect memory, modify variables
- Commands: `regs`, `bt`, `mem`, `break`, `watch`, `continue`, `skip`

### Hot Patching
- Recompile individual lines at runtime
- Patch code in-place or via trampolines
- NOP sledding for smaller replacements

### Smart Healing
- Context-aware fix suggestions ("scan() returned NULL — pattern may be outdated")
- Pointer lifetime tracking ("object was freed between ticks")
- Repeated crash detection ("same line crashing with different addresses — possible iterator invalidation")

---

## See Also

- [DLL Mode](dll.md) — debug mode in DLL projects
- [Built-in Functions](builtins.md) — memory and FFI builtins
- [Getting Started](getting-started.md) — project file configuration
