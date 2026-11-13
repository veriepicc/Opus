# Standard Library

Sable ships with a growing standard library embedded into the compiler.

Import modules explicitly:

```c
import mem
import text
import fmt
```

## Current modules in active use

| Module | What it is for |
|-------|-----------------|
| `mem` | typed memory reads/writes, `mem.move`, `mem.move_cells`, `mem.make_zero` |
| `ascii` | ASCII classification, case conversion, hex nibble/byte helpers |
| `text` | O(N) string scanning, slicing, trimming, joining, repeating, padding |
| `fmt` | single-pass formatting (`fmt_bool`, `fmt_int`, `fmt_pair`, `fmt_csv3`, `fmt_hex_byte`) |
| `fs` | file read/write helpers |
| `json` | zero-copy `view`-backed JSON DOM parser, `json_stringify`, and fast lookups |
| `http` | WinHTTP `http_fetch`, `http_get`, `http_ok`, and `http_parse_url` |
| `path` | path joining, stem, extension, basename/dirname helpers |
| `process` | process ID, handle, dynamic DLL loading, message box, exit |
| `rand` | SplitMix64 + Xorshift64* PRNG (`rand_seed`, `rand_next`, `rand_range`) |
| `time` | millisecond tick and sleep helpers |
| `vec` | growable `int` vector (`vec_new`, `vec_push`, `vec_append`, `vec_remove_at`) |
| `algo` | median-of-3 quicksort, binary search, reductions, array transforms |
| `option` | `view`-backed optional wrappers (`OptI64`, `OptStr`, `OptPtr`) |
| `result` | `view`-backed result/error wrappers (`ResI64`, `ResStr`, `ResPtr`) |
| `owner` | manual ownership structs (`OwnedPtr`, `OwnedStr`, `OwnedBlock`) |
| `cpp_vector` | `view`-backed interop helper for C++-style vector buffers |
| `prelude` | convenience bundle importing core modules |

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

`mem` and `view` are meant to work together:

```c
import mem

enum HeaderOffset {
    Magic = 0,
    Flags = 4,
}

view Header {
    magic: int @ HeaderOffset.Magic
    flags: int @ HeaderOffset.Flags
}

function int main() {
    let block = mem.make_zero(8)
    let hdr = block as Header
    hdr.magic = 0x1234
    hdr.flags = 7
    free(block)
    return 0
}
```

Use `mem` for allocation/copy/fill/read/write helpers.
Use `view` when those bytes represent a stable layout you want to name directly.
Use named offsets when those layouts are shared with SDK or offset-manager style code.

## `text` and `fmt`

```c
import text
import fmt

function int main() {
    let trimmed = text_trim("  sable  ")
    let pair = fmt_pair("mode", trimmed)
    print(pair)
    print("\n")
    free(trimmed)
    free(pair)
    return 0
}
```

## `fs` and `json`

`json` provides both quick single-field lookups (`json_get_int`, `json_get_bool`, `json_get_string`) and a full `view`-backed DOM (`json_parse`, `json_object_get`, `json_array_get`, `json_stringify`, `json_free`) with UTF-16 surrogate pair decoding.

```c
import fs
import json

function int main() {
    let file_path = "tests/tmp_doc_example.json"
    let src = "{\"name\":\"sable\",\"fast\":true}"
    if !fs_write_ok(file_path, src) {
        return 1
    }

    let content = fs_read_text(file_path)
    if content == 0 {
        return 2
    }
    if !json_get_bool(content, "fast", false) {
        free(content)
        return 3
    }

    let doc = json_parse(content)
    let out = json_stringify(doc)
    json_free(doc)
    free(out)
    free(content)
    return 0
}
```

## `http`

`http` wraps WinHTTP with typed function pointers and `view` overlays (`HttpUrl`, `HttpResponse`):

```c
import http

function int main() {
    let resp = http_fetch("http://127.0.0.1:8080/status")
    if http_ok(resp) {
        println(resp.body)
    }
    http_response_free(resp)
    return 0
}
```

## `vec` and `algo`

`algo` includes median-of-3 quicksort (`algo_sort_i64`), overflow-safe binary search (`algo_binary_search_i64`), and reductions over `vec_data(items)`.

```c
import vec
import algo

function int main() {
    var items = vec_new(4)
    items = vec_push(items, 13)
    items = vec_push(items, 7)
    items = vec_push(items, 11)

    algo_sort_i64(vec_data(items), vec_len(items))
    if algo_binary_search_i64(vec_data(items), vec_len(items), 11) < 0 {
        vec_free(items)
        return 1
    }

    vec_free(items)
    return 0
}
```

## `option`, `result`, and `owner`

`option` and `result` provide `view`-backed nullable-safe wrappers (`opt_i64_some`, `opt_i64_unwrap_or`, `res_i64_ok`, `res_i64_err`), and `owner` provides explicit manual ownership structs (`OwnedPtr`, `OwnedStr`, `OwnedBlock`).

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

- Functions returning heap strings (`text_*`, `fmt_*`, `path_*`, `fs_read_text`, `json_string`, `json_stringify`, `http_get`) transfer ownership to the caller; call `free(...)` when done.
- `test_stdlib_regression.sb` and `test_stdlib_expanded.sb` exercise the standard library surface across modules.
- For raw builtins vs module helpers, prefer the module surface whenever both exist.
