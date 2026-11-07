# Getting Started

This is the practical front door: the shortest path from the repo to a native binary.

## Build the compiler

```powershell
MSBuild.exe Sable.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output:

```text
bin/Release/sable.exe
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
> sable hello.sb
Generated EXE: hello.exe
```

Run:

```text
> hello.exe
Hello, World!
```

One-command loop:

```text
> sable --run hello.sb
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
> sable hello.sb
```

### Compile and run

```text
> sable --run hello.sb
> sable -r hello.sb
```

`--run` builds a temporary native EXE, runs it, and reports the return value.

### DLL output

```text
> sable --dll tool.sb
```

Produces `tool.dll`. `main()` still runs, but it runs from the DLL startup path.

## Projects

```c
project MyApp {
    entry: "app/main.sb"
    output: "MyApp.exe"
    mode: "exe"
    debug: true
    include: ["modules"]
}
```

Build:

```text
> sable build
> sable build path/to/sable.project
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

Sable intentionally accepts multiple declaration shapes, and the parser is much better than it used to be at explaining common mistakes.

## CLI reference

```text
sable [options] <file.sb>
sable repl
sable build [path]
```

| Command | Meaning |
|--------|---------|
| `sable <file.sb>` | compile to EXE |
| `sable --run <file.sb>` | compile and run |
| `sable -r <file.sb>` | short form of `--run` |
| `sable --dll <file.sb>` | compile to DLL |
| `sable build` | search for `sable.project` and build it |
| `sable build <path>` | build a specific project file |
| `sable repl` | start the REPL |
| `sable --help` | show usage |

## Next pages

- [language-overview.md](language-overview.md)
- [reference.md](reference.md)
- [examples.md](examples.md)
- [dll.md](dll.md)
