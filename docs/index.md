# Sable

Sable is a systems language focused on direct native Windows x64 output without the usual layers of compiler ceremony.

- No LLVM
- No VM
- One compiler binary
- One native-image pipeline
- EXE by default, DLL when requested

```c
function int main() {
    print("Hello from Sable!\n")
    return 0
}
```

```text
> sable hello.sb
Generated EXE: hello.exe

> sable --run hello.sb
Hello from Sable!
Program returned: 0
```

## Why Sable exists

Sable is built around a simple idea: the language should stay expressive while the implementation stays honest about what it is doing.

- small enough to understand
- direct enough to trust
- flexible enough to enjoy writing
- serious enough to emit real native output without a giant toolchain behind it

## What the compiler does today

- emits native EXEs
- emits native DLLs with `--dll`
- uses `--run` as a real EXE-native-image path
- supports mixed syntax in one file
  `function int add(int a, int b) { ... }`
  `fn add(a: int, b: int) -> int { ... }`
  `fn add(a: int, b: int) => a + b`
  `int add(int a, int b) { ... }`
  `define function add with left as i64, right as i64 returning i64`
- supports structs, views, classes, methods, enums, imports, and `sable.project`
- supports typed FFI, `spawn`, `await`, `parallel for`, and atomic operations
- ships a built-in crash debugger and healing runtime
- ships an embedded stdlib with modules like `mem`, `ascii`, `text`, `fmt`, `fs`, `json`, `http`, `path`, `process`, `rand`, `time`, `vec`, `algo`, `option`, `result`, and `owner`

## Practical notes

- `print(str)` does not append a newline.
- `print_int(n)` and `print_hex(n)` print a line.
- The parser is typo-aware in many common failure cases.

## Start here

[getting-started.md](getting-started.md)
Build the compiler, write the first program, and learn the CLI/project flow.

[reference.md](reference.md)
Compact syntax reference for the current parser surface.

[examples.md](examples.md)
Compile-verified examples that match the current compiler.

## Strengths

- The parser is much less brittle than it used to be in day-to-day writing.
- Views now give raw memory overlays a first-class language surface instead of forcing wrapper/accessor spam around slot blocks.
- Views can use named offsets from constants and enums, which makes SDK and reverse-engineered layouts much easier to keep in one place.
- Small `fn` helpers can infer their return type from the body, which cuts down a lot of `function ptr` or `fn x() -> int` noise in wrapper-style code.
- The debugger can report native faults with source context, and healing mode can keep some tooling-style programs alive long enough to be useful.
- The concurrency surface is part of the actual language: `spawn`, `await`, `parallel for`, and atomics all work today.

## Deep pages

| Page | What it covers |
|------|----------------|
| [language-overview.md](language-overview.md) | language shape and design direction |
| [builtins.md](builtins.md) | low-level builtin/runtime surface |
| [stdlib.md](stdlib.md) | current standard library modules |
| [classes.md](classes.md) | structs, classes, methods, enums |
| [concurrency.md](concurrency.md) | `spawn`, `await`, `parallel for`, atomics |
| [dll.md](dll.md) | DLL-specific behavior and native output notes |
| [debugger.md](debugger.md) | debug sections and healing runtime |

| [simd.md](simd.md) | AVX2 and AVX-512 SIMD intrinsics |

## Layout

```text
src/
  main.ixx       CLI
  sable.ixx       compiler API and runtime builtins
  lexer.ixx      tokenizer
  parser.ixx     parser
  ast.ixx        AST and validation
  codegen.ixx    x64 code generation
  pe.ixx         PE image generation
  project.ixx    sable.project loading
  types.ixx      shared types
  errors.ixx     diagnostics helpers

examples/        runnable example programs
tests/           integration and regression coverage
docs/            documentation
```
