# Opus

A systems programming language that compiles directly to native x64 machine code. No LLVM, no VM, no runtime — just raw machine code in standalone Windows executables and injectable DLLs.

```c
function int main() {
    alloc_console()
    print("Hello, World!\n")
    return 0
}
```

```
> opus hello.op
Generated EXE: hello.exe (3072 bytes)
```

## Features

- Compiles to native x64 — no intermediate bytecode, no interpreter
- Standalone EXE or injectable DLL output, one flag to switch
- JIT mode (`--run`) for instant compile-and-execute
- Flexible syntax — C-style, Rust-style, and English can coexist in one file
- Classes with methods, structs, enums
- `spawn`/`await` threading and `parallel for` with automatic core splitting
- Direct Windows API access through FFI
- Built-in pattern scanner, memory operations, atomics
- Self-healing crash handler with source context
- ~7ms compile times
- REPL for interactive experimentation

## Quick Start

```
> opus hello.op              # compile to EXE
> opus --dll scanner.op      # compile to DLL
> opus --run fibonacci.op    # compile and run immediately
```

## Building

Requires Visual Studio 2022 (v17.12+) with C++23 support.

```
MSBuild.exe Opus.sln /p:Configuration=Release /p:Platform=x64
```

Output: `bin/Release/opus.exe`

## Project Structure

```
Opus/
├── src/                # compiler source (C++20 modules)
│   ├── main.ixx        # entry point and CLI
│   ├── opus.ixx        # compiler API and runtime builtins
│   ├── lexer.ixx       # tokenizer
│   ├── parser.ixx      # recursive descent parser
│   ├── ast.ixx         # abstract syntax tree
│   ├── codegen.ixx     # x64 code generator
│   ├── x64.ixx         # x64 instruction emitter
│   ├── pe.ixx          # PE executable/DLL builder
│   ├── project.ixx     # opus.project build system
│   ├── types.ixx       # type system
│   └── errors.ixx      # error reporting
├── examples/           # example programs
├── tests/              # regression tests
├── selfhost/           # self-hosting compiler (written in Opus)
├── docs/               # documentation
└── vscode-extension/   # syntax highlighting
```

## Documentation

| Page | Description |
|------|-------------|
| [Getting Started](docs/getting-started.md) | Build the compiler, first program, CLI reference |
| [Language Overview](docs/language-overview.md) | Syntax styles, type system, memory model |
| [Language Reference](docs/reference.md) | Complete syntax reference |
| [Built-in Functions](docs/builtins.md) | Standard library — I/O, strings, arrays, memory, math, FFI |
| [Classes & Structs](docs/classes.md) | Classes, structs, methods, enums |
| [DLL Mode](docs/dll.md) | Generating injectable Windows DLLs |
| [Concurrency](docs/concurrency.md) | Threading, parallel for, atomics |
| [Debugger](docs/debugger.md) | Crash handler and self-healing runtime |
| [Examples](docs/examples.md) | Code examples by topic |

## License

MIT
