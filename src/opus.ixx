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
export import opus.pe;
export import opus.errors;

import std;

export namespace opus {

// runtime builtins

// string table is shared by compiler-side runtime helpers
inline std::mutex& get_string_table_mutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::vector<std::string>& get_string_table() {
    static std::vector<std::string> table;
    return table;
}

inline void reset_runtime_state() {
    std::lock_guard lock(get_string_table_mutex());
    get_string_table().clear();
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
    std::string path;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (filename_handle < 0 || filename_handle >= static_cast<std::int64_t>(table.size())) {
            return -1;
        }
        path = table[filename_handle];
    }
    
    std::ifstream file(path);
    if (!file) {
        return -1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::lock_guard lock(get_string_table_mutex());
    auto& table = get_string_table();
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(buffer.str());
    return handle;
}

extern "C" std::int64_t opus_make_string(const char* str) {
    if (!str) return -1;
    std::lock_guard lock(get_string_table_mutex());
    auto& table = get_string_table();
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(str);
    return handle;
}

extern "C" std::int64_t opus_string_length(std::int64_t handle) {
    std::lock_guard lock(get_string_table_mutex());
    auto& table = get_string_table();
    if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) {
        return 0;
    }
    return static_cast<std::int64_t>(table[handle].size());
}

extern "C" std::int64_t opus_string_get_char(std::int64_t handle, std::int64_t index) {
    std::lock_guard lock(get_string_table_mutex());
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
    std::string value;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (handle < 0 || handle >= static_cast<std::int64_t>(table.size())) {
            return;
        }
        value = table[handle];
    }
    std::print("{}", value);
}

extern "C" std::int64_t opus_write_file(std::int64_t filename_handle, std::int64_t content_handle) {
    std::string path;
    std::string content;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (filename_handle < 0 || filename_handle >= static_cast<std::int64_t>(table.size())) {
            return -1;
        }
        if (content_handle < 0 || content_handle >= static_cast<std::int64_t>(table.size())) {
            return -1;
        }
        path = table[filename_handle];
        content = table[content_handle];
    }
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return -1;
    }
    file.write(content.data(), content.size());
    return static_cast<std::int64_t>(content.size());
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
    if (dest == 0 || src == 0 || size <= 0) return;
    std::memcpy(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), static_cast<std::size_t>(size));
}

extern "C" void opus_mem_set(std::int64_t dest, std::int64_t value, std::int64_t size) {
    if (dest == 0 || size <= 0) return;
    std::memset(reinterpret_cast<void*>(dest), static_cast<int>(value), static_cast<std::size_t>(size));
}

// ffi - windows api

#ifdef _WIN32

extern "C" std::int64_t opus_get_module(std::int64_t name_handle) {
    std::string name;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
            return reinterpret_cast<std::int64_t>(GetModuleHandleA(nullptr));
        }
        name = table[name_handle];
    }
    HMODULE mod = GetModuleHandleA(name.c_str());
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_get_module_str(const char* name) {
    HMODULE mod = GetModuleHandleA(name);
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_load_library(std::int64_t name_handle) {
    std::string name;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
            return 0;
        }
        name = table[name_handle];
    }
    HMODULE mod = LoadLibraryA(name.c_str());
    return reinterpret_cast<std::int64_t>(mod);
}

extern "C" std::int64_t opus_get_proc(std::int64_t module, std::int64_t name_handle) {
    std::string name;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (name_handle < 0 || name_handle >= static_cast<std::int64_t>(table.size())) {
            return 0;
        }
        name = table[name_handle];
    }
    FARPROC proc = GetProcAddress(
        reinterpret_cast<HMODULE>(module),
        name.c_str()
    );
    return reinterpret_cast<std::int64_t>(proc);
}

extern "C" std::int64_t opus_get_last_error() {
    return static_cast<std::int64_t>(GetLastError());
}

// need a console for dll debugging since dlls dont have one
extern "C" std::int64_t opus_alloc_console() {
    if (AllocConsole()) {
        FILE* fp = nullptr;
        if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0) return 0;
        if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0) return 0;
        if (freopen_s(&fp, "CONIN$", "r", stdin) != 0) return 0;
        SetConsoleTitleA("Opus Console");
        
        return 1;
    }
    return 0;
}

extern "C" void opus_free_console() {
    FreeConsole();
}

extern "C" void opus_set_console_title(std::int64_t title_handle) {
    std::string title;
    {
        std::lock_guard lock(get_string_table_mutex());
        auto& table = get_string_table();
        if (title_handle < 0 || title_handle >= static_cast<std::int64_t>(table.size())) {
            return;
        }
        title = table[title_handle];
    }
    SetConsoleTitleA(title.c_str());
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
extern "C" std::int64_t opus_get_last_error() { return 0; }
extern "C" std::int64_t opus_virtual_protect(std::int64_t address, std::int64_t size, std::int64_t new_protect) { return -1; }
extern "C" std::int64_t opus_read_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) { return -1; }
extern "C" std::int64_t opus_write_process_memory(std::int64_t process, std::int64_t address, std::int64_t buffer, std::int64_t size) { return -1; }
extern "C" std::int64_t opus_get_current_process() { return 0; }
extern "C" std::int64_t opus_get_current_process_id() { return 0; }
#endif

// layout: arr[0] = length, arr[1..n] = elements
extern "C" std::int64_t opus_array_new(std::int64_t size) {
    if (size < 0) return 0;
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
    std::lock_guard lock(get_string_table_mutex());
    auto& table = get_string_table();
    if (handle1 < 0 || handle1 >= static_cast<std::int64_t>(table.size())) return -1;
    if (handle2 < 0 || handle2 >= static_cast<std::int64_t>(table.size())) return -1;
    
    std::int64_t new_handle = static_cast<std::int64_t>(table.size());
    // build the string before push_back to avoid iterator invalidation
    auto new_str = table[handle1] + table[handle2];
    table.push_back(std::move(new_str));
    return new_handle;
}

extern "C" std::int64_t opus_int_to_string(std::int64_t value) {
    std::lock_guard lock(get_string_table_mutex());
    auto& table = get_string_table();
    std::int64_t handle = static_cast<std::int64_t>(table.size());
    table.push_back(std::to_string(value));
    return handle;
}

extern "C" void opus_print_char(std::int64_t ch) {
    std::print("{}", static_cast<char>(ch));
}

// self-hosting helpers

// byte buffer ops for x64 codegen
// layout: arr[-1] = capacity, arr[0] = length, arr[1..n] = data
// this is compatible with the regular array layout from arr[0] onward,
// so array_get/array_set/array_len/write_bytes all still work
// runtime functions used by compiler-side lowering
struct RuntimeFunctions {
    // core i/o
    RtVoidI64   print_int = &opus_print_int;
    RtVoidStr   print_str = &opus_print_str;
    RtVoid      print_newline = &opus_print_newline;
    RtI64I64    read_file = &opus_read_file;
    RtI64I64    string_length = &opus_string_length;
    RtI64I64I64 string_get_char = &opus_string_get_char;
    RtVoidI64   print_string = &opus_print_string;
    RtI64Str    make_string = &opus_make_string;
    RtI64I64I64 write_file = &opus_write_file;

    // memory
    RtI64I64    malloc_fn = &opus_malloc;
    RtVoidI64   free_fn = &opus_free;

    // arrays
    RtI64I64    array_new = &opus_array_new;
    RtI64I64I64 array_get = &opus_array_get;
    RtVoidI64I64I64 array_set = &opus_array_set;
    RtI64I64    array_len = &opus_array_len;
    RtVoidI64   array_free = &opus_array_free;

    // strings
    RtI64I64I64 string_append = &opus_string_append;
    RtI64I64    int_to_string = &opus_int_to_string;
    RtVoidI64   print_char = &opus_print_char;

    // ffi
    RtI64I64    get_module = &opus_get_module;
    RtI64I64    load_library = &opus_load_library;
    RtI64I64I64 get_proc = &opus_get_proc;
    std::int64_t(*get_last_error)() = &opus_get_last_error;
    RtI64I64I64I64 virtual_protect = &opus_virtual_protect;
    std::int64_t(*get_current_process)() = &opus_get_current_process;
    std::int64_t(*get_current_process_id)() = &opus_get_current_process_id;
};

// meyers singleton so this works across module boundaries
inline RuntimeFunctions& get_runtime() {
    static RuntimeFunctions instance;
    return instance;
}

// compiler

struct CompileOptions {
    std::string_view source;
    std::string_view filename = "<input>";
    std::string project_root;
    std::vector<std::string> import_search_paths;
    OutputKind output_kind = OutputKind::Raw;
    ast::HealingMode healing_mode = ast::HealingMode::Off;
    bool debug_info = false;
};

class Compiler {
public:
    struct Result {
        bool success = false;
        std::vector<std::string> errors;
        std::vector<std::uint8_t> code;
        std::unordered_map<std::string, std::size_t> function_offsets;
        
        // iat fixups for pe generation
        std::vector<pe::IatFixup> iat_fixups;
        
        // instruction offset -> source line mapping
        std::vector<pe::LineMapEntry> line_map;
        
        // set when code has a writable slot in .text for globals base ptr
        bool needs_writable_text = false;
    };

    Compiler() = default;

    static RuntimeFunctions& runtime() { return get_runtime(); }

    Result compile(const CompileOptions& opts) {
        auto source = opts.source;
        auto filename = opts.filename;
        auto project_root = opts.project_root;
        auto import_search_paths = opts.import_search_paths;
        auto output_kind = opts.output_kind;
        auto healing_mode = opts.healing_mode;
        [[maybe_unused]] auto debug_info = opts.debug_info;
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
        codegen.set_runtime_pointers(RuntimePointers{
            .print_int = rt.print_int,
            .print_str = rt.print_str,
            .print_newline = rt.print_newline,
            .read_file = rt.read_file,
            .string_length = rt.string_length,
            .string_get_char = rt.string_get_char,
            .print_string = rt.print_string,
            .make_string = rt.make_string,
            .write_file = rt.write_file,
            .malloc_fn = rt.malloc_fn,
            .free_fn = rt.free_fn,
            .array_new = rt.array_new,
            .array_get = rt.array_get,
            .array_set = rt.array_set,
            .array_len = rt.array_len,
            .array_free = rt.array_free,
            .string_append = rt.string_append,
            .int_to_string = rt.int_to_string,
            .print_char = rt.print_char,
            .get_module = rt.get_module,
            .load_library = rt.load_library,
            .get_proc = rt.get_proc,
            .get_last_error = rt.get_last_error,
            .virtual_protect = rt.virtual_protect,
            .get_current_process = rt.get_current_process,
            .get_current_process_id = rt.get_current_process_id,
        });
        
        codegen.set_output_kind(output_kind);
        
        codegen.set_source_path(std::string(filename));
        codegen.set_project_root(project_root);
        codegen.set_import_search_paths(import_search_paths);
        
        // pe layout needs to know startup code size so codegen offsets are correct
        if (output_is_native_image(output_kind)) {
            bool debug_mode = healing_mode != ast::HealingMode::Off;
            auto layout = pe::PeImageGenerator::compute_layout(debug_mode, output_kind);
            codegen.set_user_code_offset(layout.startup_code_size);
        }
        
        if (!codegen.generate(mod)) {
            for (const auto& err : codegen.errors()) {
                result.errors.push_back(err);
            }
            return result;
        }

        const auto& buf = codegen.emitter().buffer();
        result.code.assign(buf.data(), buf.data() + buf.pos());

        for (const auto& [name, info] : codegen.functions()) {
            result.function_offsets[name] = info.code_offset;
        }
        
        if (output_is_native_image(output_kind)) {
            result.iat_fixups = codegen.get_iat_fixups();
            result.line_map = codegen.get_line_map();
            result.needs_writable_text = codegen.has_global_slot();
        }

        result.success = true;
        return result;
    }

    std::expected<std::int64_t, std::string> compile_and_run(const CompileOptions& opts) {
        reset_runtime_state();
        auto run_opts = opts;
        run_opts.output_kind = OutputKind::Exe;
        auto result = compile(run_opts);
        
        if (!result.success) {
            std::string errors;
            for (const auto& e : result.errors) {
                errors += e + "\n";
            }
            if (errors.empty()) {
                errors = "compilation failed without a diagnostic";
            }
            return std::unexpected(errors);
        }

        auto it = result.function_offsets.find("main");
        if (it == result.function_offsets.end()) {
            return std::unexpected("no main function found");
        }

        #ifdef _WIN32
        pe::PeImageGenerator exe_gen;
        auto exe_bytes = exe_gen.generate({
            .code = result.code,
            .main_offset = it->second,
            .alloc_console = false,
            .iat_fixups = result.iat_fixups,
            .debug_source = opts.debug_info ? std::string(opts.source) : std::string{},
            .line_map = opts.debug_info ? result.line_map : std::vector<pe::LineMapEntry>{},
            .healing_mode = opts.healing_mode,
            .output_kind = OutputKind::Exe,
            .writable_text = result.needs_writable_text,
        });

        auto temp_dir = std::filesystem::temp_directory_path();
        char temp_name[MAX_PATH]{};
        if (GetTempFileNameA(temp_dir.string().c_str(), "opu", 0, temp_name) == 0) {
            return std::unexpected("failed to create temp file for --run");
        }

        auto temp_path = std::filesystem::path(temp_name);
        auto exe_path = temp_path;
        exe_path.replace_extension(".exe");

        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        std::filesystem::remove(exe_path, ec);

        {
            std::ofstream out(exe_path, std::ios::binary);
            if (!out) {
                return std::unexpected(std::format("cannot write temp executable: {}", exe_path.string()));
            }
            out.write(reinterpret_cast<const char*>(exe_bytes.data()), static_cast<std::streamsize>(exe_bytes.size()));
            out.close();
            if (!out) {
                return std::unexpected(std::format("failed writing temp executable: {}", exe_path.string()));
            }
        }

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE child_out_read = nullptr;
        HANDLE child_out_write = nullptr;
        if (!CreatePipe(&child_out_read, &child_out_write, &sa, 0)) {
            std::filesystem::remove(exe_path, ec);
            return std::unexpected("failed to create output pipe for --run");
        }
        SetHandleInformation(child_out_read, HANDLE_FLAG_INHERIT, 0);

        std::string command_line = std::format("\"{}\"", exe_path.string());
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = child_out_write;
        si.hStdError = child_out_write;

        PROCESS_INFORMATION pi{};
        BOOL launched = CreateProcessA(
            exe_path.string().c_str(),
            command_line.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!launched) {
            auto err = GetLastError();
            CloseHandle(child_out_read);
            CloseHandle(child_out_write);
            std::filesystem::remove(exe_path, ec);
            return std::unexpected(std::format(
                "failed to launch temp executable (error {})",
                static_cast<unsigned long>(err)
            ));
        }

        CloseHandle(child_out_write);

        std::thread output_pump([child_out_read]() {
            char buffer[4096];
            DWORD bytes_read = 0;
            while (ReadFile(child_out_read, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
                std::cout.write(buffer, static_cast<std::streamsize>(bytes_read));
                std::cout.flush();
            }
            CloseHandle(child_out_read);
        });

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
            auto err = GetLastError();
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            output_pump.join();
            std::filesystem::remove(exe_path, ec);
            return std::unexpected(std::format(
                "failed to read temp executable exit code (error {})",
                static_cast<unsigned long>(err)
            ));
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        output_pump.join();
        std::filesystem::remove(exe_path, ec);

        return static_cast<std::int64_t>(exit_code);
        #else
        return std::unexpected("execution not supported on this platform");
        #endif
    }

    std::expected<std::int64_t, std::string> compile_and_run(std::string_view source) {
        return compile_and_run({.source = source});
    }
};

// repl

class Repl {
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
