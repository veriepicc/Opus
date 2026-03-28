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
- Debug mode with source mapping and crash context
- Embedded stdlib fallback in the compiler

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
