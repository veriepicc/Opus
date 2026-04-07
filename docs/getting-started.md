# Getting Started

This is the practical front door: the shortest path from the repo to a native binary.

## Build the compiler

```powershell
MSBuild.exe Opus.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output:

```text
bin/Release/opus.exe
```

## First program

```c
function int main() {
    print("Hello, World!\n")
    return 0
}
```

Compile:

```text
> opus hello.op
Generated EXE: hello.exe
```

Run:

```text
> hello.exe
Hello, World!
```

One-command loop:

```text
> opus --run hello.op
Hello, World!
Program returned: 0
```

## Console rule

Normal EXE output already has a console.

Use `alloc_console()` only when:

- compiling a DLL that wants its own console window
- running inside a host without a console
- explicitly allocating one is the goal

## Compile modes

### Default EXE

```text
> opus hello.op
```

### Compile and run

```text
> opus --run hello.op
> opus -r hello.op
```

`--run` builds a temporary native EXE, runs it, and reports the return value.

### DLL output

```text
> opus --dll tool.op
```

Produces `tool.dll`. `main()` still runs, but it runs from the DLL startup path.

## Projects

```c
project MyApp {
    entry: "app/main.op"
    output: "MyApp.exe"
    mode: "exe"
    debug: true
    include: ["modules"]
}
```

Build:

```text
> opus build
> opus build path/to/opus.project
```

Project fields:

| Field | Meaning |
|------|---------|
| `entry` | main source file |
| `output` | output filename |
| `mode` | `"exe"` or `"dll"` |
| `debug` | embed source and line-map sections |
| `healing` | `"auto"`, `"freeze"`, or `"off"` |
| `include` | extra module search directories |

## Function forms that work

```c
function int add(int left, int right) {
    return left + right
}

fn add(left: int, right: int) -> int {
    return left + right
}

int add(int left, int right) {
    return left + right
}

define function add with left as i64, right as i64 returning i64
    return left + right
end function
```

Expression bodies also work:

```c
fn add(left: int, right: int) -> int => left + right
function int add(int left, int right) => left + right
int add(int left, int right) => left + right
```

Opus intentionally accepts multiple declaration shapes, and the parser is much better than it used to be at explaining common mistakes.

## CLI reference

```text
opus [options] <file.op>
opus repl
opus build [path]
```

| Command | Meaning |
|--------|---------|
| `opus <file.op>` | compile to EXE |
| `opus --run <file.op>` | compile and run |
| `opus -r <file.op>` | short form of `--run` |
| `opus --dll <file.op>` | compile to DLL |
| `opus build` | search for `opus.project` and build it |
| `opus build <path>` | build a specific project file |
| `opus repl` | start the REPL |
| `opus --help` | show usage |

## Next pages

- [language-overview.md](language-overview.md)
- [reference.md](reference.md)
- [examples.md](examples.md)
- [dll.md](dll.md)
