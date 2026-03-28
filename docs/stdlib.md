# Opus Stdlib

This directory is a pure-Opus standard library layer that lives outside the C++ compiler/runtime. It is meant to ship as `.op` source and be imported directly by user code.

The goal of this first release is broad, practical coverage:

- useful helpers in current Opus syntax
- honest APIs that match what the host compiler/runtime handles today
- room to grow later without pretending generics, RAII, or full native container support already exist

## Module Layout

| File | Purpose |
|------|---------|
| [`stdlib/prelude.op`](../stdlib/prelude.op) | Convenience import for the full surface |
| [`stdlib/mem.op`](../stdlib/mem.op) | Allocation, copy, compare, and pointer helpers |
| [`stdlib/text.op`](../stdlib/text.op) | String/text helpers built on the builtin string layer |
| [`stdlib/vec.op`](../stdlib/vec.op) | Dynamic 8-byte-cell vector with by-value update operations |
| [`stdlib/cpp_vector.op`](../stdlib/cpp_vector.op) | C++ `std::vector`-style layout/view helpers for interop |
| [`stdlib/option.op`](../stdlib/option.op) | Specialized optional value wrappers |
| [`stdlib/result.op`](../stdlib/result.op) | Specialized success/error wrappers |
| [`stdlib/owner.op`](../stdlib/owner.op) | Manual ownership wrappers for pointers, strings, and raw blocks |
| [`stdlib/algo.op`](../stdlib/algo.op) | Integer and span algorithms |
| [`stdlib/demo.op`](../stdlib/demo.op) | Headless host-compiler smoke test |
| [`stdlib/opus.project`](../stdlib/opus.project) | Project file for building the smoke test |

## Recommended Entry Point

For normal code, import the prelude:

```c
import prelude

function int main() {
    return 0
}
```

The prelude pulls in every stdlib module so common helpers are available without manual wiring.

## What Is Implemented

`mem`

- preferred dot-style surface:
  - `make`, `make_zero`, `make_copy`, `dup`
  - `free_ptr`
  - `read`, `read8`, `read16`, `read32`, `read64`
  - `write`, `write8`, `write16`, `write32`, `write64`
  - `zero`, `fill`, `copy`, `copy_cells`, `compare`
  - `text`, `textz`
  - `offset`, `after`
- compatibility aliases kept for older code:
  - `mem_alloc_bytes`, `mem_alloc_zero`, `mem_alloc_copy`, `mem_dup`
  - `mem_free_ptr`
  - `mem_zero`, `mem_fill`, `mem_copy`, `mem_copy_cells`, `mem_compare`
  - `mem_offset`, `mem_end`

`text`

- length, emptiness, equality, clone, slicing, prefix/suffix
- character access and search helpers
- substring search and containment
- blank/trim helpers
- join, repeat, padding, surround

`vec`

- `Vec { data, size, cap }`
- `vec_new`, `vec_from_array`, `vec_from_array_copy`, `vec_copy`, `vec_to_array_copy`
- `vec_data`, `vec_len`, `vec_cap`, `vec_is_empty`
- `vec_get`, `vec_first`, `vec_last`, `vec_peek_last`, `vec_pop_value`
- `vec_reserve`, `vec_push`, `vec_pop`, `vec_set`, `vec_clear`, `vec_drop_last`, `vec_free`
- `vec_index_of`, `vec_contains`, `vec_append`, `vec_remove_at`, `vec_swap_remove`, `vec_shrink_to_fit`

`cpp_vector`

- `CppVector { first, last, finish }`
- `cpp_vector_read` for reading a foreign vector object at a raw address
- `cpp_vector_from_range`, `cpp_vector_from_array`
- `cpp_vector_size`, `cpp_vector_capacity`, `cpp_vector_is_empty`, `cpp_vector_data`
- `cpp_vector_at_i64`, `cpp_vector_front_i64`, `cpp_vector_back_i64`
- `cpp_vector_copy_view_i64`, `cpp_vector_copy_i64`
- `cpp_vector_free`
- `cpp_vector_call_size`, `cpp_vector_call_data`, `cpp_vector_call_push_back_i64` for bridge/FFI scenarios

`option`

- `OptI64`, `OptPtr`, `OptStr`, `OptBool`
- `some`, `none`, `is_some`, `is_none`, `unwrap`, `unwrap_or`
- `opt_i64_free`, `opt_ptr_free`, `opt_str_free`, `opt_bool_free`

`result`

- `ResI64`, `ResPtr`, `ResStr`, `ResVoid`
- `ok`, `err`, `is_ok`, `is_err`, `unwrap`, `unwrap_or`
- error code and error string accessors
- `res_i64_free`, `res_ptr_free`, `res_str_free`, `res_void_free`

`owner`

- `OwnedPtr`, `OwnedStr`, `OwnedBlock`
- borrow/adopt/alloc helpers
- `get`, `is_owned`, `release`, `free`, `reset`
- by-value dispose helpers: `owned_ptr_dispose`, `owned_str_dispose`, `owned_block_dispose`

`algo`

- integer helpers: min, max, clamp, abs, sign, even/odd
- span helpers over 8-byte cells: fill, copy, reverse, sum, product
- search helpers: index, contains, count, min/max span, binary search
- in-place bubble sort for 8-byte cell spans

## Vector Style

## Memory Style

For new code, prefer the `mem` module surface over the raw builtin names.

This:

```c
import mem

let hs = mem.make_zero(48)
mem.write(hs + 0, 0x3740826714391705)
mem.text(hs + 8, "ui_textured")
mem.write(hs + 24, 11)
mem.write(hs + 32, 15)
```

is the intended style now, instead of:

```c
let hs = malloc(48)
memset(hs, 0, 48)
mem_write(hs + 0, 0x3740826714391705)
mem_write_i8(hs + 8, 117)
```

The raw builtin names still exist underneath because the stdlib is layered on top of them, but they should feel like implementation detail rather than the public face of the language.

`mem.text` writes string bytes exactly as given.

`mem.textz` also appends a trailing null byte.

Use the width-specific helpers only when width actually matters:

- `mem.read32`, `mem.write32`
- `mem.read8`, `mem.write8`

For pointer-sized cells, prefer `mem.read` / `mem.write`.

`vec` uses by-value update functions for mutation. That is intentional.

The current host compiler/runtime handled direct array operations reliably, but pointer-to-struct mutation for a container like `*Vec` was not stable enough for release-quality code. Returning the updated `Vec` keeps the API usable and testable today.

Example:

```c
import vec

function int main() {
    var values = vec_new(4)
    values = vec_push(values, 30)
    values = vec_push(values, 10)
    values = vec_push(values, 20)

    if vec_pop_value(values) == 20 {
        values = vec_pop(values)
    }

    values = vec_free(values)
    return 0
}
```

`vec_pop(vec)` returns the shortened vector.

`vec_pop_value(vec)` returns the item that would be removed.

`vec_from_array(data)` adopts an existing builtin array pointer and relies on the documented builtin layout:

- `data - 16`: length
- `data - 8`: capacity
- `data`: first 8-byte cell

The wrapper now keeps the builtin length header in sync across shrinking operations too, so `array_len(vec_data(v))` stays aligned with `vec_len(v)` after `vec_pop`, `vec_clear`, `vec_remove_at`, `vec_swap_remove`, and `vec_shrink_to_fit`.

Use `vec_from_array_copy` if you want an owned copy without adopting the original builtin array storage.

`vec_reserve` uses bulk cell copies on purpose. `Vec` is explicitly an 8-byte-cell container in this release, so `mem_copy_cells` is safe and simpler than pretending we already have a generic element move story.

`vec_remove_at` still shifts elements one-by-one on purpose. That operation is an overlapping in-place move, and the current stdlib does not expose a `memmove`-style helper yet.

## C++ Vector Interop

`cpp_vector.op` is a realistic interop layer, not a fantasy template binding.

It assumes the common MSVC `std::vector<T>` object shape used in your Ada example:

- first pointer
- last pointer
- end/capacity pointer

and it treats elements as 8-byte cells.

`cpp_vector_from_array` uses the same builtin array header assumption as `vec_from_array` and normalizes obviously bad capacity headers up to at least the observed length before building the view.

This makes it useful for:

- reading vectors exposed by host C++ memory
- copying a foreign vector into an owned Opus `Vec`
- calling compatible bridge functions or instance methods through typed function aliases

Example:

```c
import cpp_vector
import vec

function int main() {
    let foreign = cpp_vector_read(some_cpp_vector_addr)
    let count = cpp_vector_size(foreign)
    let first = cpp_vector_front_i64(foreign)

    var owned = cpp_vector_copy_view_i64(foreign)
    owned = vec_free(owned)
    cpp_vector_free(foreign)

    return 0
}
```

For push-back style interop, prefer a small C or C++ bridge function when possible. `cpp_vector_call_push_back_i64` is available for compatible call sites, but arbitrary templated C++ methods are still ABI-sensitive.

`cpp_vector_free` only frees the temporary Opus view wrapper allocated by this module. It does not free the foreign C++ vector object or its backing storage.

## Option and Result

These are specialized rather than generic because the language does not have generics yet.

Today they are heap-backed handles, and the stdlib exposes matching free helpers for long-lived values.

```c
import option
import result

function int main() {
    let maybe = opt_i64_some(42)
    if opt_i64_is_some(maybe) {
        let n = opt_i64_unwrap(maybe)
    }
    opt_i64_free(maybe)

    let status = res_i64_ok(7)
    if res_i64_is_ok(status) {
        let n = res_i64_unwrap(status)
    }
    res_i64_free(status)

    return 0
}
```

## Ownership Wrappers

`owner.op` gives you explicit manual ownership markers for pointers, strings, and raw blocks. These wrappers do not provide automatic destruction; they encode intent and cleanup conventions in normal Opus code.

```c
import owner

function int main() {
    var owned = owned_ptr_alloc(64)
    if owned_ptr_is_owned(owned) {
        // use owned_ptr_get(owned)
    }
    owned = owned_ptr_dispose(owned)
    return 0
}
```

Prefer the by-value `*_dispose` helpers on the current runtime when you just want to consume and clean up a wrapper value.

## Smoke Test

`stdlib/demo.op` is a headless smoke test, not a console example. It exercises:

- memory helpers
- text helpers
- vector operations
- option/result wrappers
- C++ vector view/copy helpers
- ownership wrappers

and it also frees the temporary heap-backed handle types before returning so the tested path reflects intended usage.

The smoke test now also checks that shrinking vector operations keep the hidden builtin array length synchronized with `Vec.size`.

Build it with the host compiler:

```text
opus build
```

from inside `stdlib/`, or:

```text
opus build stdlib/opus.project
```

from the repo root.

The smoke test is expected to:

- build successfully with the host compiler
- produce `stdlib_demo.exe`
- exit with code `0`

## Limitations

- There are no language generics yet, so `option` and `result` are specialized by payload shape.
- `vec` and `cpp_vector` operate on 8-byte cells. They are not fully type-safe generic containers.
- `cpp_vector` models the common three-pointer `std::vector` layout and is best for MSVC-style interop. Different ABIs or debug iterators may need a bridge layer.
- `option`, `result`, and `cpp_vector` are currently heap-backed handles rather than fully stack-like value types. Use their free helpers when you keep them around beyond a tiny scope.
- `owner` is manual. There are no destructors, drop hooks, or RAII semantics in current Opus.
- `text` still builds on heap-allocated C strings. When a helper creates a new string, the caller owns it.
- Host-compiler validation was done through `opus build` plus the generated EXE. Current JIT `--run` did not provide a reliable validation path for this stdlib because builtin resolution there did not match the EXE build path.
