# Opus Programming Language

**A systems programming language that compiles to native x64 machine code.**

Opus outputs standalone Windows executables and injectable DLLs - no runtime, no VM, no dependencies. The compiler itself is written in C++20 modules and ships as a single binary.

```c
function int main() {
    alloc_console()
    print("Hello from Opus!\n")
    return 0
}
```

```
> opus hello.op
Generated EXE: hello.exe (3072 bytes)
```

## Feature Highlights

- **Native code generation** - compiles directly to x64 machine code, no LLVM, no intermediate bytecode
- **Fast compilation** - ~7ms compile times for typical programs
- **Flexible syntax** - C-style, Rust-style, and English syntax can all coexist in the same file
- **Two output modes** - standalone `.exe` or injectable `.dll`, one flag to switch
- **JIT mode** - `--run` compiles and executes immediately, no output file
- **Classes with methods** - `self` access, field assignment, method dispatch
- **Enums** - auto-incrementing or explicit values, dot-access syntax
- **Imports** - multi-file projects with `import` and `opus.project` build files
- **Concurrency** - `parallel for` splits work across cores, `spawn`/`await` for threading, atomic operations
- **Typed FFI** - call raw function pointers through `using Name = fn(...) -> ...`, `extern fn`, and normal calls
- **Clean memory surface** - `mem.read`, `mem.write32`, `mem.text`, and friends replace the old `mem_write_i32` style
- **Self-healing crash handler** - VEH-based crash detection with source context, optional auto-recovery
- **REPL** - interactive mode for quick experimentation
- **Built-in standard library** - strings, arrays, math, memory, file I/O, timing, and more

## Quick Start

Write a program:

```c
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
> hello.exe              # runs it

> opus --run hello.op    # or compile + run in one step
```

Compile as a DLL instead:

```
> opus --dll hello.op    # produces hello.dll
```

## Documentation

| Page | Description |
|------|-------------|
| [Getting Started](getting-started.md) | Installation, building the compiler, first program, CLI reference |
| [Language Overview](language-overview.md) | Syntax styles, type system, memory model, language design |
| [Language Reference](reference.md) | Complete syntax reference - types, operators, control flow, literals |
| [Built-in Functions](builtins.md) | Standard library - I/O, strings, arrays, memory, math, FFI |
| [Classes & Structs](classes.md) | Classes, structs, methods, enums, field access |
| [DLL Mode](dll.md) | Generating injectable Windows DLLs |
| [Debugger & Self-Healing](debugger.md) | Crash handler, source maps, self-healing runtime |
| [Examples](examples.md) | Code examples and patterns |

## Project Structure

```
Opus/
|- src/
|  |- main.ixx       # entry point and CLI
|  |- opus.ixx       # compiler API and runtime builtins
|  |- lexer.ixx      # tokenizer (mixed syntax support)
|  |- parser.ixx     # recursive descent parser
|  |- ast.ixx        # abstract syntax tree nodes
|  |- codegen.ixx    # x64 code generator
|  |- x64.ixx        # x64 instruction emitter
|  |- pe.ixx         # PE executable/DLL generator
|  |- project.ixx    # opus.project build system
|  |- types.ixx      # type system
|  `- errors.ixx     # error reporting
|- examples/         # example programs
|- tests/            # test suite
|- selfhost/         # self-hosting compiler (written in Opus)
|- docs/             # documentation (you are here)
`- vscode-extension/ # syntax highlighting for VS Code
```

## Compiler Version

v0.1.0

---

*Opus is MIT licensed.*
