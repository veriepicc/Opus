# Opus

Opus is a systems programming language that compiles directly to native Windows x64 machine code.

No LLVM, no VM, no bytecode, and no dependency on a giant external toolchain at compile time. Opus emits PE executables and DLLs directly.

## Status

This is `v1.0`.

Opus is real, usable, and already capable of native EXE and DLL output, but it is still early. It is not meant for serious production use yet. Expect rough edges, changing internals, and missing polish in some parts of the language and toolchain.

Current target:
- Windows only
- x64 only

If you want to experiment with it, improve it, or clean things up, contributions are welcome.

## Example

```c
function int main() {
    alloc_console()
    print("Hello, World!\n")
    return 0
}
```

```text
> opus hello.op
Generated EXE: hello.exe
```

## Highlights

- Native Windows x64 code generation
- Direct EXE and DLL output
- `--run` mode for compile-and-execute
- Typed FFI with normal callable function aliases
- Built-in memory helpers, pattern scanning, and low-level utilities
- Classes, structs, enums, methods, and multiple syntax styles
- Built-in threading and parallel work primitives
- Debug mode with source mapping and crash context
- Embedded crash handling / self-healing runtime support
- Embedded stdlib fallback in the compiler

## Why Opus?

| Feature | Typical MSVC Workflow | Opus |
|------|------|------|
| Compile Time | **~995-1646ms** | **~7-26ms** |
| Binary Size | Often larger with toolchain/runtime baggage | **Small standalone outputs** |
| Dependencies | Toolchain + CRT expectations | **Direct native output** |
| Crash Context | Usually external debugger first | **Embedded debug/crash context** |

Opus is built for low-level native development where iteration speed and control matter:

- fast compile times make tight edit-build-run loops practical
- direct machine-code generation keeps the output small and understandable
- embedded crash handling helps when a debugger is not the first tool you want to reach for
- built-in threading and parallel features make it usable beyond tiny toy programs
- typed FFI and low-level memory utilities make native interop straightforward

Current benchmark results against MSVC show:

- Opus compile times on the tested suite landed between **7.307ms and 26.298ms**
- MSVC compile times on the same suite landed between **994.662ms and 1645.524ms**
- Opus won runtime on `benchmark_math`, `benchmark_state`, and `benchmark_branch`
- Measured trimmed averages:
  - `benchmark_math`: **2.659ms** vs **7.676ms**
  - `benchmark_state`: **2.459ms** vs **6.758ms**
  - `benchmark_branch`: **2.220ms** vs **10.673ms**
- On broader workloads, Opus remained competitive:
  - `benchmark_general`: **35.157ms** vs **31.553ms**
  - `benchmark_text`: **16.821ms** vs **14.018ms**

## Quick Start

```text
> opus hello.op
> opus --dll client.op
> opus --run test.op
```

## Building

Requirements:
- Windows
- Visual Studio 2022
- MSVC with modern C++ support

Build:

```text
MSBuild.exe Opus.sln /p:Configuration=Release /p:Platform=x64
```

Output:

```text
bin/Release/opus.exe
```

## Project Layout

```text
Opus/
|-- src/                compiler source
|-- stdlib/             bundled standard library source
|-- docs/               language and toolchain docs
|-- examples/           small example programs
|-- tests/              regression tests
|-- selfhost/           self-hosting compiler work
`-- vscode-extension/   syntax highlighting
```

## Documentation

- [Getting Started](docs/getting-started.md)
- [Language Overview](docs/language-overview.md)
- [Language Reference](docs/reference.md)
- [Built-in Functions](docs/builtins.md)
- [Classes](docs/classes.md)
- [DLL Mode](docs/dll.md)
- [Concurrency](docs/concurrency.md)
- [Debugger](docs/debugger.md)
- [Examples](docs/examples.md)

## Contributing

PRs, issues, and cleanup are welcome.

If you want to contribute, the most useful things right now are:
- bug fixes
- diagnostics improvements
- language polish
- stdlib cleanup
- docs improvements

## License

MIT
