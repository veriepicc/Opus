// opus public api module

module;

// windows headers must go in global module fragment
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#endif

export module opus;

export import opus.types;
export import opus.lexer;
export import opus.parser;
export import opus.ast;
export import opus.codegen;
export import opus.x64;
export import opus.errors;

import std;

export namespace opus {

// runtime builtins

// string table is shared between host and jit code
inline std::vector<std::string>& get_string_table() {
    static std::vector<std::string> table;
    return table;
}

extern "C" void opus_print_int(std::int64_t value) {
    std::println("{}", value);
}

extern "C" void opus_print_str(const char* str) {
    std::print("{}", str);
}

extern "C" void opus_print_newline() {
    std::println("");
}

extern "C" std::int64_t opus_read_file(std::int64_t filename_handle) {
    auto& table = get_string_table();
    if (filename_handle < 0 || filename_handle >= static_cast<std::int64_t>(table.size())) {
        return -1;
    }
    
    std::ifstream file(table[filename_handle]);
    if (!file) {
        return -1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(buffer.str());
    return handle;
}

extern "C" std::int64_t opus_make_string(const char* str) {
    auto& table = get_string_table();
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(str);
    return handle;
}

extern "C" std::int64_t opus_string_length(std::int64_t handle) {
    auto& table = get_string_table();
    if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) {
        return 0;
    }
    return static_cast<std::int64_t>(table[handle].size());
}

extern "C" std::int64_t opus_string_get_char(std::int64_t handle, std::int64_t index) {
    auto& table = get_string_table();
    if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) {
        return 0;
    }
    const auto& str = table[handle];
    if (index < 0 || index >= static_cast<std::int64_t>(str.size())) {
        return 0;
    }
    return static_cast<std::int64_t>(static_cast<unsigned char>(str[index]));
}

extern "C" void opus_print_string(std::int64_t handle) {
    auto& table = get_string_table();
    if (handle >= 0 && handle < static_cast<std::int64_t>(table.size())) {
        std::print("{}", table[handle]);
    }
}

extern "C" std::int64_t opus_write_file(std::int64_t filename_handle, std::int64_t content_handle) {
    auto& table = get_string_table();
    if (filename_handle < 0 || filename_handle >= static_cast<std::int64_t>(table.size())) {
        return -1;
    }
    if (content_handle < 0 || content_handle >= static_cast<std::int64_t>(table.size())) {
        return -1;
    }
    
    std::ofstream file(table[filename_handle], std::ios::binary);
    if (!file) {
        return -1;
    }
    file.write(table[content_handle].data(), table[content_handle].size());
    return static_cast<std::int64_t>(table[content_handle].size());
}

extern "C" std::int64_t opus_malloc(std::int64_t size) {
    void* ptr = std::malloc(static_cast<std::size_t>(size));
    return reinterpret_cast<std::int64_t>(ptr);
}

extern "C" void opus_free(std::int64_t ptr) {
    std::free(reinterpret_cast<void*>(ptr));
}

// raw memory access for systems programming

extern "C" std::int64_t opus_mem_read_i8(std::int64_t addr) {
    if (addr == 0) return 0;
    return static_cast<std::int64_t>(*reinterpret_cast<std::int8_t*>(addr));
}

extern "C" std::int64_t opus_mem_read_i16(std::int64_t addr) {
    if (addr == 0) return 0;
    return static_cast<std::int64_t>(*reinterpret_cast<std::int16_t*>(addr));
}

extern "C" std::int64_t opus_mem_read_i32(std::int64_t addr) {
    if (addr == 0) return 0;
    return static_cast<std::int64_t>(*reinterpret_cast<std::int32_t*>(addr));
}

extern "C" std::int64_t opus_mem_read_i64(std::int64_t addr) {
    if (addr == 0) return 0;
    return *reinterpret_cast<std::int64_t*>(addr);
}

extern "C" double opus_mem_read_f32(std::int64_t addr) {
    if (addr == 0) return 0.0;
    return static_cast<double>(*reinterpret_cast<float*>(addr));
}

extern "C" double opus_mem_read_f64(std::int64_t addr) {
    if (addr == 0) return 0.0;
    return *reinterpret_cast<double*>(addr);
}

// same as i64 but semantically a pointer
extern "C" std::int64_t opus_mem_read_ptr(std::int64_t addr) {
    if (addr == 0) return 0;
    return *reinterpret_cast<std::int64_t*>(addr);
}

extern "C" void opus_mem_write_i8(std::int64_t addr, std::int64_t value) {
    if (addr == 0) return;
    *reinterpret_cast<std::int8_t*>(addr) = static_cast<std::int8_t>(value);
}

extern "C" void opus_mem_write_i16(std::int64_t addr, std::int64_t value) {
    if (addr == 0) return;
    *reinterpret_cast<std::int16_t*>(addr) = static_cast<std::int16_t>(value);
}

extern "C" void opus_mem_write_i32(std::int64_t addr, std::int64_t value) {
    if (addr == 0) return;
    *reinterpret_cast<std::int32_t*>(addr) = static_cast<std::int32_t>(value);
}

extern "C" void opus_mem_write_i64(std::int64_t addr, std::int64_t value) {
    if (addr == 0) return;
    *reinterpret_cast<std::int64_t*>(addr) = value;
}

extern "C" void opus_mem_write_f32(std::int64_t addr, double value) {
    if (addr == 0) return;
    *reinterpret_cast<float*>(addr) = static_cast<float>(value);
}

extern "C" void opus_mem_write_f64(std::int64_t addr, double value) {
    if (addr == 0) return;
    *reinterpret_cast<double*>(addr) = value;
}

extern "C" void opus_mem_write_ptr(std::int64_t addr, std::int64_t value) {
    if (addr == 0) return;
    *reinterpret_cast<std::int64_t*>(addr) = value;
}

extern "C" void opus_mem_copy(std::int64_t dest, std::int64_t src, std::int64_t size) {
    if (dest == 0 || src == 0) return;
    std::memcpy(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), static_cast<std::size_t>(size));
}

extern "C" void opus_mem_set(std::int64_t dest, std::int64_t value, std::int64_t size) {
    if (dest == 0) return;
    std::memset(reinterpret_cast<void*>(dest), static_cast<int>(value), static_cast<std::size_t>(size));
}

// ffi - windows api

#ifdef _WIN32

extern "C" std::int64_t opus_get_module(std::int64_t name_handle) {
    auto& table = get_string_table();
    if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
        // no name = current module
        return reinterpret_cast<std::int64_t>(GetModuleHandleA(nullptr));
    }
    HMODULE mod = GetModuleHandleA(table[name_handle].c_str());
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_get_module_str(const char* name) {
    HMODULE mod = GetModuleHandleA(name);
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_load_library(std::int64_t name_handle) {
    auto& table = get_string_table();
    if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
        return 0;
    }
    HMODULE mod = LoadLibraryA(table[name_handle].c_str());
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_get_proc(std::int64_t module, std::int64_t name_handle) {
    auto& table = get_string_table();
    if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
        return 0;
    }
    FARPROC proc = GetProcAddress(
        reinterpret_cast<HMODULE>(module),
        table[name_handle].c_str()
    );
    return reinterpret_cast<std::int64_t>(proc);
}

extern "C" std::int64_t opus_ffi_call0(std::int64_t fn_ptr) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)();
    return reinterpret_cast<Fn>(fn_ptr)();
}

extern "C" std::int64_t opus_ffi_call1(std::int64_t fn_ptr, std::int64_t a1) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1);
}

extern "C" std::int64_t opus_ffi_call2(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t, std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1, a2);
}

extern "C" std::int64_t opus_ffi_call3(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1, a2, a3);
}

extern "C" std::int64_t opus_ffi_call4(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1, a2, a3, a4);
}

extern "C" std::int64_t opus_ffi_call5(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4, std::int64_t a5) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1, a2, a3, a4, a5);
}

extern "C" std::int64_t opus_ffi_call6(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4, std::int64_t a5, std::int64_t a6) {
    if (fn_ptr == 0) return 0;
    using Fn = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t);
    return reinterpret_cast<Fn>(fn_ptr)(a1, a2, a3, a4, a5, a6);
}

extern "C" std::int64_t opus_msgbox(std::int64_t title_handle, std::int64_t text_handle, std::int64_t flags) {
    auto& table = get_string_table();
    const char* title = "";
    const char* text = "";
    if (title_handle >= 0 && title_handle < static_cast<std::int64_t>(table.size())) {
        title = table[title_handle].c_str();
    }
    if (text_handle >= 0 && text_handle < static_cast<std::int64_t>(table.size())) {
        text = table[text_handle].c_str();
    }
    return MessageBoxA(nullptr, text, title, static_cast<UINT>(flags));
}

extern "C" std::int64_t opus_get_last_error() {
    return static_cast<std::int64_t>(GetLastError());
}

// need a console for dll debugging since dlls dont have one
extern "C" std::int64_t opus_alloc_console() {
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        SetConsoleTitleA("Opus Console");
        
        return 1;
    }
    return 0;
}

extern "C" void opus_free_console() {
    FreeConsole();
}

extern "C" void opus_set_console_title(std::int64_t title_handle) {
    auto& table = get_string_table();
    if (title_handle >= 0 && title_handle < static_cast<std::int64_t>(table.size())) {
        SetConsoleTitleA(table[title_handle].c_str());
    }
}

extern "C" std::int64_t opus_virtual_protect(std::int64_t address, std::int64_t size, std::int64_t new_protect) {
    DWORD old_protect;
    BOOL result = VirtualProtect(
        reinterpret_cast<void*>(address),
        static_cast<SIZE_T>(size),
        static_cast<DWORD>(new_protect),
        &old_protect
    );
    if (result) {
        return static_cast<std::int64_t>(old_protect);
    }
    return -1;
}

extern "C" std::int64_t opus_read_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) {
    SIZE_T bytes_read;
    BOOL result = ReadProcessMemory(
        reinterpret_cast<HANDLE>(process),
        reinterpret_cast<void*>(address),
        reinterpret_cast<void*>(buffer),
        static_cast<SIZE_T>(size),
        &bytes_read
    );
    return result ? static_cast<std::int64_t>(bytes_read) : -1;
}

extern "C" std::int64_t opus_write_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) {
    SIZE_T bytes_written;
    BOOL result = WriteProcessMemory(
        reinterpret_cast<HANDLE>(process),
        reinterpret_cast<void*>(address),
        reinterpret_cast<void*>(buffer),
        static_cast<SIZE_T>(size),
        &bytes_written
    );
    return result ? static_cast<std::int64_t>(bytes_written) : -1;
}

extern "C" std::int64_t opus_get_current_process() {
    return reinterpret_cast<std::int64_t>(GetCurrentProcess());
}

extern "C" std::int64_t opus_get_current_process_id() {
    return static_cast<std::int64_t>(GetCurrentProcessId());
}

#else
// non-windows stubs
extern "C" std::int64_t opus_get_module(std::int64_t name_handle) { return 0; }
extern "C" std::int64_t opus_get_module_str(const char* name) { return 0; }
extern "C" std::int64_t opus_load_library(std::int64_t name_handle) { return 0; }
extern "C" std::int64_t opus_get_proc(std::int64_t module, std::int64_t name_handle) { return 0; }
extern "C" std::int64_t opus_ffi_call0(std::int64_t fn_ptr) { return 0; }
extern "C" std::int64_t opus_ffi_call1(std::int64_t fn_ptr, std::int64_t a1) { return 0; }
extern "C" std::int64_t opus_ffi_call2(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2) { return 0; }
extern "C" std::int64_t opus_ffi_call3(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3) { return 0; }
extern "C" std::int64_t opus_ffi_call4(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4) { return 0; }
extern "C" std::int64_t opus_ffi_call5(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4, std::int64_t a5) { return 0; }
extern "C" std::int64_t opus_ffi_call6(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4, std::int64_t a5, std::int64_t a6) { return 0; }
extern "C" std::int64_t opus_msgbox(std::int64_t title_handle, std::int64_t text_handle, std::int64_t flags) { return 0; }
extern "C" std::int64_t opus_get_last_error() { return 0; }
extern "C" std::int64_t opus_virtual_protect(std::int64_t address, std::int64_t size, std::int64_t new_protect) { return -1; }
extern "C" std::int64_t opus_read_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) { return -1; }
extern "C" std::int64_t opus_write_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) { return -1; }
extern "C" std::int64_t opus_get_current_process() { return 0; }
extern "C" std::int64_t opus_get_current_process_id() { return 0; }
#endif

// layout: arr[0] = length, arr[1..n] = elements
extern "C" std::int64_t opus_array_new(std::int64_t size) {
    std::int64_t* arr = static_cast<std::int64_t*>(std::malloc((size + 1) * sizeof(std::int64_t)));
    if (!arr) return 0;
    arr[0] = size;
    for (std::int64_t i = 1; i <= size; ++i) {
        arr[i] = 0;
    }
    return reinterpret_cast<std::int64_t>(arr);
}

extern "C" std::int64_t opus_array_get(std::int64_t arr_ptr, std::int64_t index) {
    std::int64_t* arr = reinterpret_cast<std::int64_t*>(arr_ptr);
    if (!arr || index < 0 || index >= arr[0]) return 0;
    return arr[index + 1];
}

extern "C" void opus_array_set(std::int64_t arr_ptr, std::int64_t index, std::int64_t value) {
    std::int64_t* arr = reinterpret_cast<std::int64_t*>(arr_ptr);
    if (!arr || index < 0 || index >= arr[0]) return;
    arr[index + 1] = value;
}

extern "C" std::int64_t opus_array_len(std::int64_t arr_ptr) {
    std::int64_t* arr = reinterpret_cast<std::int64_t*>(arr_ptr);
    if (!arr) return 0;
    return arr[0];
}

extern "C" void opus_array_free(std::int64_t arr_ptr) {
    std::free(reinterpret_cast<void*>(arr_ptr));
}

extern "C" std::int64_t opus_string_append(std::int64_t handle1, std::int64_t handle2) {
    auto& table = get_string_table();
    if (handle1 < 0 || handle1 >= static_cast<std::int64_t>(table.size())) return -1;
    if (handle2 < 0 || handle2 >= static_cast<std::int64_t>(table.size())) return -1;
    
    std::int64_t new_handle = static_cast<std::int64_t>(table.size());
    table.push_back(table[handle1] + table[handle2]);
    return new_handle;
}

extern "C" std::int64_t opus_int_to_string(std::int64_t value) {
    auto& table = get_string_table();
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(std::to_string(value));
    return handle;
}

extern "C" void opus_print_char(std::int64_t ch) {
    std::print("{}", static_cast<char>(ch));
}

// self-hosting helpers

extern "C" std::int64_t opus_string_equals(std::int64_t h1, std::int64_t h2) {
    auto& table = get_string_table();
    if (h1 < 0 || h1 >= static_cast<std::int64_t>(table.size())) return 0;
    if (h2 < 0 || h2 >= static_cast<std::int64_t>(table.size())) return 0;
    return table[h1] == table[h2] ? 1 : 0;
}

extern "C" std::int64_t opus_string_substring(std::int64_t handle, std::int64_t start, std::int64_t len) {
    auto& table = get_string_table();
    if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) return -1;
    const auto& str = table[handle];
    if (start < 0 || start >= static_cast<std::int64_t>(str.size())) return -1;
    
    std::int64_t actual_len = std::min(len, static_cast<std::int64_t>(str.size()) - start);
    std::int64_t new_handle = static_cast<std::int64_t>(table.size());
    table.push_back(str.substr(start, actual_len));
    return new_handle;
}

extern "C" std::int64_t opus_is_alpha(std::int64_t ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' ? 1 : 0;
}

extern "C" std::int64_t opus_is_digit(std::int64_t ch) {
    return (ch >= '0' && ch <= '9') ? 1 : 0;
}

extern "C" std::int64_t opus_is_alnum(std::int64_t ch) {
    return opus_is_alpha(ch) || opus_is_digit(ch) ? 1 : 0;
}

extern "C" std::int64_t opus_is_whitespace(std::int64_t ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') ? 1 : 0;
}

extern "C" std::int64_t opus_string_starts_with(std::int64_t str_handle, std::int64_t prefix_handle) {
    auto& table = get_string_table();
    if (str_handle < 0 || str_handle >= static_cast<std::int64_t>(table.size())) return 0;
    if (prefix_handle < 0 || prefix_handle >= static_cast<std::int64_t>(table.size())) return 0;
    return table[str_handle].starts_with(table[prefix_handle]) ? 1 : 0;
}

extern "C" void opus_exit(std::int64_t code) {
    std::exit(static_cast<int>(code));
}

extern "C" std::int64_t opus_write_bytes(std::int64_t filename_handle, std::int64_t arr_ptr, std::int64_t len) {
    auto& table = get_string_table();
    if (filename_handle < 0 || filename_handle >= static_cast<std::int64_t>(table.size())) return -1;
    
    std::int64_t* arr = reinterpret_cast<std::int64_t*>(arr_ptr);
    if (!arr) return -1;
    
    std::ofstream file(table[filename_handle], std::ios::binary);
    if (!file) return -1;
    
    for (std::int64_t i = 0; i < len; ++i) {
        char byte = static_cast<char>(arr[i + 1]);  // +1 because arr[0] is length
        file.write(&byte, 1);
    }
    return len;
}

// byte buffer ops for x64 codegen
extern "C" std::int64_t opus_buffer_new(std::int64_t capacity) {
    return opus_array_new(capacity);
}

extern "C" void opus_buffer_push(std::int64_t arr_ptr, std::int64_t byte) {
    std::int64_t* arr = reinterpret_cast<std::int64_t*>(arr_ptr);
    if (!arr) return;
    std::int64_t len = arr[0];
    // no bounds check, caller must ensure capacity
    arr[len + 1] = byte;
    arr[0] = len + 1;
}

extern "C" std::int64_t opus_buffer_len(std::int64_t arr_ptr) {
    return opus_array_len(arr_ptr);
}

extern "C" std::int64_t opus_parse_int(std::int64_t handle) {
    auto& table = get_string_table();
    if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) return 0;
    try {
        return std::stoll(table[handle]);
    } catch (...) {
        return 0;
    }
}

// pointers to runtime functions, passed into jit code
struct RuntimeFunctions {
    void* print_int = reinterpret_cast<void*>(&opus_print_int);
    void* print_str = reinterpret_cast<void*>(&opus_print_str);
    void* print_newline = reinterpret_cast<void*>(&opus_print_newline);
    void* read_file = reinterpret_cast<void*>(&opus_read_file);
    void* string_length = reinterpret_cast<void*>(&opus_string_length);
    void* string_get_char = reinterpret_cast<void*>(&opus_string_get_char);
    void* print_string = reinterpret_cast<void*>(&opus_print_string);
    void* make_string = reinterpret_cast<void*>(&opus_make_string);
    void* write_file = reinterpret_cast<void*>(&opus_write_file);
    void* malloc_fn = reinterpret_cast<void*>(&opus_malloc);
    void* free_fn = reinterpret_cast<void*>(&opus_free);
    void* array_new = reinterpret_cast<void*>(&opus_array_new);
    void* array_get = reinterpret_cast<void*>(&opus_array_get);
    void* array_set = reinterpret_cast<void*>(&opus_array_set);
    void* array_len = reinterpret_cast<void*>(&opus_array_len);
    void* array_free = reinterpret_cast<void*>(&opus_array_free);
    void* string_append = reinterpret_cast<void*>(&opus_string_append);
    void* int_to_string = reinterpret_cast<void*>(&opus_int_to_string);
    void* print_char = reinterpret_cast<void*>(&opus_print_char);
    // self-hosting helpers
    void* string_equals = reinterpret_cast<void*>(&opus_string_equals);
    void* string_substring = reinterpret_cast<void*>(&opus_string_substring);
    void* is_alpha = reinterpret_cast<void*>(&opus_is_alpha);
    void* is_digit = reinterpret_cast<void*>(&opus_is_digit);
    void* is_alnum = reinterpret_cast<void*>(&opus_is_alnum);
    void* is_whitespace = reinterpret_cast<void*>(&opus_is_whitespace);
    void* string_starts_with = reinterpret_cast<void*>(&opus_string_starts_with);
    void* exit_fn = reinterpret_cast<void*>(&opus_exit);
    void* write_bytes = reinterpret_cast<void*>(&opus_write_bytes);
    void* buffer_new = reinterpret_cast<void*>(&opus_buffer_new);
    void* buffer_push = reinterpret_cast<void*>(&opus_buffer_push);
    void* buffer_len = reinterpret_cast<void*>(&opus_buffer_len);
    void* parse_int = reinterpret_cast<void*>(&opus_parse_int);
    // memory ops
    void* mem_read_i8 = reinterpret_cast<void*>(&opus_mem_read_i8);
    void* mem_read_i16 = reinterpret_cast<void*>(&opus_mem_read_i16);
    void* mem_read_i32 = reinterpret_cast<void*>(&opus_mem_read_i32);
    void* mem_read_i64 = reinterpret_cast<void*>(&opus_mem_read_i64);
    void* mem_read_f32 = reinterpret_cast<void*>(&opus_mem_read_f32);
    void* mem_read_f64 = reinterpret_cast<void*>(&opus_mem_read_f64);
    void* mem_read_ptr = reinterpret_cast<void*>(&opus_mem_read_ptr);
    void* mem_write_i8 = reinterpret_cast<void*>(&opus_mem_write_i8);
    void* mem_write_i16 = reinterpret_cast<void*>(&opus_mem_write_i16);
    void* mem_write_i32 = reinterpret_cast<void*>(&opus_mem_write_i32);
    void* mem_write_i64 = reinterpret_cast<void*>(&opus_mem_write_i64);
    void* mem_write_f32 = reinterpret_cast<void*>(&opus_mem_write_f32);
    void* mem_write_f64 = reinterpret_cast<void*>(&opus_mem_write_f64);
    void* mem_write_ptr = reinterpret_cast<void*>(&opus_mem_write_ptr);
    void* mem_copy = reinterpret_cast<void*>(&opus_mem_copy);
    void* mem_set = reinterpret_cast<void*>(&opus_mem_set);
    // ffi
    void* get_module = reinterpret_cast<void*>(&opus_get_module);
    void* load_library = reinterpret_cast<void*>(&opus_load_library);
    void* get_proc = reinterpret_cast<void*>(&opus_get_proc);
    void* ffi_call0 = reinterpret_cast<void*>(&opus_ffi_call0);
    void* ffi_call1 = reinterpret_cast<void*>(&opus_ffi_call1);
    void* ffi_call2 = reinterpret_cast<void*>(&opus_ffi_call2);
    void* ffi_call3 = reinterpret_cast<void*>(&opus_ffi_call3);
    void* ffi_call4 = reinterpret_cast<void*>(&opus_ffi_call4);
    void* ffi_call5 = reinterpret_cast<void*>(&opus_ffi_call5);
    void* ffi_call6 = reinterpret_cast<void*>(&opus_ffi_call6);
    void* msgbox = reinterpret_cast<void*>(&opus_msgbox);
    void* get_last_error = reinterpret_cast<void*>(&opus_get_last_error);
    void* virtual_protect = reinterpret_cast<void*>(&opus_virtual_protect);
    void* get_current_process = reinterpret_cast<void*>(&opus_get_current_process);
    void* get_current_process_id = reinterpret_cast<void*>(&opus_get_current_process_id);
};

// meyers singleton so this works across module boundaries
inline RuntimeFunctions& get_runtime() {
    static RuntimeFunctions instance;
    return instance;
}

// compiler

class Compiler {
public:
    struct Result {
        bool success = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<std::uint8_t> code;
        std::unordered_map<std::string, std::size_t> function_offsets;
        
        // (patch_site, iat_offset)
        std::vector<std::pair<std::size_t, std::size_t>> iat_fixups;
        
        // (instruction_offset, source_line)
        std::vector<std::pair<std::uint32_t, std::uint32_t>> line_map;
        
        // set when code has a writable slot in .text for globals base ptr
        bool needs_writable_text = false;
    };

    Compiler() = default;

    static RuntimeFunctions& runtime() { return get_runtime(); }

    Result compile(std::string_view source, std::string_view filename = "<input>", bool dll_mode = false, const std::string& healing_mode = "off", bool exe_mode = false) {
        Result result;

        Lexer lexer(source, filename);
        auto tokens = lexer.tokenize_all();

        for (const auto& tok : tokens) {
            if (tok.kind == TokenKind::Error) {
                result.errors.push_back(std::format("{}:{}:{}: lexer error: {}", 
                    tok.loc.file, tok.loc.line, tok.loc.column, tok.text));
            }
        }
        if (!result.errors.empty()) {
            return result;
        }

        // cstyle as base, mixed detection happens per statement
        Parser parser(std::move(tokens), SyntaxMode::CStyle);
        auto mod_result = parser.parse_module(filename);
        
        if (!mod_result) {
            for (const auto& err : mod_result.error()) {
                result.errors.push_back(err.to_string());
            }
            return result;
        }

        ast::Module& mod = *mod_result;

        CodeGenerator codegen;
        auto& rt = get_runtime();
        codegen.set_runtime_pointers(
            rt.print_int, rt.print_str, rt.read_file,
            rt.string_length, rt.string_get_char, rt.print_string,
            rt.make_string, rt.write_file, rt.malloc_fn, rt.free_fn,
            rt.array_new, rt.array_get, rt.array_set, rt.array_len,
            rt.array_free, rt.string_append, rt.int_to_string, rt.print_char,
            // self-hosting helpers
            rt.string_equals, rt.string_substring, rt.is_alpha, rt.is_digit,
            rt.is_alnum, rt.is_whitespace, rt.string_starts_with, rt.exit_fn,
            rt.write_bytes, rt.buffer_new, rt.buffer_push, rt.buffer_len, rt.parse_int);
        
        codegen.set_dll_mode(dll_mode || exe_mode);
        
        codegen.set_source_path(std::string(filename));
        
        // pe layout needs to know startup code size so codegen offsets are correct
        if (dll_mode || exe_mode) {
            bool debug_mode = healing_mode != "off";
            auto layout = pe::DllGenerator::compute_layout(debug_mode, exe_mode);
            codegen.set_user_code_offset(layout.startup_code_size);
            if (exe_mode) codegen.set_exe_mode();
        }
        
        if (!codegen.generate(mod)) {
            for (const auto& err : codegen.errors()) {
                result.errors.push_back(err);
            }
            return result;
        }

        const auto& buf = codegen.emitter().buffer();
        result.code.assign(buf.data(), buf.data() + buf.size());

        for (const auto& [name, info] : codegen.functions()) {
            result.function_offsets[name] = info.code_offset;
        }
        
        if (dll_mode || exe_mode) {
            result.iat_fixups = codegen.get_iat_fixups();
            result.line_map = codegen.get_line_map();
            result.needs_writable_text = codegen.has_global_slot();
        }

        result.success = true;
        return result;
    }

    std::expected<std::int64_t, std::string> compile_and_run(std::string_view source) {
        auto result = compile(source);
        
        if (!result.success) {
            std::string errors;
            for (const auto& e : result.errors) {
                errors += e + "\n";
            }
            return std::unexpected(errors);
        }

        auto it = result.function_offsets.find("main");
        if (it == result.function_offsets.end()) {
            return std::unexpected("no main function found");
        }

        #ifdef _WIN32
        x64::ExecutableMemory exec(result.code.size());
        std::memcpy(exec.ptr(), result.code.data(), result.code.size());

        using MainFn = std::int64_t(*)();
        auto main_fn = reinterpret_cast<MainFn>(
            static_cast<std::uint8_t*>(exec.ptr()) + it->second
        );
        
        return main_fn();
        #else
        return std::unexpected("execution not supported on this platform");
        #endif
    }
};

// repl

class REPL {
public:
    void run() {
        std::println("Opus REPL v0.1");
        std::println("Type :help for help, :quit to exit");
        std::println("");

        std::string line;
        while (true) {
            std::print("opus> ");
            if (!std::getline(std::cin, line)) break;
            
            if (line.empty()) continue;
            
            if (line[0] == ':') {
                if (line == ":quit" || line == ":q") break;
                if (line == ":help" || line == ":h") {
                    print_help();
                    continue;
                }
                if (line == ":clear" || line == ":c") {
                    std::println("\033[2J\033[H");
                    continue;
                }
                std::println("Unknown command: {}", line);
                continue;
            }

            // wrap in main() so expressions can be evaluated directly
            std::string wrapped = std::format("fn main() -> i64 {{ return {}; }}", line);
            
            auto result = compiler_.compile_and_run(wrapped);
            if (result) {
                std::println("=> {}", *result);
            } else {
                std::println("Error: {}", result.error());
            }
        }
    }

private:
    Compiler compiler_;

    void print_help() {
        std::println("Commands:");
        std::println("  :help, :h   - Show this help");
        std::println("  :quit, :q   - Exit REPL");
        std::println("  :clear, :c  - Clear screen");
        std::println("");
        std::println("Enter any expression to evaluate it.");
        std::println("Examples:");
        std::println("  1 + 2 * 3");
        std::println("  42");
        std::println("  (10 + 5) / 3");
    }
};

} // namespace opus

