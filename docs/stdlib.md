# Standard Library

Opus ships with a growing standard library embedded into the compiler.

Import modules explicitly:

```c
import mem
import text
import fmt
```

## Current modules in active use

| Module | What it is for |
|-------|-----------------|
| `mem` | typed memory reads/writes and raw helpers |
| `text` | string and text helpers |
| `fmt` | formatting helpers |
| `fs` | file read/write helpers |
| `json` | JSON parsing and helpers |
| `path` | path manipulation |
| `process` | process helpers |
| `rand` | RNG helpers |
| `time` | timing helpers |
| `vec` | growable vectors |
| `algo` | algorithm helpers |
| `ascii` | ASCII helpers |
| `simd` | SIMD helpers |
| `http` | current HTTP surface |

## `mem`

Preferred low-level surface:

```c
import mem

function int main() {
    let buf = malloc(16)
    mem.write(buf, 0x1122334455667788)
    mem.write32(buf + 8, 123)

    if mem.read(buf) == 0 {
        return 1
    }
    if mem.read32(buf + 8) != 123 {
        return 2
    }

    free(buf)
    return 0
}
```

## `text` and `fmt`

```c
import text
import fmt

function int main() {
    let trimmed = text_trim("  opus  ")
    let pair = fmt_pair("mode", trimmed)
    print(pair)
    print("\n")
    free(trimmed)
    free(pair)
    return 0
}
```

## `fs` and `json`

```c
import fs
import json

function int main() {
    let path = "tests/tmp_doc_example.json"
    let src = "{\"name\":\"opus\",\"fast\":true}"
    if !fs_write_ok(path, src) {
        return 1
    }

    let text = fs_read_text(path)
    if text == 0 {
        return 2
    }
    if !json_get_bool(text, "fast", false) {
        free(text)
        return 3
    }

    free(text)
    return 0
}
```

## `vec` and `algo`

```c
import vec
import algo

function int main() {
    var items = vec_new(4)
    items = vec_push(items, 7)
    items = vec_push(items, 11)
    items = vec_push(items, 13)

    if !algo_contains_i64(items.data, items.size, 11) {
        vec_free(items)
        return 1
    }

    vec_free(items)
    return 0
}
```

## `time` and `rand`

```c
import time
import rand

function int main() {
    let t0 = time_tick_ms()
    time_sleep_ms(1)
    if time_elapsed_ms(t0) < 0 {
        return 1
    }

    rand_seed(12345)
    let value = rand_next()
    if value < 0 {
        return 2
    }
    return 0
}
```

## Notes

- The standard library examples in the docs are aligned with the current parser and tests.
- `test_stdlib_expanded.op` is a good reality check for what is currently expected to work together.
- For raw builtins vs module helpers, the docs prefer the module surface whenever both exist.
