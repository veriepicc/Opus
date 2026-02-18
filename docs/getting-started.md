# Getting Started

This guide covers building the Opus compiler from source, writing your first program, and the full CLI reference.

## Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022** (v17.12+) with C++23 support and the "Desktop development with C++" workload
  - The compiler is built from C++20 modules, so you need a recent MSVC toolchain

## Building the Compiler

Clone the repo and build with MSBuild:

```bash
MSBuild.exe Opus.sln /p:Configuration=Release /p:Platform=x64
```

Or open `Opus.sln` in Visual Studio and build the Release x64 configuration.

The output binary lands at:

```
bin/Release/opus.exe
```

Add that to your PATH or reference it directly.

## Your First Program

Create a file called `hello.op`:

```c
function int main() {
    alloc_console()
    print("Hello, World!\n")
    return 0
}
```

A few things to note:
- `function int main()` is the entry point — the compiler looks for `main`
- `alloc_console()` creates a console window (required for EXE mode to see output)
- `print()` does not add a newline automatically — use `\n` explicitly
- Semicolons are optional

Compile it:

```
> opus hello.op
Generated EXE: hello.exe (3072 bytes)
  Entry point: exe startup -> calls main()
Functions:
  main @ 0x0
```

Run it:

```
> hello.exe
Hello, World!
```

## Compilation Modes

### Standalone EXE (default)

```
> opus hello.op
```

Produces `hello.exe` — a standalone Windows executable. No runtime dependencies, no DLLs needed. The generated PE imports from `kernel32.dll` for system functions.

When writing EXE programs, call `alloc_console()` at the start of `main()` so you get a console window for output.

### Windows DLL

```
> opus --dll hello.op
```

Produces `hello.dll` — a valid Windows DLL that calls `main()` on `DLL_PROCESS_ATTACH`. Can be loaded with `LoadLibrary` or injected into a running process.

In DLL mode, `alloc_console()` creates a new console window attached to the host process.

### JIT Mode (compile and run)

```
> opus --run hello.op
```

Compiles to native code in memory and executes immediately. No file is written to disk. Great for quick testing and iteration.

## Multi-File Projects

For projects with multiple source files, create an `opus.project` file:

```c
project MyApp {
    entry: "main.op"
    output: "myapp.dll"
    mode: dll
    debug: true
    healing: "auto"
    include: [
        "src/",
        "lib/"
    ]
}
```

Build it:

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

You can also import individual files directly:

```c
// main.op
import my_library

function int main() {
    alloc_console()
    let result = add_nums(10, 20)
    print_dec(result)
    print("\n")
    return 0
}
```

```c
// my_library.op
function int add_nums(int a, int b) {
    return a + b
}
```

The compiler resolves `import my_library` to `my_library.op` in the same directory.

## Project File Reference

| Field | Description | Default |
|-------|-------------|---------|
| `entry` | Main source file | required |
| `output` | Output filename | `<project_name>.<ext>` |
| `mode` | `dll` or `exe` | `dll` |
| `debug` | Include source maps and debug info | `false` |
| `healing` | Crash recovery mode: `"auto"`, `"freeze"`, `"off"` | based on `debug` |
| `include` | Directories to scan for `.op` files | `[]` |

## CLI Reference

```
Opus Compiler v0.1.0

Usage: opus [options] <file.op>
       opus repl
       opus build [path]
```

### Compilation

| Flag | Description |
|------|-------------|
| `opus <file.op>` | Compile to standalone EXE (default) |
| `opus --dll <file.op>` | Compile to Windows DLL |
| `opus --run <file.op>` | Compile and run immediately (JIT) |
| `opus -r <file.op>` | Short form of `--run` |
| `-o <file>` | Set output filename |
| `-c` | Compile only (don't link) |
| `-S` | Output assembly listing |
| `-O<level>` | Optimization level (0-3) |

### Project Commands

| Command | Description |
|---------|-------------|
| `opus build` | Build project using `opus.project` in current directory (searches parent dirs) |
| `opus build <path>` | Build using a specific project file |

### Other

| Flag | Description |
|------|-------------|
| `opus repl` | Start the interactive REPL |
| `--help` / `-h` | Show help |

## VS Code Extension

There is a VS Code extension for Opus syntax highlighting in the `vscode-extension/` directory. Install the `.vsix` file:

```
code --install-extension vscode-extension/opus-lang-1.0.0.vsix
```

This gives you syntax highlighting for `.op` files.

## Next Steps

- [Language Overview](language-overview.md) — understand the syntax styles and type system
- [Language Reference](reference.md) — complete syntax reference
- [Built-in Functions](builtins.md) — everything in the standard library
- [Examples](examples.md) — real code to learn from
