# Opus

Opus is a systems language that compiles directly to native Windows x64 binaries.

No LLVM. No VM. No bytecode detour. No pretending a huge compiler stack is “just the cost of doing business.”

Opus emits PE executables and DLLs directly, keeps the language surface flexible, and tries very hard to stay honest about what every part of the toolchain is actually doing.

## Why this project is interesting

Opus is not just a parser demo or a codegen toy.

It already has:

- native EXE output by default
- native DLL output with `--dll`
- `--run` as a real compile-and-execute path
- mixed syntax support in one file
- structs, classes, methods, enums, imports, and `opus.project`
- typed FFI
- `spawn`, `await`, `parallel for`, and atomic operations
- a built-in debugger / crash-healing runtime with source-aware crash context
- an embedded standard library and low-level runtime surface for real native work

That combination is what gives the project its own personality. It is aiming for small, fast, native, and understandable without being small-minded.

## Example

```c
function int main() {
    print("Hello, World!\n")
    return 0
}
```

```text
> opus hello.op
Generated EXE: hello.exe

> opus --run hello.op
Hello, World!
Program returned: 0
```

Normal EXEs already have a console. `alloc_console()` is mainly for DLL tools or detached situations where you explicitly want to create one.

## Current status

Opus is real and usable, but still early.

Current target:

- Windows only
- x64 only

It is already capable of serious native output and serious compiler work, but it is still evolving fast. Internals are changing, the language is still being sharpened, and polish is still actively improving.

## Why Opus instead of a normal toolchain stack

Because iteration speed and architectural honesty matter.

Current benchmark snapshots show:

- Opus compile times on the tested suite landing roughly in the **7ms to 26ms** range
- MSVC on the same suite landing roughly in the **995ms to 1646ms** range
- Opus runtime wins on several focused benchmarks
- competitive runtime on broader workloads
- in compile-time terms, Opus is already **well over 100x faster than MSVC** on this benchmark set

That is the profile Opus is chasing:

- compiler turnaround measured in single-digit milliseconds on these benches
- runtime that is already competitive and sometimes outright faster
- small native outputs without dragging a giant toolchain story into the shipped artifact

The benchmark sources, exact benchmark harness settings, recorded results, and the exact MSVC release configuration are documented in [BENCHMARKS.md](BENCHMARKS.md).

## Quick start

```text
> opus hello.op
> opus --dll client.op
> opus --run test.op
> opus build
> opus repl
```

## Build the compiler

Requirements:

- Windows
- Visual Studio 2022
- MSVC with modern C++ modules support

Build:

```text
MSBuild.exe Opus.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output:

```text
bin/Release/opus.exe
```

## Repository layout

```text
src/                compiler source
stdlib/             bundled standard library source
docs/               language and toolchain docs
examples/           compile-verified example programs
tests/              regression and integration coverage
selfhost/           self-hosting compiler work
vscode-extension/   syntax highlighting
```

## Documentation

- [Getting Started](docs/getting-started.md)
- [Language Overview](docs/language-overview.md)
- [Language Reference](docs/reference.md)
- [Builtins](docs/builtins.md)
- [Standard Library](docs/stdlib.md)
- [Classes](docs/classes.md)
- [DLL Mode](docs/dll.md)
- [Concurrency](docs/concurrency.md)
- [Debugger](docs/debugger.md)
- [Examples](docs/examples.md)

## Contributing

The highest-value work right now is:

- parser and diagnostics polish
- language ergonomics
- stdlib cleanup and expansion
- docs that stay honest and readable
- performance work that survives the full suite

## License

MIT
