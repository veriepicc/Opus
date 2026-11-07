# DLL Mode

Sable can emit a native Windows DLL directly:

```text
sable --dll tool.sb
```

or through `sable.project`:

```c
project Tool {
    entry: "main.sb"
    output: "Tool.dll"
    mode: "dll"
}
```

## Runtime model

DLL output uses the same compiler/codegen pipeline as EXE output. The important difference is the startup wrapper:

- EXE output enters through the EXE startup path
- DLL output enters through `DllMain`
- DLL startup then calls your `main()`

## Console behavior

Unlike a normal EXE, a DLL host may not already have a console.

So this is a normal DLL pattern:

```c
function int main() {
    alloc_console()
    print("DLL loaded\n")
    return 0
}
```

## What DLL mode is good for

- `LoadLibrary` tools
- injected diagnostics/helpers
- process-introspection tools
- utilities that still want the same Sable language surface as EXE builds

## Example

`examples/scanner.sb` is a good DLL-oriented sample:

- compile with `sable --dll examples/scanner.sb`
- it allocates a console explicitly
- it uses `get_module`, process helpers, and memory scanning

## Notes

- DLL and EXE output are siblings on the same native-image path
- typed FFI, imports, classes, and concurrency still work in DLL builds
- if you want a standalone console program, use the default EXE path instead
