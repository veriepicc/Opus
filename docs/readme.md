# Opus Programming Language

**A systems programming language that compiles to native x64 machine code.**

## Quick Start

```opus
// hello.op
function int main() {
    alloc_console()
    print("Hello, World!\n")
    return 0
}
```

Compile and run:

```
> opus hello.op          # produces hello.exe
> opus --run hello.op    # compile + run in one step
> opus --dll hello.op    # produces hello.dll
```

## Features

- Native x64 code generation — no LLVM, no VM
- Standalone EXE and injectable DLL output
- JIT mode (`--run`) for instant execution
- Flexible syntax — C-style, Rust-style, English, all in one file
- Classes with methods, enums, structs
- PascalCase namespace builtins (`Mem.Read()`, `Array.New()`)
- Concurrency — `spawn`/`await`, `parallel for`, atomics
- FFI — direct Windows API access
- Self-healing crash handler with source context
- ~7ms compile times
- REPL for interactive experimentation

## Documentation

| Page | Description |
|------|-------------|
| [Getting Started](getting-started.md) | Build the compiler, write your first program, CLI reference |
| [Language Overview](language-overview.md) | Syntax styles, type system, memory model |
| [Language Reference](reference.md) | Complete syntax reference |
| [Built-in Functions](builtins.md) | Standard library — I/O, strings, arrays, memory, math, FFI |
| [Classes & Structs](classes.md) | Classes, structs, methods, enums |
| [DLL Mode](dll.md) | Generating injectable Windows DLLs |
| [Concurrency](concurrency.md) | Threading, parallel for, atomics |
| [Debugger & Self-Healing](debugger.md) | Crash handler, source maps, auto-recovery |
| [Examples](examples.md) | Code examples organized by topic |

## License

MIT
