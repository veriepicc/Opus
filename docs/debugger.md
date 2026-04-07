# Debugger and Self-Healing

Opus can embed source text, line mappings, and crash-handling support into native images so faults are easier to inspect after launch.

## What it does

With project debug mode enabled, native images can:

- intercept structured exceptions through VEH
- recover the source location of the faulting instruction
- display local source context from embedded program text
- choose one of three post-fault policies:
  freeze
  auto-heal
  off

## Project settings

```c
project MyApp {
    entry: "main.op"
    output: "MyApp.dll"
    mode: "dll"
    debug: true
    healing: "auto"
}
```

Healing modes:

| Mode | Meaning |
|------|---------|
| `"freeze"` | report the crash and stop at the fault site |
| `"auto"` | attempt a constrained recovery and continue |
| `"off"` | disable the handler |

## Crash report shape

```text
=== OPUS CRASH DETECTED ===
  Exception: ACCESS_VIOLATION
  Location: line 47

  -------------------------------------------
  45 |     let base = get_module(0)
  46 |     let slot = base + 0x180
> 47 |     let value = mem.read(slot)
  48 |     print_hex(value)
  49 |     return 0
  -------------------------------------------
```

## Embedded debug sections

With `debug: true`, native images can embed:

- `.src` with source text
- `.srcmap` with instruction-to-line mappings

That is how the runtime can point back to real Opus code after code generation is complete and the binary is already running.

## Auto-heal behavior

In `healing: "auto"` mode, the runtime can attempt narrow repairs such as:

- returning `0` for invalid reads
- skipping invalid writes
- neutralizing divide-by-zero results

This is intentionally conservative. The runtime is not trying to claim arbitrary crash recovery.

## Example

```c
import mem

function int main() {
    alloc_console()
    set_title("debug sample")

    let base = get_module(0)
    let slot = base + 0x100
    let value = mem.read(slot)
    print_hex(value)
    return 0
}
```

This example is DLL-oriented, so `alloc_console()` is intentional there.

## Operational notes

- debug/self-healing is part of the native-image path, not a separate execution mode
- it is already exercised by the integration suite
- it is intended to make native behavior easier to inspect and reason about
- it is most useful for tooling-style binaries, probes, scanners, and DLL utilities

## Deeper Interpretation

What makes this subsystem unusual is not that it prints a nicer crash message. What makes it unusual is that the generated native image retains enough structural knowledge of its own origin to interpret machine-level failure in source terms after launch.

The binary is not merely executable output. In debug-enabled native-image mode, it also carries:

- embedded source text
- instruction-to-line mappings
- an exception interception path
- a policy layer for post-fault response

That allows the image to do something relatively uncommon even in much larger toolchains: treat a live native fault as a source-level event, and in constrained cases continue execution instead of terminating immediately.

The important claim here is narrow but strong:

- not that every crash is safe
- not that arbitrary corruption can be healed
- but that some failure classes are sufficiently local and deterministic that the runtime can preserve observability, and sometimes preserve liveness, after the fault occurs

For scanners, probes, injected DLL tools, and “stay alive long enough to explain what just happened” workflows, that is a meaningful capability rather than a cosmetic one.

## Bottom line

The debugger and healing runtime make Opus more than a language that emits native bytes. In debug-enabled native images, the generated binary retains enough knowledge of its origin to participate in its own diagnosis under failure, and in limited cases, its own survival.
