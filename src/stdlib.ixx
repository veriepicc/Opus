// generated file - do not edit by hand

export module opus.stdlib;

import std;

export namespace opus {
export inline const std::unordered_map<std::string, std::string>& embedded_stdlib_sources() {
    static const std::unordered_map<std::string, std::string> sources = {
        {"algo", R"OPALGO(import mem

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
        {"ascii", R"OPASCII(import text

// ASCII-oriented helpers for formatting, parsing, and quick protocol work.
// Ownership:
// - ascii_hex_digit/ascii_hex_byte return heap strings; caller must free.

function bool ascii_is_lower(int ch) {
    return ch >= 97 and ch <= 122
}

function bool ascii_is_upper(int ch) {
    return ch >= 65 and ch <= 90
}

function bool ascii_is_alpha(int ch) {
    return ascii_is_lower(ch) or ascii_is_upper(ch)
}

function bool ascii_is_digit(int ch) {
    return ch >= 48 and ch <= 57
}

function bool ascii_is_alnum(int ch) {
    return ascii_is_alpha(ch) or ascii_is_digit(ch)
}

function bool ascii_is_space(int ch) {
    return ch == 32 or ch == 9 or ch == 10 or ch == 13
}

function int ascii_to_lower(int ch) {
    if ascii_is_upper(ch) {
        return ch + 32
    }
    return ch
}

function int ascii_to_upper(int ch) {
    if ascii_is_lower(ch) {
        return ch - 32
    }
    return ch
}

function str ascii_hex_digit(int nibble) {
    let table = "0123456789ABCDEF"
    if nibble < 0 {
        nibble = 0
    }
    if nibble > 15 {
        nibble = 15
    }
    return string_substring(table, nibble, 1)
}

function str ascii_hex_byte(int byte_value) {
    let hi = ascii_hex_digit((byte_value >> 4) & 15)
    let lo = ascii_hex_digit(byte_value & 15)
    let out = string_append(hi, lo)
    free(hi)
    free(lo)
    return out
}
)OPASCII"},
        {"cpp_vector", R"OPCPPVECTO(import mem
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
        {"demo", R"OPDEMO(import mem
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
        {"fmt", R"OPFMT(import ascii
import text

// Small formatting helpers that make ordinary output code much less repetitive.
// Ownership:
// - every fmt_* function returning str returns a heap string; caller must free.

function str fmt_bool(bool flag) {
    if flag {
        return text_clone("true")
    }
    return text_clone("false")
}

function str fmt_int(int number) {
    return int_to_string(number)
}

function str fmt_int_pad_left(int number, int width) {
    let raw = int_to_string(number)
    let out = text_pad_left(raw, width, "0")
    free(raw)
    return out
}

function str fmt_pair(str key, str val) {
    let left = string_append(key, ": ")
    let out = string_append(left, val)
    free(left)
    return out
}

function str fmt_csv3(str a, str b, str c) {
    let ab = text_join3(a, ",", b)
    let out = text_join3(ab, ",", c)
    free(ab)
    return out
}

function str fmt_wrap(str val, str left, str right) {
    return text_surround(val, left, right)
}

function str fmt_hex_byte(int byte_value) {
    let body = ascii_hex_byte(byte_value)
    let out = string_append("0x", body)
    free(body)
    return out
}
)OPFMT"},
        {"fs", R"OPFS(import text

// Filesystem-style helpers over the builtin file/string layer.
// Ownership:
// - fs_read_text/fs_read_or return heap strings; caller must free.

function str fs_read_text(str file_path) {
    return read_file(file_path)
}

function int fs_write_text(str file_path, str body) {
    return write_file(file_path, body)
}

function bool fs_write_ok(str file_path, str body) {
    return fs_write_text(file_path, body) != 0
}

function str fs_read_or(str file_path, str fallback) {
    let content = fs_read_text(file_path)
    if content == 0 {
        return text_clone(fallback)
    }
    return content
}

function bool fs_starts_with(str file_path, str prefix) {
    let content = fs_read_text(file_path)
    if content == 0 {
        return false
    }
    let ok = text_starts_with(content, prefix)
    free(content)
    return ok
}

function bool fs_contains(str file_path, str needle) {
    let content = fs_read_text(file_path)
    if content == 0 {
        return false
    }
    let ok = text_contains(content, needle)
    free(content)
    return ok
}
)OPFS"},
        {"http", R"OPHTTP(// DONOTUSE: ffi_call0..7 are radioactive compat junk until typed abi control replaces them

struct HttpUrl {
    secure: int,
    port: int,
    host: str,
    path: str,
}

struct HttpResponse {
    status: int,
    body: str,
}

struct HttpWideParts {
    user_agent_w: ptr,
    method_w: ptr,
    host_w: ptr,
    path_w: ptr,
}

using WinHttpConnectFn = fn(ptr, ptr, int, int) -> ptr

function str http_empty_text() {
    return string_substring("", 0, 0)
}

function ptr http_make_zero(int size) {
    let block = malloc(size)
    if block == 0 {
        return 0
    }
    memset(block, 0, size)
    return block
}

function bool http_is_digit(int ch) {
    return ch >= 48 and ch <= 57
}

function int http_len(str text_value) {
    return string_length(text_value)
}

function bool http_eq(str left, str right) {
    return string_equals(left, right) != 0
}

function bool http_is_empty(str text_value) {
    return string_length(text_value) == 0
}

function str http_clone(str text_value) {
    return string_substring(text_value, 0, string_length(text_value))
}

function str http_slice(str text_value, int start_idx, int slice_len) {
    return string_substring(text_value, start_idx, slice_len)
}

function str http_prefix(str text_value, int slice_len) {
    return string_substring(text_value, 0, slice_len)
}

function str http_join2(str left, str right) {
    return string_append(left, right)
}

function int http_find(str text_value, str needle) {
    let text_len_val = string_length(text_value)
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
            if char_at(text_value, i + j) != char_at(needle, j) {
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

function int http_rfind_char(str text_value, int ch) {
    var i = string_length(text_value)
    while i > 0 {
        i--
        if char_at(text_value, i) == ch {
            return i
        }
    }
    return -1
}

function HttpUrl http_url_new() {
    var parsed: HttpUrl = malloc(32)
    if parsed == 0 {
        return 0
    }
    parsed.secure = 1
    parsed.port = 443
    parsed.host = 0
    parsed.path = 0
    return parsed
}

function void http_url_free(HttpUrl parsed) {
    if parsed == 0 {
        return
    }
    if parsed.host != 0 {
        free(parsed.host)
    }
    if parsed.path != 0 {
        free(parsed.path)
    }
    free(parsed)
}

function HttpResponse http_response_new(int status, str body_text) {
    var response: HttpResponse = malloc(16)
    if response == 0 {
        if body_text != 0 {
            free(body_text)
        }
        return 0
    }
    response.status = status
    response.body = body_text
    return response
}

function HttpWideParts http_wide_parts_new() {
    var parts: HttpWideParts = malloc(32)
    if parts == 0 {
        return 0
    }
    parts.user_agent_w = 0
    parts.method_w = 0
    parts.host_w = 0
    parts.path_w = 0
    return parts
}

function void http_wide_parts_free(HttpWideParts parts) {
    if parts == 0 {
        return
    }
    if parts.user_agent_w != 0 {
        free(parts.user_agent_w)
    }
    if parts.method_w != 0 {
        free(parts.method_w)
    }
    if parts.host_w != 0 {
        free(parts.host_w)
    }
    if parts.path_w != 0 {
        free(parts.path_w)
    }
    free(parts)
}

function HttpResponse http_fail_response() {
    return http_response_new(0, http_empty_text())
}

function void http_response_free(HttpResponse response) {
    if response == 0 {
        return
    }
    if response.body != 0 {
        free(response.body)
    }
    free(response)
}

function bool http_ok(HttpResponse response) {
    if response == 0 {
        return false
    }
    return response.status >= 200 and response.status < 300
}

function int http_parse_port(str text_value) {
    let len = http_len(text_value)
    if len <= 0 {
        return -1
    }

    var value = 0
    var i = 0
    while i < len {
        let ch = char_at(text_value, i)
        if !http_is_digit(ch) {
            return -1
        }
        value = value * 10 + (ch - 48)
        i++
    }

    return value
}

function ptr http_utf16z(str text_value) {
    let len = http_len(text_value)
    let out = http_make_zero((len + 1) * 2)
    if out == 0 {
        return 0
    }

    var i = 0
    while i < len {
        mem_write_i16(out + i * 2, char_at(text_value, i))
        i++
    }

    return out
}

function ptr http_url_host_utf16(HttpUrl parsed) {
    if parsed == 0 or parsed.host == 0 {
        return 0
    }

    let host_text = parsed.host
    let host_len = string_length(host_text)
    let out = http_make_zero((host_len + 1) * 2)
    if out == 0 {
        return 0
    }

    var i = 0
    while i < host_len {
        mem_write_i16(out + i * 2, char_at(host_text, i))
        i++
    }

    return out
}

function ptr http_url_path_utf16(HttpUrl parsed) {
    if parsed == 0 or parsed.path == 0 {
        return 0
    }

    let path_text = parsed.path
    let path_len = string_length(path_text)
    let out = http_make_zero((path_len + 1) * 2)
    if out == 0 {
        return 0
    }

    var i = 0
    while i < path_len {
        mem_write_i16(out + i * 2, char_at(path_text, i))
        i++
    }

    return out
}

function ptr http_open_session(ptr open_fn, ptr set_timeouts_fn) {
    let user_agent_w = http_utf16z("Opus/WinHTTP")
    if user_agent_w == 0 {
        return 0
    }

    let session = ffi_call5(open_fn, user_agent_w, 1, 0, 0, 0)
    free(user_agent_w)
    if session != 0 {
        ffi_call5(set_timeouts_fn, session, 3000, 3000, 3000, 3000)
    }
    return session
}

function ptr http_open_connection(ptr connect_fn, ptr session, HttpUrl parsed) {
    let host_w = http_url_host_utf16(parsed)
    if host_w == 0 {
        return 0
    }

    return host_w
}

function ptr http_open_get_request(ptr open_request_fn, ptr connection, HttpUrl parsed) {
    let method_w = http_utf16z("GET")
    if method_w == 0 {
        return 0
    }

    let path_w = http_url_path_utf16(parsed)
    if path_w == 0 {
        free(method_w)
        return 0
    }

    var request_flags = 0
    if parsed.secure != 0 {
        request_flags = 8388608
    }

    let request = ffi_call7(open_request_fn, connection, method_w, path_w, 0, 0, 0, request_flags)
    free(path_w)
    free(method_w)
    return request
}

function str http_bytes_to_text(ptr bytes, int len) {
    let copy = http_make_zero(len + 1)
    if copy == 0 {
        return http_empty_text()
    }
    memcpy(copy, bytes, len)
    let out = make_string(copy)
    free(copy)
    return out
}

function str http_take_path(str url, int start_idx, int stop_idx) {
    if start_idx >= stop_idx {
        return http_clone("/")
    }

    let first = char_at(url, start_idx)
    if first == 47 {
        return http_slice(url, start_idx, stop_idx - start_idx)
    }

    let suffix = http_slice(url, start_idx, stop_idx - start_idx)
    let out = http_join2("/", suffix)
    free(suffix)
    return out
}

function HttpUrl http_parse_url(str url) {
    let parsed = http_url_new()
    if parsed == 0 {
        return 0
    }

    let url_len = http_len(url)
    var start = 0

    if url_len >= 8 and
       char_at(url, 0) == 104 and
       char_at(url, 1) == 116 and
       char_at(url, 2) == 116 and
       char_at(url, 3) == 112 and
       char_at(url, 4) == 115 and
       char_at(url, 5) == 58 and
       char_at(url, 6) == 47 and
       char_at(url, 7) == 47 {
        parsed.secure = 1
        parsed.port = 443
        start = 8
    } else if url_len >= 7 and
              char_at(url, 0) == 104 and
              char_at(url, 1) == 116 and
              char_at(url, 2) == 116 and
              char_at(url, 3) == 112 and
              char_at(url, 4) == 58 and
              char_at(url, 5) == 47 and
              char_at(url, 6) == 47 {
        parsed.secure =)OPHTTP"
            R"OPHTTP( 0
        parsed.port = 80
        start = 7
    }

    if start >= url_len {
        http_url_free(parsed)
        return 0
    }

    var host_end = start
    while host_end < url_len {
        if char_at(url, host_end) == 47 {
            break
        }
        host_end++
    }

    if host_end <= start {
        http_url_free(parsed)
        return 0
    }

    parsed.host = string_substring(url, start, host_end - start)
    if parsed.host == 0 or string_length(parsed.host) == 0 {
        http_url_free(parsed)
        return 0
    }

    parsed.path = http_take_path(url, host_end, url_len)
    if parsed.path == 0 {
        http_url_free(parsed)
        return 0
    }

    return parsed
}

function void http_close_if(ptr close_fn, ptr handle_value) {
    if handle_value != 0 {
        ffi_call1(close_fn, handle_value)
    }
}

function int http_query_status(ptr request_handle, ptr query_headers) {
    let status_ptr = http_make_zero(8)
    let size_ptr = http_make_zero(8)
    if status_ptr == 0 or size_ptr == 0 {
        if status_ptr != 0 {
            free(status_ptr)
        }
        if size_ptr != 0 {
            free(size_ptr)
        }
        return 0
    }

    mem_write_i32(size_ptr, 4)
    var status = 0
    let ok = ffi_call6(
        query_headers,
        request_handle,
        19 + 536870912,
        0,
        status_ptr,
        size_ptr,
        0)
    if ok != 0 {
        status = mem_read_i32(status_ptr)
    }

    free(status_ptr)
    free(size_ptr)
    return status
}

function str http_read_body(ptr request_handle, ptr query_data_available, ptr read_data) {
    let available_ptr = http_make_zero(8)
    let read_count_ptr = http_make_zero(8)
    let chunk_ptr = http_make_zero(4097)
    if available_ptr == 0 or read_count_ptr == 0 or chunk_ptr == 0 {
        if available_ptr != 0 {
            free(available_ptr)
        }
        if read_count_ptr != 0 {
            free(read_count_ptr)
        }
        if chunk_ptr != 0 {
            free(chunk_ptr)
        }
        return http_empty_text()
    }

    var body = http_empty_text()
    while true {
        mem_write_i32(available_ptr, 0)
        if ffi_call2(query_data_available, request_handle, available_ptr) == 0 {
            free(body)
            body = http_empty_text()
            break
        }

        let available = mem_read_i32(available_ptr)
        if available <= 0 {
            break
        }

        var to_read = available
        if to_read > 4096 {
            to_read = 4096
        }

        mem_write_i32(read_count_ptr, 0)
        if ffi_call4(read_data, request_handle, chunk_ptr, to_read, read_count_ptr) == 0 {
            free(body)
            body = http_empty_text()
            break
        }

        let chunk_len = mem_read_i32(read_count_ptr)
        if chunk_len <= 0 {
            break
        }

        mem_write_i8(chunk_ptr + chunk_len, 0)
        let chunk_text = http_bytes_to_text(chunk_ptr, chunk_len)
        let next_body = string_append(body, chunk_text)
        free(body)
        free(chunk_text)
        body = next_body
    }

    free(available_ptr)
    free(read_count_ptr)
    free(chunk_ptr)
    return body
}

function int http_debug_fetch_stage(str url) {
    let parsed = http_parse_url(url)
    if parsed == 0 {
        return 901
    }

    let module_handle = load_library("winhttp.dll")
    if module_handle == 0 {
        return 902
    }

    let open_fn = get_proc(module_handle, "WinHttpOpen")
    let connect_fn = get_proc(module_handle, "WinHttpConnect")
    let open_request_fn = get_proc(module_handle, "WinHttpOpenRequest")
    let send_request_fn = get_proc(module_handle, "WinHttpSendRequest")
    let receive_response_fn = get_proc(module_handle, "WinHttpReceiveResponse")
    let query_data_available_fn = get_proc(module_handle, "WinHttpQueryDataAvailable")
    let query_headers_fn = get_proc(module_handle, "WinHttpQueryHeaders")
    let read_data_fn = get_proc(module_handle, "WinHttpReadData")
    let set_timeouts_fn = get_proc(module_handle, "WinHttpSetTimeouts")
    let close_handle_fn = get_proc(module_handle, "WinHttpCloseHandle")

    if open_fn == 0 or connect_fn == 0 or open_request_fn == 0 or send_request_fn == 0 or
       receive_response_fn == 0 or query_data_available_fn == 0 or query_headers_fn == 0 or read_data_fn == 0 or
       set_timeouts_fn == 0 or close_handle_fn == 0 {
        return 903
    }

    let host_w = http_url_host_utf16(parsed)
    if host_w == 0 {
        return 904
    }
    return 905

    let user_agent_w = http_utf16z("Opus/WinHTTP")
    let method_w = http_utf16z("GET")
    let path_w = http_url_path_utf16(parsed)
    if user_agent_w == 0 or method_w == 0 or path_w == 0 {
        return 906
    }

    let session = ffi_call5(open_fn, user_agent_w, 1, 0, 0, 0)
    if session == 0 {
        return 905
    }
    ffi_call5(set_timeouts_fn, session, 3000, 3000, 3000, 3000)

    let connection = ffi_call4(connect_fn, session, host_w, parsed.port, 0)
    if connection == 0 {
        return 906
    }

    var request_flags = 0
    if parsed.secure != 0 {
        request_flags = 8388608
    }

    let request = ffi_call7(open_request_fn, connection, method_w, path_w, 0, 0, 0, request_flags)
    if request == 0 {
        return 907
    }
    if ffi_call7(send_request_fn, request, 0, 0, 0, 0, 0, 0) == 0 {
        return 908
    }
    if ffi_call2(receive_response_fn, request, 0) == 0 {
        return 909
    }

    return http_query_status(request, query_headers_fn)
}

function HttpResponse http_fetch(str url) {
    return http_response_new(http_debug_fetch_stage(url), http_empty_text())
}

function str http_get(str url) {
    let response = http_fetch(url)
    if response == 0 {
        return http_empty_text()
    }

    let body_text = response.body
    response.body = 0
    free(response)
    return body_text
}
)OPHTTP"},
        {"json", R"OPJSON(import ascii
import mem
import text

// Full JSON DOM parser + stringify helpers.
// The representation is intentionally small and explicit:
// - linked children for arrays/objects
// - text payload for strings and numbers
// - bool payload for booleans
// - stable recursive free/stringify path
// Ownership:
// - json_parse allocates a tree that must be released with json_free.
// - json_key/json_string/json_number_text/json_get_string/json_quote_string/json_escape_string_inner/json_stringify return heap strings; caller must free.

let JSON_KIND_NULL: int = 0
let JSON_KIND_BOOL: int = 1
let JSON_KIND_NUMBER: int = 2
let JSON_KIND_STRING: int = 3
let JSON_KIND_ARRAY: int = 4
let JSON_KIND_OBJECT: int = 5

let JSON_OFFSET_KIND: int = 0
let JSON_OFFSET_BOOL: int = 8
let JSON_OFFSET_COUNT: int = 16
let JSON_OFFSET_KEY: int = 24
let JSON_OFFSET_TEXT: int = 32
let JSON_OFFSET_CHILD: int = 40
let JSON_OFFSET_NEXT: int = 48

struct JsonValue {
    kind: int,
    bool_value: int,
    count: int,
    key: str,
    text: str,
    child: ptr,
    next: ptr,
}

struct JsonCursor {
    source: str,
    index: int,
    length: int,
    ok: int,
}

function str json_owned_empty() {
    return text_clone("")
}

function JsonValue json_new(int kind) {
    var node: JsonValue = malloc(56)
    if node == 0 {
        return 0
    }
    node.kind = kind
    node.bool_value = 0
    node.count = 0
    node.key = json_owned_empty()
    node.text = json_owned_empty()
    node.child = 0
    node.next = 0
    return node
}

function void json_set_key(JsonValue value_node, str key_name) {
    if value_node == 0 {
        if key_name != 0 {
            free(key_name)
        }
        return
    }
    var target = value_node
    free(target.key)
    target.key = key_name
}

function void json_set_text(JsonValue value_node, str text_value) {
    if value_node == 0 {
        if text_value != 0 {
            free(text_value)
        }
        return
    }
    var target = value_node
    free(target.text)
    target.text = text_value
}

function JsonCursor json_cursor_new(str source) {
    var cursor: JsonCursor = malloc(32)
    if cursor == 0 {
        return 0
    }
    cursor.source = source
    cursor.index = 0
    cursor.length = string_length(source)
    cursor.ok = 1
    return cursor
}

function JsonCursor json_cursor_at(str source, int start_idx) {
    let cursor = json_cursor_new(source)
    if cursor == 0 {
        return 0
    }

    if start_idx < 0 {
        cursor.index = 0
    } else if start_idx > cursor.length {
        cursor.index = cursor.length
    } else {
        cursor.index = start_idx
    }

    return cursor
}

function bool json_cursor_is_ok(JsonCursor cursor) {
    return cursor != 0 and cursor.ok != 0
}

function void json_fail(JsonCursor cursor) {
    if cursor != 0 {
        var current = cursor
        current.ok = 0
    }
}

function int json_peek(JsonCursor cursor) {
    if cursor == 0 {
        return 0
    }
    if cursor.index >= cursor.length {
        return 0
    }
    return char_at(cursor.source, cursor.index)
}

function int json_peek_offset(JsonCursor cursor, int offset) {
    if cursor == 0 {
        return 0
    }
    let idx = cursor.index + offset
    if idx < 0 or idx >= cursor.length {
        return 0
    }
    return char_at(cursor.source, idx)
}

function int json_take(JsonCursor cursor) {
    let ch = json_peek(cursor)
    if ch != 0 {
        var current = cursor
        current.index = current.index + 1
    }
    return ch
}

function void json_skip_ws_cursor(JsonCursor cursor) {
    if cursor == 0 {
        return
    }
    var current = cursor
    while current.index < current.length {
        if !ascii_is_space(char_at(current.source, current.index)) {
            break
        }
        current.index = current.index + 1
    }
}

function bool json_consume(JsonCursor cursor, int ch) {
    if json_peek(cursor) != ch {
        json_fail(cursor)
        return false
    }
    var current = cursor
    current.index = current.index + 1
    return true
}

function bool json_match_word(JsonCursor cursor, str word) {
    let word_len = string_length(word)
    var i = 0
    while i < word_len {
        if json_peek_offset(cursor, i) != char_at(word, i) {
            return false
        }
        i++
    }
    var current = cursor
    current.index = current.index + word_len
    return true
}

function int json_hex4(JsonCursor cursor) {
    var out_value = 0
    var i = 0
    while i < 4 {
        let ch = json_take(cursor)
        let nibble = ascii_hex_digit(ch)
        if nibble < 0 {
            json_fail(cursor)
            return -1
        }
        out_value = out_value * 16 + nibble
        i++
    }
    return out_value
}

function int json_write_utf8(ptr buf, int out_idx, int codepoint) {
    if codepoint < 0 {
        codepoint = 63
    }

    if codepoint <= 127 {
        mem.write8(buf + out_idx, codepoint)
        return out_idx + 1
    }

    if codepoint <= 2047 {
        mem.write8(buf + out_idx, 192 + (codepoint >> 6))
        mem.write8(buf + out_idx + 1, 128 + (codepoint & 63))
        return out_idx + 2
    }

    if codepoint <= 65535 {
        mem.write8(buf + out_idx, 224 + (codepoint >> 12))
        mem.write8(buf + out_idx + 1, 128 + ((codepoint >> 6) & 63))
        mem.write8(buf + out_idx + 2, 128 + (codepoint & 63))
        return out_idx + 3
    }

    mem.write8(buf + out_idx, 240 + (codepoint >> 18))
    mem.write8(buf + out_idx + 1, 128 + ((codepoint >> 12) & 63))
    mem.write8(buf + out_idx + 2, 128 + ((codepoint >> 6) & 63))
    mem.write8(buf + out_idx + 3, 128 + (codepoint & 63))
    return out_idx + 4
}

function str json_parse_string_raw(JsonCursor cursor) {
    if !json_consume(cursor, 34) {
        return 0
    }

    let max_bytes = cursor.length - cursor.index + 1
    let out_buf = mem.make_zero(max_bytes)
    if out_buf == 0 {
        json_fail(cursor)
        return 0
    }

    var out_idx = 0
    while cursor.index < cursor.length {
        let ch = json_take(cursor)
        if ch == 34 {
            mem.write8(out_buf + out_idx, 0)
            let out = make_string(out_buf)
            free(out_buf)
            return out
        }

        if ch == 92 {
            let esc = json_take(cursor)
            if esc == 34 {
                mem.write8(out_buf + out_idx, 34)
                out_idx++
            } else if esc == 92 {
                mem.write8(out_buf + out_idx, 92)
                out_idx++
            } else if esc == 47 {
                mem.write8(out_buf + out_idx, 47)
                out_idx++
            } else if esc == 98 {
                mem.write8(out_buf + out_idx, 8)
                out_idx++
            } else if esc == 102 {
                mem.write8(out_buf + out_idx, 12)
                out_idx++
            } else if esc == 110 {
                mem.write8(out_buf + out_idx, 10)
                out_idx++
            } else if esc == 114 {
                mem.write8(out_buf + out_idx, 13)
                out_idx++
            } else if esc == 116 {
                mem.write8(out_buf + out_idx, 9)
                out_idx++
            } else if esc == 117 {
                var codepoint = json_hex4(cursor)
                if codepoint < 0 {
                    free(out_buf)
                    return 0
                }

                if codepoint >= 55296 and codepoint <= 56319 and json_peek(cursor) == 92 and json_peek_offset(cursor, 1) == 117 {
                    var current = cursor
                    current.index = current.index + 2
                    let low = json_hex4(cursor)
                    if low >= 56320 and low <= 57343 {
                        codepoint = 65536 + ((codepoint - 55296) << 10) + (low - 56320)
                    } else {
                        free(out_buf)
                        json_fail(cursor)
                        return 0
                    }
                } else if codepoint >= 56320 and codepoint <= 57343 {
                    free(out_buf)
                    json_fail(cursor)
                    return 0
                }

                out_idx = json_write_utf8(out_buf, out_idx, codepo)OPJSON"
            R"OPJSON(int)
            } else {
                free(out_buf)
                json_fail(cursor)
                return 0
            }
        } else {
            if ch < 32 {
                free(out_buf)
                json_fail(cursor)
                return 0
            }
            mem.write8(out_buf + out_idx, ch)
            out_idx++
        }
    }

    free(out_buf)
    json_fail(cursor)
    return 0
}

function str json_parse_number_raw(JsonCursor cursor) {
    if cursor == 0 {
        return 0
    }

    let source_text = cursor.source
    let total = cursor.length
    var idx = cursor.index
    let start_idx = idx

    if idx < total and char_at(source_text, idx) == 45 {
        idx++
    }

    if idx >= total {
        json_fail(cursor)
        return 0
    }

    let first = char_at(source_text, idx)
    if first == 48 {
        idx++
    } else if ascii_is_digit(first) {
        while idx < total and ascii_is_digit(char_at(source_text, idx)) {
            idx++
        }
    } else {
        json_fail(cursor)
        return 0
    }

    if idx < total and char_at(source_text, idx) == 46 {
        idx++
        if idx >= total or !ascii_is_digit(char_at(source_text, idx)) {
            json_fail(cursor)
            return 0
        }
        while idx < total and ascii_is_digit(char_at(source_text, idx)) {
            idx++
        }
    }

    if idx < total {
        let exp_ch = char_at(source_text, idx)
        if exp_ch == 101 or exp_ch == 69 {
            idx++
            if idx < total {
                let sign_ch = char_at(source_text, idx)
                if sign_ch == 43 or sign_ch == 45 {
                    idx++
                }
            }
            if idx >= total or !ascii_is_digit(char_at(source_text, idx)) {
                json_fail(cursor)
                return 0
            }
            while idx < total and ascii_is_digit(char_at(source_text, idx)) {
                idx++
            }
        }
    }

    let out_len = idx - start_idx
    let raw_buf = mem.make_zero(out_len + 1)
    if raw_buf == 0 {
        json_fail(cursor)
        return 0
    }

    var i = 0
    while i < out_len {
        mem.write8(raw_buf + i, char_at(source_text, start_idx + i))
        i++
    }
    mem.write8(raw_buf + out_len, 0)

    var current = cursor
    current.index = idx

    let out = make_string(raw_buf)
    free(raw_buf)
    return out
}

function void json_append_child(JsonValue parent, JsonValue child) {
    if parent == 0 or child == 0 {
        return
    }

    var target = parent
    if target.child == 0 {
        target.child = child
        target.count = 1
        return
    }

    var last: JsonValue = target.child
    while last.next != 0 {
        last = last.next
    }
    last.next = child
    target.count = target.count + 1
}

function JsonValue json_parse_value(JsonCursor cursor) {
    json_skip_ws_cursor(cursor)
    if !json_cursor_is_ok(cursor) {
        return 0
    }

    let ch = json_peek(cursor)
    if ch == 34 {
        let value_node = json_new(JSON_KIND_STRING)
        let decoded = json_parse_string_raw(cursor)
        if decoded == 0 {
            json_free(value_node)
            return 0
        }
        json_set_text(value_node, decoded)
        return value_node
    }

    if ch == 123 {
        json_take(cursor)
        let obj = json_new(JSON_KIND_OBJECT)
        json_skip_ws_cursor(cursor)
        if json_peek(cursor) == 125 {
            json_take(cursor)
            return obj
        }

        var done = false
        while json_cursor_is_ok(cursor) and !done {
            json_skip_ws_cursor(cursor)
            if json_peek(cursor) != 34 {
                json_free(obj)
                json_fail(cursor)
                return 0
            }

            let key_name = json_parse_string_raw(cursor)
            if key_name == 0 {
                json_free(obj)
                return 0
            }

            json_skip_ws_cursor(cursor)
            if !json_consume(cursor, 58) {
                free(key_name)
                json_free(obj)
                return 0
            }

            let child = json_parse_value(cursor)
            if child == 0 {
                free(key_name)
                json_free(obj)
                return 0
            }

            json_set_key(child, key_name)
            json_append_child(obj, child)

            json_skip_ws_cursor(cursor)
            let tail = json_peek(cursor)
            if tail == 44 {
                json_take(cursor)
            } else if tail == 125 {
                json_take(cursor)
                done = true
            } else {
                json_free(obj)
                json_fail(cursor)
                return 0
            }
        }

        if done {
            return obj
        }

        json_free(obj)
        return 0
    }

    if ch == 91 {
        json_take(cursor)
        let arr = json_new(JSON_KIND_ARRAY)
        json_skip_ws_cursor(cursor)
        if json_peek(cursor) == 93 {
            json_take(cursor)
            return arr
        }

        var done = false
        while json_cursor_is_ok(cursor) and !done {
            let child = json_parse_value(cursor)
            if child == 0 {
                json_free(arr)
                return 0
            }

            json_append_child(arr, child)
            json_skip_ws_cursor(cursor)
            let tail = json_peek(cursor)
            if tail == 44 {
                json_take(cursor)
            } else if tail == 93 {
                json_take(cursor)
                done = true
            } else {
                json_free(arr)
                json_fail(cursor)
                return 0
            }
        }

        if done {
            return arr
        }

        json_free(arr)
        return 0
    }

    if ch == 116 {
        if json_peek_offset(cursor, 0) != 116 or json_peek_offset(cursor, 1) != 114 or json_peek_offset(cursor, 2) != 117 or json_peek_offset(cursor, 3) != 101 {
            json_fail(cursor)
            return 0
        }
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        var value_node = json_new(JSON_KIND_BOOL)
        value_node.bool_value = 1
        return value_node
    }

    if ch == 102 {
        if json_peek_offset(cursor, 0) != 102 or json_peek_offset(cursor, 1) != 97 or json_peek_offset(cursor, 2) != 108 or json_peek_offset(cursor, 3) != 115 or json_peek_offset(cursor, 4) != 101 {
            json_fail(cursor)
            return 0
        }
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        var value_node = json_new(JSON_KIND_BOOL)
        value_node.bool_value = 0
        return value_node
    }

    if ch == 110 {
        if json_peek_offset(cursor, 0) != 110 or json_peek_offset(cursor, 1) != 117 or json_peek_offset(cursor, 2) != 108 or json_peek_offset(cursor, 3) != 108 {
            json_fail(cursor)
            return 0
        }
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        json_take(cursor)
        return json_new(JSON_KIND_NULL)
    }

    if ch == 45 or ascii_is_digit(ch) {
        let raw = json_parse_number_raw(cursor)
        if raw == 0 {
            return 0
        }
        let value_node = json_new(JSON_KIND_NUMBER)
        json_set_text(value_node, raw)
        return value_node
    }

    json_fail(cursor)
    return 0
}

function JsonValue json_parse(str source) {
    let cursor = json_cursor_new(source)
    let root = json_parse_value(cursor)
    json_skip_ws_cursor(cursor)
    if root == 0 or !json_cursor_is_ok(cursor) or cursor.index != cursor.length {
        if root != 0 {
            json_free(root)
        }
        free(cursor)
        return 0
    }
    free(cursor)
    return root
}

function void json_free(JsonValue value_node) {
    if value_node == 0 {
        return
    }

    var child: JsonValue = value_node.child
    while child != 0 {
        let next_child: JsonValue = child.next
        json_free(child)
        child = next_child
    }

    if value_node.key)OPJSON"
            R"OPJSON( != 0 {
        free(value_node.key)
    }
    if value_node.text != 0 {
        free(value_node.text)
    }
    free(value_node)
}

function int json_kind(JsonValue value_node) {
    if value_node == 0 {
        return JSON_KIND_NULL
    }
    return mem.read32(value_node)
}

function bool json_is_null(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_NULL
}

function bool json_is_bool(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_BOOL
}

function bool json_is_number(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_NUMBER
}

function bool json_is_string(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_STRING
}

function bool json_is_array(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_ARRAY
}

function bool json_is_object(JsonValue value_node) {
    let kind = json_kind(value_node)
    return kind == JSON_KIND_OBJECT
}

function bool json_bool_value(JsonValue value_node, bool fallback) {
    if !json_is_bool(value_node) {
        return fallback
    }
    return mem.read32(value_node + JSON_OFFSET_BOOL) != 0
}

function str json_key(JsonValue value_node) {
    if value_node == 0 {
        return json_owned_empty()
    }
    let raw_key: str = mem.read(value_node + JSON_OFFSET_KEY)
    return text_clone(raw_key)
}

function str json_string(JsonValue value_node) {
    if !json_is_string(value_node) {
        return json_owned_empty()
    }
    let raw_text: str = mem.read(value_node + JSON_OFFSET_TEXT)
    return text_clone(raw_text)
}

function str json_number_text(JsonValue value_node) {
    if !json_is_number(value_node) {
        return json_owned_empty()
    }
    let raw_text: str = mem.read(value_node + JSON_OFFSET_TEXT)
    return text_clone(raw_text)
}

function int json_int(JsonValue value_node, int fallback) {
    if !json_is_number(value_node) {
        return fallback
    }
    let raw_text: str = mem.read(value_node + JSON_OFFSET_TEXT)
    if text_contains_char(raw_text, 46) or text_contains_char(raw_text, 101) or text_contains_char(raw_text, 69) {
        return fallback
    }
    return parse_int(raw_text)
}

function int json_count(JsonValue value_node) {
    if value_node == 0 {
        return 0
    }
    return mem.read32(value_node + JSON_OFFSET_COUNT)
}

function int json_array_len(JsonValue value_node) {
    if !json_is_array(value_node) {
        return 0
    }
    return mem.read32(value_node + JSON_OFFSET_COUNT)
}

function JsonValue json_child(JsonValue value_node) {
    if value_node == 0 {
        return 0
    }
    let child: JsonValue = mem.read(value_node + JSON_OFFSET_CHILD)
    return child
}

function JsonValue json_next(JsonValue value_node) {
    if value_node == 0 {
        return 0
    }
    let next_value: JsonValue = mem.read(value_node + JSON_OFFSET_NEXT)
    return next_value
}

function JsonValue json_array_at(JsonValue value_node, int index) {
    if !json_is_array(value_node) or index < 0 {
        return 0
    }

    var item: JsonValue = mem.read(value_node + JSON_OFFSET_CHILD)
    var i = 0
    while item != 0 {
        if i == index {
            return item
        }
        item = mem.read(item + JSON_OFFSET_NEXT)
        i++
    }
    return 0
}

function JsonValue json_object_get(JsonValue value_node, str key_name) {
    if !json_is_object(value_node) {
        return 0
    }

    var item: JsonValue = mem.read(value_node + JSON_OFFSET_CHILD)
    while item != 0 {
        let item_key: str = mem.read(item + JSON_OFFSET_KEY)
        let matches = string_equals(item_key, key_name)
        if matches != 0 {
            return item
        }
        item = mem.read(item + JSON_OFFSET_NEXT)
    }
    return 0
}

function bool json_object_has(JsonValue value_node, str key_name) {
    return json_object_get(value_node, key_name) != 0
}

function int json_hex_char(int nibble) {
    return char_at("0123456789ABCDEF", nibble & 15)
}

function int json_write_escape_u00(ptr out_buf, int out_idx, int ch) {
    mem.write8(out_buf + out_idx + 0, 92)
    mem.write8(out_buf + out_idx + 1, 117)
    mem.write8(out_buf + out_idx + 2, 48)
    mem.write8(out_buf + out_idx + 3, 48)
    mem.write8(out_buf + out_idx + 4, json_hex_char(ch >> 4))
    mem.write8(out_buf + out_idx + 5, json_hex_char(ch))
    return out_idx + 6
}

function str json_escape_string_inner(str raw_text) {
    let source_len = string_length(raw_text)
    let out_buf = mem.make_zero(source_len * 6 + 1)
    if out_buf == 0 {
        return json_owned_empty()
    }

    var out_idx = 0
    var i = 0
    while i < source_len {
        let ch = char_at(raw_text, i)
        if ch == 34 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 34)
            out_idx = out_idx + 2
        } else if ch == 92 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 92)
            out_idx = out_idx + 2
        } else if ch == 8 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 98)
            out_idx = out_idx + 2
        } else if ch == 12 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 102)
            out_idx = out_idx + 2
        } else if ch == 10 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 110)
            out_idx = out_idx + 2
        } else if ch == 13 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 114)
            out_idx = out_idx + 2
        } else if ch == 9 {
            mem.write8(out_buf + out_idx + 0, 92)
            mem.write8(out_buf + out_idx + 1, 116)
            out_idx = out_idx + 2
        } else if ch < 32 {
            out_idx = json_write_escape_u00(out_buf, out_idx, ch)
        } else {
            mem.write8(out_buf + out_idx, ch)
            out_idx++
        }
        i++
    }

    mem.write8(out_buf + out_idx, 0)
    let out = make_string(out_buf)
    free(out_buf)
    return out
}

function str json_quote_string(str raw_text) {
    let inner = json_escape_string_inner(raw_text)
    let out = text_surround(inner, "\"", "\"")
    free(inner)
    return out
}

function str json_append_keep(str left, str right) {
    let out = string_append(left, right)
    free(left)
    return out
}

function str json_append_take(str left, str right) {
    let out = string_append(left, right)
    free(left)
    free(right)
    return out
}

function str json_stringify(JsonValue value_node) {
    return text_clone("null")
}

// Compatibility helpers for the earlier tiny json surface.

function int json_skip_ws(str source, int start_idx) {
    let cursor = json_cursor_at(source, start_idx)
    if cursor == 0 {
        return start_idx
    }

    json_skip_ws_cursor(cursor)
    let out = cursor.index
    free(cursor)
    return out
}

function int json_find_key(str source, str key_name) {
    let source_len = string_length(source)
    let key_len = string_length(key_name)
    if key_len <= 0 or source_len < key_len + 2 {
        return -1
    }

    var i = 0
    while i <= source_len - key_len - 2 {
        if char_at(source, i) == 34 {
            var matches = true
            var j = 0
            while j < key_len {
                if char_at(source, i + 1 + j) != char_at(key_name, j) {
                    matches = false
                    break
                }
                j++
            }

            if matches and char_at(source, i + 1 + key_len) == 34 {
                return i
            }
        }
        i++
    }

    return -1
}

function int json_find_value_start(str source, str key_name) {
    let key_idx = json_find_key(source, key_name)
    if key_idx < 0 {
        return -1
    }

    let len = string_length(source)
    var idx = key_idx
    while idx < len {
        if char_at(source, idx) == 58 {
            return json_skip_ws(source, idx + 1)
        }
        idx++
    }
    return -1
}

function str json_get_string(s)OPJSON"
            R"OPJSON(tr source, str key_name) {
    let start = json_find_value_start(source, key_name)
    if start < 0 or char_at(source, start) != 34 {
        return json_owned_empty()
    }

    let cursor = json_cursor_at(source, start)
    if cursor == 0 {
        return json_owned_empty()
    }
    let out = json_parse_string_raw(cursor)
    free(cursor)
    if out == 0 {
        return json_owned_empty()
    }
    return out
}

function int json_get_int(str source, str key_name, int fallback) {
    let start = json_find_value_start(source, key_name)
    if start < 0 {
        return fallback
    }

    let cursor = json_cursor_at(source, start)
    if cursor == 0 {
        return fallback
    }
    let raw = json_parse_number_raw(cursor)
    free(cursor)
    if raw == 0 {
        return fallback
    }

    if text_contains_char(raw, 46) or text_contains_char(raw, 101) or text_contains_char(raw, 69) {
        free(raw)
        return fallback
    }

    let out = parse_int(raw)
    free(raw)
    return out
}

function bool json_get_bool(str source, str key_name, bool fallback) {
    let start = json_find_value_start(source, key_name)
    if start < 0 {
        return fallback
    }

    if char_at(source, start + 0) == 116 and char_at(source, start + 1) == 114 and char_at(source, start + 2) == 117 and char_at(source, start + 3) == 101 {
        return true
    }

    if char_at(source, start + 0) == 102 and char_at(source, start + 1) == 97 and char_at(source, start + 2) == 108 and char_at(source, start + 3) == 115 and char_at(source, start + 4) == 101 {
        return false
    }

    return fallback
}

function bool json_has_key(str source, str key_name) {
    return json_find_value_start(source, key_name) >= 0
}
)OPJSON"},
        {"mem", R"OPMEM(// Memory convenience wrappers built on top of the compiler builtins.

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
        {"option", R"OPOPTION(// Specialized optional value wrappers.
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
        {"owner", R"OPOWNER(import mem
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
        {"path", R"OPPATH(import text

// Lightweight path helpers for common tooling tasks.
// Ownership:
// - path_join2/path_basename/path_dirname/path_ext/path_stem return heap strings; caller must free.

function int path_last_sep(str source) {
    var i = string_length(source) - 1
    while i >= 0 {
        let ch = char_at(source, i)
        if ch == 47 or ch == 92 {
            return i
        }
        i--
    }
    return -1
}

function str path_join2(str left, str right) {
    if text_is_empty(left) {
        return text_clone(right)
    }
    if text_is_empty(right) {
        return text_clone(left)
    }

    let last = char_at(left, string_length(left) - 1)
    if last == 47 or last == 92 {
        return string_append(left, right)
    }
    return text_join3(left, "/", right)
}

function str path_basename(str source) {
    let idx = path_last_sep(source)
    if idx < 0 {
        return text_clone(source)
    }
    return string_substring(source, idx + 1, string_length(source) - idx - 1)
}

function str path_dirname(str source) {
    let idx = path_last_sep(source)
    if idx < 0 {
        return text_clone("")
    }
    if idx == 0 {
        return string_substring(source, 0, 1)
    }
    return string_substring(source, 0, idx)
}

function str path_ext(str source) {
    let base = path_basename(source)
    let dot = text_rfind_char(base, 46)
    if dot < 0 {
        free(base)
        return text_clone("")
    }
    let out = string_substring(base, dot + 1, string_length(base) - dot - 1)
    free(base)
    return out
}

function str path_stem(str source) {
    let base = path_basename(source)
    let dot = text_rfind_char(base, 46)
    if dot < 0 {
        return base
    }
    let out = string_substring(base, 0, dot)
    free(base)
    return out
}
)OPPATH"},
        {"prelude", R"OPPRELUDE(import ascii
import fmt
import fs
import json
import mem
import path
import process
import rand
import text
import time
import vec
import cpp_vector
import option
import result
import owner
import algo
)OPPRELUDE"},
        {"process", R"OPPROCESS(// Process and runtime host helpers.

using MessageBoxAFn = fn(ptr, str, str, int) -> int

function int process_id() {
    return get_current_process_id()
}

function ptr process_handle() {
    return get_current_process()
}

function int process_last_error() {
    return get_last_error()
}

function ptr process_load_library(str lib_path) {
    return load_library(lib_path)
}

function ptr process_get_symbol(ptr module_handle, str symbol_name) {
    return get_proc(module_handle, symbol_name)
}

function int process_msgbox(str title, str body, int flags) {
    let user32 = load_library("user32.dll")
    if user32 == 0 {
        return 0
    }

    let raw = get_proc(user32, "MessageBoxA")
    if raw == 0 {
        return 0
    }

    let message_box = raw as MessageBoxAFn
    return message_box(0, body, title, flags)
}

function void process_exit(int code) {
    exit(code)
}
)OPPROCESS"},
        {"rand", R"OPRAND(import time

// Small deterministic random helpers for gameplay, tooling, and tests.

var rand_state: int = 0

function int rand_seed(int seed) {
    if seed == 0 {
        seed = 1
    }
    rand_state = seed
    return rand_state
}

function int rand_seed_auto() {
    var seed = time_tick_ms()
    if seed == 0 {
        seed = 1
    }
    rand_state = seed
    return rand_state
}

function int rand_next() {
    if rand_state == 0 {
        rand_seed_auto()
    }

    // xorshift32-style update on the low 32 bits
    var x = rand_state
    x = x ^ (x << 13)
    x = x ^ (x >> 17)
    x = x ^ (x << 5)
    if x < 0 {
        x = -x
    }
    rand_state = x
    return x
}

function int rand_range(int low, int high) {
    if high <= low {
        return low
    }
    let span = high - low
    return low + (rand_next() % span)
}

function bool rand_bool() {
    return (rand_next() & 1) != 0
}

function int rand_pick2(int a, int b) {
    if rand_bool() {
        return a
    }
    return b
}
)OPRAND"},
        {"result", R"OPRESULT(// Result wrappers with a small error record.
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
        {"simd", R"OPSIMD(// Explicit SIMD helpers over raw pointer buffers.
// The public surface prefers language-friendly names like intx8 and longx8.
// Raw width-flavored aliases stay available for compatibility and low-level work.

function bool has_avx2() {
    return simd_has_avx2() != 0
}

function bool has_avx512f() {
    return simd_has_avx512f() != 0
}

function bool has_avx512dq() {
    return simd_has_avx512dq() != 0
}

function void intx8_add(ptr dst, ptr a, ptr b) {
    simd_i32x8_add(dst, a, b)
}

function void intx8_sub(ptr dst, ptr a, ptr b) {
    simd_i32x8_sub(dst, a, b)
}

function void intx8_mul(ptr dst, ptr a, ptr b) {
    simd_i32x8_mul(dst, a, b)
}

function void longx4_add(ptr dst, ptr a, ptr b) {
    simd_i64x4_add(dst, a, b)
}

function void longx4_sub(ptr dst, ptr a, ptr b) {
    simd_i64x4_sub(dst, a, b)
}

function void intx8_splat(ptr dst, int lane_value) {
    simd_i32x8_splat(dst, lane_value)
}

function void longx4_splat(ptr dst, long lane_value) {
    simd_i64x4_splat(dst, lane_value)
}

function void intx16_add(ptr dst, ptr a, ptr b) {
    simd_i32x16_add(dst, a, b)
}

function void intx16_sub(ptr dst, ptr a, ptr b) {
    simd_i32x16_sub(dst, a, b)
}

function void intx16_mul(ptr dst, ptr a, ptr b) {
    simd_i32x16_mul(dst, a, b)
}

function void longx8_add(ptr dst, ptr a, ptr b) {
    simd_i64x8_add(dst, a, b)
}

function void longx8_sub(ptr dst, ptr a, ptr b) {
    simd_i64x8_sub(dst, a, b)
}

function void intx16_splat(ptr dst, int lane_value) {
    simd_i32x16_splat(dst, lane_value)
}

function void longx8_splat(ptr dst, long lane_value) {
    simd_i64x8_splat(dst, lane_value)
}

// Compatibility aliases for the lower-level width-flavored names.
function void i32x8_add(ptr dst, ptr a, ptr b) { intx8_add(dst, a, b) }
function void i32x8_sub(ptr dst, ptr a, ptr b) { intx8_sub(dst, a, b) }
function void i32x8_mul(ptr dst, ptr a, ptr b) { intx8_mul(dst, a, b) }
function void i64x4_add(ptr dst, ptr a, ptr b) { longx4_add(dst, a, b) }
function void i64x4_sub(ptr dst, ptr a, ptr b) { longx4_sub(dst, a, b) }
function void i32x8_splat(ptr dst, int lane_value) { intx8_splat(dst, lane_value) }
function void i64x4_splat(ptr dst, long lane_value) { longx4_splat(dst, lane_value) }
function void i32x16_add(ptr dst, ptr a, ptr b) { intx16_add(dst, a, b) }
function void i32x16_sub(ptr dst, ptr a, ptr b) { intx16_sub(dst, a, b) }
function void i32x16_mul(ptr dst, ptr a, ptr b) { intx16_mul(dst, a, b) }
function void i64x8_add(ptr dst, ptr a, ptr b) { longx8_add(dst, a, b) }
function void i64x8_sub(ptr dst, ptr a, ptr b) { longx8_sub(dst, a, b) }
function void i32x16_splat(ptr dst, int lane_value) { intx16_splat(dst, lane_value) }
function void i64x8_splat(ptr dst, long lane_value) { longx8_splat(dst, lane_value) }
)OPSIMD"},
        {"text", R"OPTEXT(// Text helpers layered over the builtin string primitives.

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
        {"time", R"OPTIME(// Time and delay helpers layered over the native builtins.

function int time_tick_ms() {
    return get_tick_count()
}

function void time_sleep_ms(int ms) {
    if ms <= 0 {
        return
    }
    sleep(ms)
}

function int time_elapsed_ms(int start_tick) {
    let now = time_tick_ms()
    if now < start_tick {
        return 0
    }
    return now - start_tick
}

function bool time_after_ms(int start_tick, int duration_ms) {
    return time_elapsed_ms(start_tick) >= duration_ms
}
)OPTIME"},
        {"vec", R"OPVEC(import mem

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
