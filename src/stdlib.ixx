// generated file - do not edit by hand

export module opus.stdlib;

import std;

export namespace opus {
export inline const std::unordered_map<std::string, std::string>& embedded_stdlib_sources() {
    static const std::unordered_map<std::string, std::string> sources = {
        {"algo", R"OPALGO(
import mem

// General utility algorithms for 8-byte cells and integers.

function i64 algo_min_i64(i64 a, i64 b) {
    if a < b {
        return a
    }
    return b
}

function i64 algo_max_i64(i64 a, i64 b) {
    if a > b {
        return a
    }
    return b
}

function i64 algo_clamp_i64(i64 item, i64 lo, i64 hi) {
    if item < lo {
        return lo
    }
    if item > hi {
        return hi
    }
    return item
}

function i64 algo_abs_i64(i64 item) {
    if item < 0 {
        return -item
    }
    return item
}

function i64 algo_sign_i64(i64 item) {
    if item < 0 {
        return -1
    }
    if item > 0 {
        return 1
    }
    return 0
}

function bool algo_is_even_i64(i64 item) {
    return item % 2 == 0
}

function bool algo_is_odd_i64(i64 item) {
    return item % 2 != 0
}

function void algo_swap_i64(ptr a, ptr b) {
    let tmp = mem.read(a)
    mem.write(a, mem.read(b))
    mem.write(b, tmp)
}

function void algo_fill_i64(ptr values, int len, i64 item) {
    for i in range(0, len) {
        mem.write(values + i * 8, item)
    }
}

function void algo_copy_i64(ptr dst, ptr src, int len) {
    memcpy(dst, src, len * 8)
}

function void algo_reverse_i64(ptr values, int len) {
    var left = 0
    var right = len - 1
    while left < right {
        algo_swap_i64(values + left * 8, values + right * 8)
        left++
        right--
    }
}

function i64 algo_sum_i64(ptr values, int len) {
    var total = 0
    for i in range(0, len) {
        total += mem.read(values + i * 8)
    }
    return total
}

function i64 algo_product_i64(ptr values, int len) {
    if len <= 0 {
        return 0
    }

    var total = 1
    for i in range(0, len) {
        total *= mem.read(values + i * 8)
    }
    return total
}

function i64 algo_index_of_i64(ptr values, int len, i64 target) {
    for i in range(0, len) {
        if mem.read(values + i * 8) == target {
            return i
        }
    }
    return -1
}

function bool algo_contains_i64(ptr values, int len, i64 target) {
    return algo_index_of_i64(values, len, target) >= 0
}

function i64 algo_count_i64(ptr values, int len, i64 target) {
    var count = 0
    for i in range(0, len) {
        if mem.read(values + i * 8) == target {
            count++
        }
    }
    return count
}

function i64 algo_min_span_i64(ptr values, int len) {
    if len <= 0 {
        return 0
    }

    var best = mem.read(values)
    for i in range(1, len) {
        let item = mem.read(values + i * 8)
        if item < best {
            best = item
        }
    }
    return best
}

function i64 algo_max_span_i64(ptr values, int len) {
    if len <= 0 {
        return 0
    }

    var best = mem.read(values)
    for i in range(1, len) {
        let item = mem.read(values + i * 8)
        if item > best {
            best = item
        }
    }
    return best
}

function i64 algo_binary_search_i64(ptr values, int len, i64 target) {
    var low = 0
    var high = len - 1
    while low <= high {
        let mid = (low + high) / 2
        let item = mem.read(values + mid * 8)
        if item == target {
            return mid
        }
        if item < target {
            low = mid + 1
        } else {
            high = mid - 1
        }
    }
    return -1
}

function void algo_sort_i64(ptr values, int len) {
    for i in range(0, len) {
        for j in range(0, len - i - 1) {
            let left = mem.read(values + j * 8)
            let right = mem.read(values + (j + 1) * 8)
            if left > right {
                mem.write(values + j * 8, right)
                mem.write(values + (j + 1) * 8, left)
            }
        }
    }
}

)OPALGO"},
        {"cpp_vector", R"OPCPPVECTO(
import mem
import vec

using CppVectorSizeFn = fn(ptr) -> i64
using CppVectorDataFn = fn(ptr) -> ptr
using CppVectorPushBackI64Fn = fn(ptr, i64) -> void

// MSVC std::vector-style layout view for 8-byte cells:
// [first ptr][last ptr][end ptr]
// This is useful for host interop when C++ exposes a vector object or bridge API.

struct CppVector {
    first: ptr,
    last: ptr,
    finish: ptr,
}

function CppVector cpp_vector_from_range(ptr first, ptr last, ptr finish) {
    var view: CppVector = malloc(24)
    if view == 0 {
        return 0
    }
    view.first = first
    view.last = last
    view.finish = finish
    return view
}

function CppVector cpp_vector_read(ptr object_addr) {
    var view: CppVector = malloc(24)
    if view == 0 {
        return 0
    }
    view.first = mem.read(object_addr)
    view.last = mem.read(object_addr + 8)
    view.finish = mem.read(object_addr + 16)
    return view
}

function CppVector cpp_vector_from_array(ptr data) {
    if data == 0 {
        return cpp_vector_from_range(0, 0, 0)
    }

    // This mirrors the builtin array header described in the docs:
    // length at data - 16, capacity at data - 8, cells at data.
    let used = array_len(data)
    var capacity = mem.read(data - 8)
    if capacity < used {
        capacity = used
    }
    let last = data + used * 8
    let finish = data + capacity * 8
    return cpp_vector_from_range(data, last, finish)
}

function ptr cpp_vector_data(CppVector view) {
    return view.first
}

function int cpp_vector_size(CppVector view) {
    let first = view.first
    let last = view.last
    if first == 0 || last < first {
        return 0
    }
    return (last - first) / 8
}

function int cpp_vector_capacity(CppVector view) {
    let first = view.first
    let finish = view.finish
    if first == 0 || finish < first {
        return 0
    }
    return (finish - first) / 8
}

function bool cpp_vector_is_empty(CppVector view) {
    return cpp_vector_size(view) == 0
}

function i64 cpp_vector_at_i64(CppVector view, int idx) {
    let count = cpp_vector_size(view)
    if idx < 0 || idx >= count {
        return 0
    }

    let addr = view.first + idx * 8
    return mem.read(addr)
}

function i64 cpp_vector_front_i64(CppVector view) {
    return cpp_vector_at_i64(view, 0)
}

function i64 cpp_vector_back_i64(CppVector view) {
    let count = cpp_vector_size(view)
    if count <= 0 {
        return 0
    }
    return cpp_vector_at_i64(view, count - 1)
}

function Vec cpp_vector_copy_view_i64(CppVector view) {
    let count = cpp_vector_size(view)
    var out = vec_new(count)
    if count > 0 && view.first != 0 {
        mem_copy_cells(vec_data(out), view.first, count)
        out.size = count
        mem.write(vec_data(out) - 16, count)
    }
    return out
}

function Vec cpp_vector_copy_i64(ptr object_addr) {
    let view = cpp_vector_read(object_addr)
    let out = cpp_vector_copy_view_i64(view)
    cpp_vector_free(view)
    return out
}

function void cpp_vector_free(CppVector view) {
    if view != 0 {
        free(view)
    }
}

function i64 cpp_vector_call_size(ptr method_addr, ptr this_vec) {
    let method = method_addr as CppVectorSizeFn
    return method(this_vec)
}

function ptr cpp_vector_call_data(ptr method_addr, ptr this_vec) {
    let method = method_addr as CppVectorDataFn
    return method(this_vec)
}

function void cpp_vector_call_push_back_i64(ptr method_addr, ptr this_vec, i64 item) {
    let method = method_addr as CppVectorPushBackI64Fn
    method(this_vec, item)
}

)OPCPPVECTO"},
        {"demo", R"OPDEMO(
import mem
import text
import vec
import cpp_vector
import option
import result
import owner
import algo

// Headless smoke test for the standalone stdlib.
// Returns 0 on success so it can be used in automated host-compiler checks.

function int main() {
    let raw = mem_alloc_zero(32)
    if raw == 0 {
        return 1
    } else {
        mem_fill(raw, 0x41, 8)
        mem_write(raw + 8, 1234)
        if mem_read(raw + 8) != 1234 {
            return 2
        }
    }

    let greeting = text_join3("hello", " ", "stdlib")
    let padded = text_pad_right(greeting, 14, ".")
    if text_eq(greeting, "hello stdlib") == 0 {
        return 3
    }
    if text_eq(padded, "hello stdlib..") == 0 {
        return 4
    }

    var numbers = vec_new(4)
    numbers = vec_push(numbers, 30)
    numbers = vec_push(numbers, 10)
    numbers = vec_push(numbers, 20)
    if vec_len(numbers) != 3 {
        return 5
    }
    algo_sort_i64(vec_data(numbers), vec_len(numbers))
    if vec_first(numbers) != 10 {
        return 6
    }
    if vec_last(numbers) != 30 {
        return 7
    }
    numbers = vec_set(numbers, 1, 25)
    if vec_get(numbers, 1) != 25 {
        return 8
    }
    if vec_pop_value(numbers) != 30 {
        return 9
    }
    numbers = vec_pop(numbers)
    if vec_len(numbers) != 2 {
        return 10
    }
    if array_len(vec_data(numbers)) != vec_len(numbers) {
        return 29
    }
    if vec_contains(numbers, 25) == 0 {
        return 11
    }
    if vec_index_of(numbers, 25) != 1 {
        return 12
    }

    let maybe_total = opt_i64_some(vec_len(numbers))
    if opt_i64_is_some(maybe_total) == 0 {
        return 13
    }
    if opt_i64_unwrap(maybe_total) != 2 {
        return 14
    }
    opt_i64_free(maybe_total)

    let parsed = res_i64_ok(vec_first(numbers))
    if res_i64_is_ok(parsed) == 0 {
        return 15
    }
    if res_i64_unwrap(parsed) != 10 {
        return 16
    }
    res_i64_free(parsed)

    let foreign_cells = array_new(3)
    array_set(foreign_cells, 0, 7)
    array_set(foreign_cells, 1, 9)
    array_set(foreign_cells, 2, 11)
    let foreign_view = cpp_vector_from_array(foreign_cells)
    if cpp_vector_size(foreign_view) != 3 {
        return 17
    }
    if cpp_vector_back_i64(foreign_view) != 11 {
        return 18
    }

    var copied = cpp_vector_copy_view_i64(foreign_view)
    if vec_len(copied) != 3 {
        return 19
    }
    if vec_first(copied) != 7 {
        return 20
    }
    copied = vec_append(copied, numbers)
    if vec_len(copied) != 5 {
        return 21
    }
    copied = vec_remove_at(copied, 1)
    if vec_len(copied) != 4 {
        return 22
    }
    if array_len(vec_data(copied)) != vec_len(copied) {
        return 30
    }
    copied = vec_swap_remove(copied, 0)
    if vec_len(copied) != 3 {
        return 23
    }
    if array_len(vec_data(copied)) != vec_len(copied) {
        return 31
    }
    copied = vec_shrink_to_fit(copied)
    if vec_cap(copied) < vec_len(copied) {
        return 24
    }

    let copied_array = vec_to_array_copy(numbers)
    var wrapped_copy = vec_from_array_copy(copied_array)
    if vec_len(wrapped_copy) != vec_len(numbers) {
        return 25
    }
    if vec_first(wrapped_copy) != vec_first(numbers) {
        return 26
    }
    wrapped_copy = vec_clear(wrapped_copy)
    if array_len(vec_data(wrapped_copy)) != 0 {
        return 32
    }

    var owned_copy = owned_str_clone(padded)
    if owned_str_is_owned(owned_copy) == 0 {
        return 27
    }
    if text_eq(owned_str_get(owned_copy), padded) == 0 {
        return 28
    }

    cpp_vector_free(foreign_view)
    copied = vec_free(copied)
    wrapped_copy = vec_free(wrapped_copy)
    numbers = vec_free(numbers)
    owned_copy = owned_str_dispose(owned_copy)
    array_free(copied_array)
    array_free(foreign_cells)
    free(greeting)
    free(padded)
    mem_free_ptr(raw)

    return 0
}
)OPDEMO"},
        {"mem", R"OPMEM(
// Memory convenience wrappers built on top of the compiler builtins.

function ptr make(int size) {
    return malloc(size)
}

function ptr make_zero(int size) {
    let block = malloc(size)
    memset(block, 0, size)
    return block
}

function ptr make_copy(ptr src, int size) {
    let block = malloc(size)
    memcpy(block, src, size)
    return block
}

function void free_ptr(ptr p) {
    if p == 0 {
        return
    }
    free(p)
}

function i64 read(ptr location) {
    return mem_read(location)
}

function i64 read8(ptr location) {
    return mem_read_i8(location)
}

function i64 read16(ptr location) {
    return mem_read_i16(location)
}

function i64 read32(ptr location) {
    return mem_read_i32(location)
}

function i64 read64(ptr location) {
    return mem_read(location)
}

function void write(ptr location, i64 cell) {
    mem_write(location, cell)
}

function void write8(ptr location, int byte_value) {
    mem_write_i8(location, byte_value)
}

function void write16(ptr location, int short_value) {
    mem_write_i16(location, short_value)
}

function void write32(ptr location, int int_value) {
    mem_write_i32(location, int_value)
}

function void write64(ptr location, i64 cell) {
    mem_write(location, cell)
}

function void text(ptr dst, str text_value) {
    let len = string_length(text_value)
    var i = 0
    while i < len {
        write8(dst + i, char_at(text_value, i))
        i++
    }
}

function void textz(ptr dst, str text_value) {
    text(dst, text_value)
    write8(dst + string_length(text_value), 0)
}

function void zero(ptr dst, int size) {
    memset(dst, 0, size)
}

function void fill(ptr dst, int byte_value, int size) {
    memset(dst, byte_value, size)
}

function void copy(ptr dst, ptr src, int size) {
    memcpy(dst, src, size)
}

function void copy_cells(ptr dst, ptr src, int cells) {
    memcpy(dst, src, cells * 8)
}

function int compare(ptr a, ptr b, int size) {
    return memcmp(a, b, size)
}

function ptr dup(ptr src, int size) {
    return make_copy(src, size)
}

function ptr offset(ptr base, int delta) {
    return base + delta
}

function ptr after(ptr base, int delta) {
    return base + delta
}

function ptr mem_alloc_bytes(int size) {
    return make(size)
}

function ptr mem_alloc_zero(int size) {
    return make_zero(size)
}

function ptr mem_alloc_copy(ptr src, int size) {
    return make_copy(src, size)
}

function void mem_free_ptr(ptr p) {
    free_ptr(p)
}

function void mem_zero(ptr dst, int size) {
    zero(dst, size)
}

function void mem_fill(ptr dst, int byte_value, int size) {
    fill(dst, byte_value, size)
}

function void mem_copy(ptr dst, ptr src, int size) {
    copy(dst, src, size)
}

function void mem_copy_cells(ptr dst, ptr src, int cells) {
    copy_cells(dst, src, cells)
}

function int mem_compare(ptr a, ptr b, int size) {
    return compare(a, b, size)
}

function ptr mem_dup(ptr src, int size) {
    return dup(src, size)
}

function ptr mem_offset(ptr base, int offset) {
    return offset(base, offset)
}

function ptr mem_end(ptr base, int offset) {
    return after(base, offset)
}
)OPMEM"},
        {"option", R"OPOPTION(
// Specialized optional value wrappers.
// Opus does not have generics yet, so we keep the most useful payload shapes.

struct OptI64 {
    has_value: i64,
    item: i64,
}

struct OptPtr {
    has_value: i64,
    item: ptr,
}

struct OptStr {
    has_value: i64,
    item: str,
}

struct OptBool {
    has_value: i64,
    item: i64,
}

function OptI64 opt_i64_some(i64 item) {
    var opt: OptI64 = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 1
    opt.item = item
    return opt
}

function OptI64 opt_i64_none() {
    var opt: OptI64 = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 0
    opt.item = 0
    return opt
}

function bool opt_i64_is_some(OptI64 opt) {
    return opt.has_value != 0
}

function bool opt_i64_is_none(OptI64 opt) {
    return opt.has_value == 0
}

function i64 opt_i64_unwrap(OptI64 opt) {
    if opt.has_value != 0 {
        return opt.item
    }
    return 0
}

function i64 opt_i64_unwrap_or(OptI64 opt, i64 fallback) {
    if opt.has_value != 0 {
        return opt.item
    }
    return fallback
}

function void opt_i64_free(OptI64 opt) {
    if opt != 0 {
        free(opt)
    }
}

function OptPtr opt_ptr_some(ptr item_ptr) {
    var opt: OptPtr = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 1
    opt.item = item_ptr
    return opt
}

function OptPtr opt_ptr_none() {
    var opt: OptPtr = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 0
    opt.item = 0
    return opt
}

function bool opt_ptr_is_some(OptPtr opt) {
    return opt.has_value != 0
}

function bool opt_ptr_is_none(OptPtr opt) {
    return opt.has_value == 0
}

function ptr opt_ptr_unwrap(OptPtr opt) {
    if opt.has_value != 0 {
        return opt.item
    }
    return 0
}

function ptr opt_ptr_unwrap_or(OptPtr opt, ptr fallback) {
    if opt.has_value != 0 {
        return opt.item
    }
    return fallback
}

function void opt_ptr_free(OptPtr opt) {
    if opt != 0 {
        free(opt)
    }
}

function OptStr opt_str_some(str text_value) {
    var opt: OptStr = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 1
    opt.item = text_value
    return opt
}

function OptStr opt_str_none() {
    var opt: OptStr = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 0
    opt.item = ""
    return opt
}

function bool opt_str_is_some(OptStr opt) {
    return opt.has_value != 0
}

function bool opt_str_is_none(OptStr opt) {
    return opt.has_value == 0
}

function str opt_str_unwrap(OptStr opt) {
    if opt.has_value != 0 {
        return opt.item
    }
    return ""
}

function str opt_str_unwrap_or(OptStr opt, str fallback) {
    if opt.has_value != 0 {
        return opt.item
    }
    return fallback
}

function void opt_str_free(OptStr opt) {
    if opt != 0 {
        free(opt)
    }
}

function OptBool opt_bool_some(bool flag) {
    var opt: OptBool = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 1
    opt.item = flag
    return opt
}

function OptBool opt_bool_none() {
    var opt: OptBool = malloc(16)
    if opt == 0 {
        return 0
    }
    opt.has_value = 0
    opt.item = 0
    return opt
}

function bool opt_bool_is_some(OptBool opt) {
    return opt.has_value != 0
}

function bool opt_bool_is_none(OptBool opt) {
    return opt.has_value == 0
}

function bool opt_bool_unwrap(OptBool opt) {
    if opt.has_value != 0 {
        return opt.item != 0
    }
    return false
}

function bool opt_bool_unwrap_or(OptBool opt, bool fallback) {
    if opt.has_value != 0 {
        return opt.item != 0
    }
    return fallback
}

function void opt_bool_free(OptBool opt) {
    if opt != 0 {
        free(opt)
    }
}
)OPOPTION"},
        {"owner", R"OPOWNER(
import mem
import text

// Manual ownership helpers.
// These are not RAII. They provide a strong convention for "who frees what".

struct OwnedPtr {
    value: ptr,
    owned: i64,
}

struct OwnedStr {
    value: str,
    owned: i64,
}

struct OwnedBlock {
    value: ptr,
    size: int,
    owned: i64,
}

function OwnedPtr owned_ptr_borrow(ptr value_ptr) {
    let owner = OwnedPtr {
        value: value_ptr,
        owned: 0,
    }
    return owner
}

function OwnedPtr owned_ptr_adopt(ptr value_ptr) {
    let owner = OwnedPtr {
        value: value_ptr,
        owned: 1,
    }
    return owner
}

function OwnedPtr owned_ptr_alloc(int size) {
    let value_ptr = mem_alloc_zero(size)
    let owner = OwnedPtr {
        value: value_ptr,
        owned: 1,
    }
    return owner
}

function ptr owned_ptr_get(OwnedPtr owner) {
    return owner.value
}

function bool owned_ptr_is_owned(OwnedPtr owner) {
    return owner.owned != 0
}

function ptr owned_ptr_release(*OwnedPtr owner) {
    owner.owned = 0
    return owner.value
}

function void owned_ptr_free(*OwnedPtr owner) {
    if owner.owned != 0 {
        if owner.value == 0 {
            owner.value = 0
            owner.owned = 0
            return
        }
        free(owner.value)
    }
    owner.value = 0
    owner.owned = 0
}

function OwnedPtr owned_ptr_dispose(OwnedPtr owner) {
    if owner.owned != 0 && owner.value != 0 {
        free(owner.value)
    }
    owner.value = 0
    owner.owned = 0
    return owner
}

function void owned_ptr_reset(*OwnedPtr owner, ptr value_ptr, bool take_ownership) {
    owned_ptr_free(owner)
    owner.value = value_ptr
    owner.owned = take_ownership
}

function OwnedStr owned_str_borrow(str text_value) {
    let owner = OwnedStr {
        value: text_value,
        owned: 0,
    }
    return owner
}

function OwnedStr owned_str_adopt(str text_value) {
    let owner = OwnedStr {
        value: text_value,
        owned: 1,
    }
    return owner
}

function OwnedStr owned_str_clone(str text_value) {
    let cloned = text_clone(text_value)
    let owner = OwnedStr {
        value: cloned,
        owned: 1,
    }
    return owner
}

function str owned_str_get(OwnedStr owner) {
    return owner.value
}

function bool owned_str_is_owned(OwnedStr owner) {
    return owner.owned != 0
}

function str owned_str_release(*OwnedStr owner) {
    owner.owned = 0
    return owner.value
}

function void owned_str_free(*OwnedStr owner) {
    if owner.owned != 0 {
        if owner.value == 0 {
            owner.value = ""
            owner.owned = 0
            return
        }
        free(owner.value)
    }
    owner.value = ""
    owner.owned = 0
}

function OwnedStr owned_str_dispose(OwnedStr owner) {
    if owner.owned != 0 && owner.value != 0 {
        free(owner.value)
    }
    owner.value = ""
    owner.owned = 0
    return owner
}

function void owned_str_reset(*OwnedStr owner, str text_value, bool take_ownership) {
    owned_str_free(owner)
    owner.value = text_value
    owner.owned = take_ownership
}

function OwnedBlock owned_block_alloc(int size) {
    let value_ptr = mem_alloc_zero(size)
    let block = OwnedBlock {
        value: value_ptr,
        size: size,
        owned: 1,
    }
    return block
}

function OwnedBlock owned_block_borrow(ptr value_ptr, int size) {
    let block = OwnedBlock {
        value: value_ptr,
        size: size,
        owned: 0,
    }
    return block
}

function ptr owned_block_get(OwnedBlock block) {
    return block.value
}

function int owned_block_size(OwnedBlock block) {
    return block.size
}

function bool owned_block_is_owned(OwnedBlock block) {
    return block.owned != 0
}

function ptr owned_block_release(*OwnedBlock block) {
    block.owned = 0
    return block.value
}

function void owned_block_free(*OwnedBlock block) {
    if block.owned != 0 {
        if block.value == 0 {
            block.value = 0
            block.size = 0
            block.owned = 0
            return
        }
        free(block.value)
    }
    block.value = 0
    block.size = 0
    block.owned = 0
}

function OwnedBlock owned_block_dispose(OwnedBlock block) {
    if block.owned != 0 && block.value != 0 {
        free(block.value)
    }
    block.value = 0
    block.size = 0
    block.owned = 0
    return block
}

function void owned_block_reset(*OwnedBlock block, ptr value_ptr, int size, bool take_ownership) {
    owned_block_free(block)
    block.value = value_ptr
    block.size = size
    block.owned = take_ownership
}
)OPOWNER"},
        {"prelude", R"OPPRELUDE(
import mem
import text
import vec
import cpp_vector
import option
import result
import owner
import algo
)OPPRELUDE"},
        {"result", R"OPRESULT(
// Result wrappers with a small error record.
// These are intentionally specialized instead of generic.

struct ResI64 {
    ok: i64,
    item: i64,
    error_code: i64,
    error: str,
}

struct ResPtr {
    ok: i64,
    item: ptr,
    error_code: i64,
    error: str,
}

struct ResStr {
    ok: i64,
    item: str,
    error_code: i64,
    error: str,
}

struct ResVoid {
    ok: i64,
    error_code: i64,
    error: str,
}

function ResI64 res_i64_ok(i64 item) {
    var result: ResI64 = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 1
    result.item = item
    result.error_code = 0
    result.error = ""
    return result
}

function ResI64 res_i64_err(i64 code, str message) {
    var result: ResI64 = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 0
    result.item = 0
    result.error_code = code
    result.error = message
    return result
}

function bool res_i64_is_ok(ResI64 result) {
    return result.ok != 0
}

function bool res_i64_is_err(ResI64 result) {
    return result.ok == 0
}

function i64 res_i64_unwrap(ResI64 result) {
    if result.ok != 0 {
        return result.item
    }
    return 0
}

function i64 res_i64_unwrap_or(ResI64 result, i64 fallback) {
    if result.ok != 0 {
        return result.item
    }
    return fallback
}

function i64 res_i64_error_code(ResI64 result) {
    return result.error_code
}

function str res_i64_error(ResI64 result) {
    return result.error
}

function void res_i64_free(ResI64 result) {
    if result != 0 {
        free(result)
    }
}

function ResPtr res_ptr_ok(ptr item_ptr) {
    var result: ResPtr = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 1
    result.item = item_ptr
    result.error_code = 0
    result.error = ""
    return result
}

function ResPtr res_ptr_err(i64 code, str message) {
    var result: ResPtr = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 0
    result.item = 0
    result.error_code = code
    result.error = message
    return result
}

function bool res_ptr_is_ok(ResPtr result) {
    return result.ok != 0
}

function ptr res_ptr_unwrap(ResPtr result) {
    if result.ok != 0 {
        return result.item
    }
    return 0
}

function ptr res_ptr_unwrap_or(ResPtr result, ptr fallback) {
    if result.ok != 0 {
        return result.item
    }
    return fallback
}

function i64 res_ptr_error_code(ResPtr result) {
    return result.error_code
}

function str res_ptr_error(ResPtr result) {
    return result.error
}

function void res_ptr_free(ResPtr result) {
    if result != 0 {
        free(result)
    }
}

function ResStr res_str_ok(str text_value) {
    var result: ResStr = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 1
    result.item = text_value
    result.error_code = 0
    result.error = ""
    return result
}

function ResStr res_str_err(i64 code, str message) {
    var result: ResStr = malloc(32)
    if result == 0 {
        return 0
    }
    result.ok = 0
    result.item = ""
    result.error_code = code
    result.error = message
    return result
}

function bool res_str_is_ok(ResStr result) {
    return result.ok != 0
}

function str res_str_unwrap(ResStr result) {
    if result.ok != 0 {
        return result.item
    }
    return ""
}

function str res_str_unwrap_or(ResStr result, str fallback) {
    if result.ok != 0 {
        return result.item
    }
    return fallback
}

function i64 res_str_error_code(ResStr result) {
    return result.error_code
}

function str res_str_error(ResStr result) {
    return result.error
}

function void res_str_free(ResStr result) {
    if result != 0 {
        free(result)
    }
}

function ResVoid res_void_ok() {
    var result: ResVoid = malloc(24)
    if result == 0 {
        return 0
    }
    result.ok = 1
    result.error_code = 0
    result.error = ""
    return result
}

function ResVoid res_void_err(i64 code, str message) {
    var result: ResVoid = malloc(24)
    if result == 0 {
        return 0
    }
    result.ok = 0
    result.error_code = code
    result.error = message
    return result
}

function bool res_void_is_ok(ResVoid result) {
    return result.ok != 0
}

function bool res_void_is_err(ResVoid result) {
    return result.ok == 0
}

function i64 res_void_error_code(ResVoid result) {
    return result.error_code
}

function str res_void_error(ResVoid result) {
    return result.error
}

function void res_void_free(ResVoid result) {
    if result != 0 {
        free(result)
    }
}
)OPRESULT"},
        {"text", R"OPTEXT(
// Text helpers layered over the builtin string primitives.

function int text_len(str text) {
    return string_length(text)
}

function bool text_is_empty(str text) {
    return string_length(text) == 0
}

function bool text_eq(str a, str b) {
    return string_equals(a, b) != 0
}

function str text_clone(str text) {
    return string_substring(text, 0, string_length(text))
}

function str text_slice(str text, int start, int length) {
    return string_substring(text, start, length)
}

function str text_prefix(str text, int length) {
    return string_substring(text, 0, length)
}

function str text_suffix(str text, int length) {
    let total = string_length(text)
    if length >= total {
        return text_clone(text)
    }
    return string_substring(text, total - length, length)
}

function int text_char_at(str text, int index) {
    return char_at(text, index)
}

function bool text_starts_with(str text, str prefix) {
    let text_len_val = string_length(text)
    let prefix_len = string_length(prefix)
    if prefix_len > text_len_val {
        return false
    }

    var i = 0
    while i < prefix_len {
        if char_at(text, i) != char_at(prefix, i) {
            return false
        }
        i++
    }

    return true
}

function bool text_ends_with(str text, str suffix) {
    let text_len_val = string_length(text)
    let suffix_len = string_length(suffix)
    if suffix_len > text_len_val {
        return false
    }

    let start = text_len_val - suffix_len
    var i = 0
    while i < suffix_len {
        if char_at(text, start + i) != char_at(suffix, i) {
            return false
        }
        i++
    }

    return true
}

function int text_find_char(str text, int ch) {
    let len = string_length(text)
    var i = 0
    while i < len {
        if char_at(text, i) == ch {
            return i
        }
        i++
    }
    return -1
}

function int text_rfind_char(str text, int ch) {
    var i = string_length(text) - 1
    while i >= 0 {
        if char_at(text, i) == ch {
            return i
        }
        i--
    }
    return -1
}

function int text_count_char(str text, int ch) {
    let len = string_length(text)
    var count = 0
    var i = 0
    while i < len {
        if char_at(text, i) == ch {
            count++
        }
        i++
    }
    return count
}

function bool text_contains_char(str text, int ch) {
    return text_find_char(text, ch) >= 0
}

function int text_find(str text, str needle) {
    let text_len_val = string_length(text)
    let needle_len = string_length(needle)
    if needle_len == 0 {
        return 0
    }
    if needle_len > text_len_val {
        return -1
    }

    var i = 0
    while i <= text_len_val - needle_len {
        var j = 0
        var matched = true
        while j < needle_len {
            if char_at(text, i + j) != char_at(needle, j) {
                matched = false
                break
            }
            j++
        }
        if matched {
            return i
        }
        i++
    }

    return -1
}

function bool text_contains(str text, str needle) {
    return text_find(text, needle) >= 0
}

function bool text_is_blank(str text) {
    let len = string_length(text)
    var i = 0
    while i < len {
        if !is_whitespace(char_at(text, i)) {
            return false
        }
        i++
    }
    return true
}

function str text_trim_left(str text) {
    let len = string_length(text)
    var start = 0
    while start < len {
        if !is_whitespace(char_at(text, start)) {
            break
        }
        start++
    }
    return string_substring(text, start, len - start)
}

function str text_trim_right(str text) {
    var stop = string_length(text)
    while stop > 0 {
        if !is_whitespace(char_at(text, stop - 1)) {
            break
        }
        stop--
    }
    return string_substring(text, 0, stop)
}

function str text_trim(str text) {
    return text_trim_right(text_trim_left(text))
}

function str text_join2(str left, str right) {
    return string_append(left, right)
}

function str text_join3(str a, str b, str c) {
    let ab = string_append(a, b)
    let abc = string_append(ab, c)
    free(ab)
    return abc
}

function str text_join4(str a, str b, str c, str d) {
    let abc = text_join3(a, b, c)
    let abcd = string_append(abc, d)
    free(abc)
    return abcd
}

function str text_repeat(str text, int count) {
    if count <= 0 {
        return string_substring(text, 0, 0)
    }

    var out = text_clone(text)
    var i = 1
    while i < count {
        let next = string_append(out, text)
        free(out)
        out = next
        i++
    }
    return out
}

function str text_pad_left(str text, int width, str fill) {
    let len = string_length(text)
    if len >= width {
        return text_clone(text)
    }

    let pad = text_repeat(fill, width - len)
    let out = string_append(pad, text)
    free(pad)
    return out
}

function str text_pad_right(str text, int width, str fill) {
    let len = string_length(text)
    if len >= width {
        return text_clone(text)
    }

    let pad = text_repeat(fill, width - len)
    let out = string_append(text, pad)
    free(pad)
    return out
}

function str text_surround(str text, str left, str right) {
    let inner = string_append(left, text)
    let out = string_append(inner, right)
    free(inner)
    return out
}
)OPTEXT"},
        {"vec", R"OPVEC(
import mem

// A growable vector layered over the builtin array storage.
// The vector owns its backing array when created via vec_new or vec_copy.
// vec_from_array adopts the passed array pointer and will free it in vec_free.
// Mutating operations return an updated Vec instead of writing through *Vec because
// plain by-value updates are more reliable in the current host compiler/runtime.

struct Vec {
    data: ptr,
    size: int,
    cap: int,
}

function void vec_sync_len(Vec vec) {
    if vec.data != 0 {
        mem.write(vec.data - 16, vec.size)
    }
}

function Vec vec_new(int capacity) {
    if capacity < 4 {
        capacity = 4
    }

    let store = array_new(capacity)
    let vec = Vec {
        data: store,
        size: 0,
        cap: capacity,
    }
    vec_sync_len(vec)
    return vec
}

function Vec vec_from_array(ptr data) {
    if data == 0 {
        return vec_new(4)
    }

    // Builtin array layout per the shipped docs:
    //   data - 16 => logical length
    //   data -  8 => capacity
    //   data      => first 8-byte cell
    // This wrapper is only valid for pointers returned by the builtin array layer.
    let used = array_len(data)
    var capacity = mem.read(data - 8)
    if capacity < used {
        capacity = used
    }
    if capacity < 4 {
        capacity = 4
    }
    let vec = Vec {
        data: data,
        size: used,
        cap: capacity,
    }
    vec_sync_len(vec)
    return vec
}

function Vec vec_from_array_copy(ptr data) {
    if data == 0 {
        return vec_new(4)
    }

    let used = array_len(data)
    var out = vec_new(used)
    if used > 0 {
        mem_copy_cells(out.data, data, used)
        mem.write(out.data - 16, used)
        out.size = used
    }
    return out
}

function ptr vec_data(Vec vec) {
    return vec.data
}

function int vec_len(Vec vec) {
    return vec.size
}

function int vec_cap(Vec vec) {
    return vec.cap
}

function bool vec_is_empty(Vec vec) {
    return vec.size == 0
}

function i64 vec_get(Vec vec, int idx) {
    let used = vec.size
    if idx < 0 || idx >= used {
        return 0
    }
    return array_get(vec.data, idx)
}

function i64 vec_first(Vec vec) {
    if vec.size <= 0 {
        return 0
    }
    return array_get(vec.data, 0)
}

function i64 vec_last(Vec vec) {
    let used = vec.size
    if used <= 0 {
        return 0
    }
    return array_get(vec.data, used - 1)
}

function i64 vec_peek_last(Vec vec) {
    return vec_last(vec)
}

function Vec vec_reserve(Vec vec, int min_capacity) {
    var current = vec.cap
    if min_capacity <= current {
        return vec
    }

    if current < 4 {
        current = 4
    }

    var previous = 0
    while current < min_capacity {
        previous = current
        current = current + current / 2
        if current <= previous {
            current = previous + 1
        }
    }

    let next_data = array_new(current)
    let used = vec.size
    let old_data = vec.data
    if old_data != 0 && used > 0 {
        // Vec stores fixed 8-byte cells, so a bulk copy is safe here.
        mem_copy_cells(next_data, old_data, used)
        mem.write(next_data - 16, used)
    }

    if old_data != 0 {
        array_free(old_data)
    }

    vec.data = next_data
    vec.cap = current
    vec_sync_len(vec)
    return vec
}

function Vec vec_push(Vec vec, i64 item) {
    let used = vec.size
    if used >= vec.cap {
        vec = vec_reserve(vec, used + 1)
    }

    let data = vec.data
    array_set(data, used, item)
    vec.size = used + 1
    vec_sync_len(vec)
    return vec
}

function i64 vec_pop_value(Vec vec) {
    let used = vec.size
    if used <= 0 {
        return 0
    }

    let idx = used - 1
    let data = vec.data
    let item = array_get(data, idx)
    return item
}

function Vec vec_drop_last(Vec vec) {
    let used = vec.size
    if used <= 0 {
        return vec
    }
    vec.size = used - 1
    vec_sync_len(vec)
    return vec
}

function Vec vec_pop(Vec vec) {
    return vec_drop_last(vec)
}

function Vec vec_set(Vec vec, int idx, i64 item) {
    let used = vec.size
    if idx < 0 || idx >= used {
        return vec
    }
    let data = vec.data
    array_set(data, idx, item)
    vec_sync_len(vec)
    return vec
}

function Vec vec_clear(Vec vec) {
    vec.size = 0
    vec_sync_len(vec)
    return vec
}

function i64 vec_index_of(Vec vec, i64 target) {
    let used = vec.size
    for i in range(0, used) {
        if array_get(vec.data, i) == target {
            return i
        }
    }
    return -1
}

function bool vec_contains(Vec vec, i64 target) {
    return vec_index_of(vec, target) >= 0
}

function Vec vec_append(Vec vec, Vec other) {
    let other_used = other.size
    if other_used <= 0 {
        return vec
    }

    let used = vec.size
    vec = vec_reserve(vec, used + other_used)
    mem_copy_cells(vec.data + used * 8, other.data, other_used)
    vec.size = used + other_used
    vec_sync_len(vec)
    return vec
}

function Vec vec_remove_at(Vec vec, int idx) {
    let used = vec.size
    if idx < 0 || idx >= used {
        return vec
    }

    let stop = used - 1
    // Shift left one element at a time because this overlaps in-place and the
    // current stdlib only exposes copy helpers that behave like memcpy, not memmove.
    for i in range(idx, stop) {
        let next_item = array_get(vec.data, i + 1)
        array_set(vec.data, i, next_item)
    }
    vec.size = stop
    vec_sync_len(vec)
    return vec
}

function Vec vec_swap_remove(Vec vec, int idx) {
    let used = vec.size
    if idx < 0 || idx >= used {
        return vec
    }

    let last_idx = used - 1
    if idx < last_idx {
        let last_item = array_get(vec.data, last_idx)
        array_set(vec.data, idx, last_item)
    }
    vec.size = last_idx
    vec_sync_len(vec)
    return vec
}

function Vec vec_shrink_to_fit(Vec vec) {
    let used = vec.size
    var target = used
    if target < 4 {
        target = 4
    }
    if target >= vec.cap {
        return vec
    }

    let old_data = vec.data
    let next_data = array_new(target)
    if old_data != 0 && used > 0 {
        mem_copy_cells(next_data, old_data, used)
        mem.write(next_data - 16, used)
    }
    if old_data != 0 {
        array_free(old_data)
    }

    vec.data = next_data
    vec.cap = target
    vec_sync_len(vec)
    return vec
}

function Vec vec_copy(Vec vec) {
    let used = vec.size
    var copy = vec_new(vec.cap)
    if used > 0 {
        mem_copy_cells(copy.data, vec.data, used)
        mem.write(copy.data - 16, used)
        copy.size = used
    }
    return copy
}

function ptr vec_to_array_copy(Vec vec) {
    let used = vec.size
    let out = array_new(used)
    if used > 0 {
        mem_copy_cells(out, vec.data, used)
        mem.write(out - 16, used)
    }
    return out
}

function Vec vec_free(Vec vec) {
    if vec.data != 0 {
        array_free(vec.data)
    }
    vec.data = 0
    vec.size = 0
    vec.cap = 0
    return vec
}

)OPVEC"},
    };
    return sources;
}
}
