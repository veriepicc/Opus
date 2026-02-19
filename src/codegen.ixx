// codegen - AST to x64

export module opus.codegen;

import opus.types;
import opus.ast;
import opus.x64;
import opus.pe;
import opus.lexer;
import opus.parser;
import std;

extern "C" {
    std::int64_t opus_get_module(std::int64_t name_handle);
    std::int64_t opus_load_library(std::int64_t name_handle);
    std::int64_t opus_get_proc(std::int64_t module, std::int64_t name_handle);
    std::int64_t opus_ffi_call0(std::int64_t fn_ptr);
    std::int64_t opus_ffi_call1(std::int64_t fn_ptr, std::int64_t a1);
    std::int64_t opus_ffi_call2(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2);
    std::int64_t opus_ffi_call3(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3);
    std::int64_t opus_ffi_call4(std::int64_t fn_ptr, std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4);
    std::int64_t opus_msgbox(std::int64_t title_handle, std::int64_t text_handle, std::int64_t flags);
    std::int64_t opus_get_last_error();
    std::int64_t opus_virtual_protect(std::int64_t address, std::int64_t size, std::int64_t new_protect);
    std::int64_t opus_get_current_process();
    std::int64_t opus_get_current_process_id();
    std::int64_t opus_alloc_console();
    void opus_free_console();
    void opus_set_console_title(std::int64_t title_handle);
}

export namespace opus {

// visitor helper for std::visit dispatch
template<typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// typed runtime function pointer signatures
using RtVoidI64     = void(*)(std::int64_t);
using RtVoidStr     = void(*)(const char*);
using RtVoid        = void(*)();
using RtI64I64      = std::int64_t(*)(std::int64_t);
using RtI64I64I64   = std::int64_t(*)(std::int64_t, std::int64_t);
using RtI64I64I64I64 = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t);
using RtI64Str      = std::int64_t(*)(const char*);
using RtVoidI64I64  = void(*)(std::int64_t, std::int64_t);
using RtVoidI64I64I64 = void(*)(std::int64_t, std::int64_t, std::int64_t);
using RtVoidI64Dbl  = void(*)(std::int64_t, double);
using RtDblI64      = double(*)(std::int64_t);
using RtI64I64I64I64I64 = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t);
using RtI64I64I64I64I64I64 = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t);
using RtI64I64I64I64I64I64I64 = std::int64_t(*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t);

// all runtime function pointers passed from the host into codegen
// typed so argument ordering bugs are caught at compile time
struct RuntimePointers {
    // core i/o
    RtVoidI64   print_int = nullptr;
    RtVoidStr   print_str = nullptr;
    RtVoid      print_newline = nullptr;
    RtI64I64    read_file = nullptr;
    RtI64I64    string_length = nullptr;
    RtI64I64I64 string_get_char = nullptr;
    RtVoidI64   print_string = nullptr;
    RtI64Str    make_string = nullptr;
    RtI64I64I64 write_file = nullptr;

    // memory
    RtI64I64    malloc_fn = nullptr;
    RtVoidI64   free_fn = nullptr;

    // arrays
    RtI64I64    array_new = nullptr;
    RtI64I64I64 array_get = nullptr;
    RtVoidI64I64I64 array_set = nullptr;
    RtI64I64    array_len = nullptr;
    RtVoidI64   array_free = nullptr;

    // strings
    RtI64I64I64 string_append = nullptr;
    RtI64I64    int_to_string = nullptr;
    RtVoidI64   print_char = nullptr;

    // self-hosting helpers
    RtI64I64I64 string_equals = nullptr;
    RtI64I64I64I64 string_substring = nullptr;
    RtI64I64    is_alpha = nullptr;
    RtI64I64    is_digit = nullptr;
    RtI64I64    is_alnum = nullptr;
    RtI64I64    is_whitespace = nullptr;
    RtI64I64I64 string_starts_with = nullptr;
    RtVoidI64   exit_fn = nullptr;
    RtI64I64I64I64 write_bytes = nullptr;
    RtI64I64    buffer_new = nullptr;
    RtVoidI64I64 buffer_push = nullptr;
    RtI64I64    buffer_len = nullptr;
    RtI64I64    parse_int = nullptr;

    // raw memory access
    RtI64I64    mem_read_i8 = nullptr;
    RtI64I64    mem_read_i16 = nullptr;
    RtI64I64    mem_read_i32 = nullptr;
    RtI64I64    mem_read_i64 = nullptr;
    RtDblI64    mem_read_f32 = nullptr;
    RtDblI64    mem_read_f64 = nullptr;
    RtI64I64    mem_read_ptr = nullptr;
    RtVoidI64I64 mem_write_i8 = nullptr;
    RtVoidI64I64 mem_write_i16 = nullptr;
    RtVoidI64I64 mem_write_i32 = nullptr;
    RtVoidI64I64 mem_write_i64 = nullptr;
    RtVoidI64Dbl mem_write_f32 = nullptr;
    RtVoidI64Dbl mem_write_f64 = nullptr;
    RtVoidI64I64 mem_write_ptr = nullptr;
    RtVoidI64I64I64 mem_copy = nullptr;
    RtVoidI64I64I64 mem_set = nullptr;

    // ffi
    RtI64I64    get_module = nullptr;
    RtI64I64    load_library = nullptr;
    RtI64I64I64 get_proc = nullptr;
    RtI64I64    ffi_call0 = nullptr;
    RtI64I64I64 ffi_call1 = nullptr;
    RtI64I64I64I64 ffi_call2 = nullptr;
    RtI64I64I64I64I64 ffi_call3 = nullptr;
    RtI64I64I64I64I64I64 ffi_call4 = nullptr;
    RtI64I64I64I64I64I64I64 ffi_call5 = nullptr;
    // ffi_call6 has 7 args
    std::int64_t(*ffi_call6)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t) = nullptr;
    RtI64I64I64I64 msgbox = nullptr;
    std::int64_t(*get_last_error)() = nullptr;
    RtI64I64I64I64 virtual_protect = nullptr;
    std::int64_t(*get_current_process)() = nullptr;
    std::int64_t(*get_current_process_id)() = nullptr;
};

// convert any function pointer to void* for the emitter
template<typename F>
void* as_void(F fn) { return reinterpret_cast<void*>(fn); }

struct Symbol {
    std::string name;
    Type type;
    std::int32_t stack_offset;  // Offset from RBP (negative = local)
    bool is_param = false;
    bool is_mut = false;
};

struct Scope {
    std::unordered_map<std::string, Symbol> symbols;
    Scope* parent = nullptr;
    std::int32_t next_offset = -8;  // Start at RBP-8

    Symbol* lookup(const std::string& name) {
        if (auto it = symbols.find(name); it != symbols.end()) {
            return &it->second;
        }
        if (parent) return parent->lookup(name);
        return nullptr;
    }

    Symbol& define(const std::string& name, Type type, bool is_mut = false) {
        Symbol sym{
            .name = name,
            .type = std::move(type),
            .stack_offset = next_offset,
            .is_mut = is_mut
        };
        next_offset -= 8;
        auto [it, _] = symbols.emplace(name, std::move(sym));
        return it->second;
    }
};

struct FunctionInfo {
    std::string name;
    std::size_t code_offset;
    std::size_t code_size;
    Type return_type;
    std::vector<Type> param_types;
};

class CodeGenerator {
public:
    CodeGenerator() = default;

    // Enable DLL mode (changes how builtins are generated)
    void set_dll_mode(bool enabled) { dll_mode_ = enabled; }
    bool is_dll_mode() const { return dll_mode_; }

    // set source file path for import resolution
    void set_source_path(const std::string& path) { source_path_ = path; }

    // set the user code offset for rip-relative calculations
    // defaults to debug layout for backward compat
    void set_user_code_offset(std::size_t offset) { user_code_offset_ = offset; }
    std::size_t user_code_offset() const { return user_code_offset_; }

    // switch runtime routine offsets to exe layout
    void set_exe_mode() {
        print_offset_ = pe::DllGenerator::EXE_PRINT_OFFSET;
        set_title_offset_ = pe::DllGenerator::EXE_SET_TITLE_OFFSET;
        alloc_console_offset_ = pe::DllGenerator::EXE_ALLOC_CONSOLE_OFFSET;
        print_hex_offset_ = pe::DllGenerator::EXE_PRINT_HEX_OFFSET;
        crash_handler_offset_ = pe::DllGenerator::EXE_CRASH_HANDLER_OFFSET;
    }

    // enable auto-parallelization of for loops
    void set_auto_parallel(bool enabled) { auto_parallel_ = enabled; }
    bool is_auto_parallel() const { return auto_parallel_; }

    void set_runtime_pointers(const RuntimePointers& rt) {
        rt_ = rt;
    }

    // Generate code for a module
    [[nodiscard]] bool generate(const ast::Module& mod) {
        module_name_ = mod.name;

        // first pass: collect function signatures and globals
        for (const auto& decl : mod.decls) {
            if (decl->is<ast::FnDecl>()) {
                const auto& fn = decl->as<ast::FnDecl>();
                functions_[fn.name] = FunctionInfo{
                    .name = fn.name,
                    .return_type = fn.return_type.clone()
                };
            }
            // collect globals early so they're available in functions
            if (decl->is<ast::StaticDecl>()) {
                register_static_decl(decl->as<ast::StaticDecl>());
            }
        }

        // if we have globals, reserve 8 bytes at the start of the code buffer
        // for the global base pointer (rip-relative storage)
        // emit a jmp over it so execution doesnt hit the data bytes
        if (!globals_.empty()) {
            std::size_t jmp_over = emit_.jmp_rel32_placeholder();
            global_base_slot_ = emit_.buffer().pos();
            has_global_slot_ = true;
            emit_.buffer().emit64(0); // 8 bytes for global base ptr
            emit_.patch_jump(jmp_over);
        }

        // Second pass: generate code
        for (const auto& decl : mod.decls) {
            if (!generate_decl(*decl)) {
                return false;
            }
        }
        
        // generate __opus_init if we have globals
        if (!globals_.empty()) {
            generate_global_init();
        }
        
        // Third pass: patch call fixups (for forward references)
        for (const auto& fixup : call_fixups_) {
            auto it = functions_.find(fixup.target_fn);
            if (it == functions_.end()) {
                error(std::format("undefined function in fixup: {}", fixup.target_fn));
                continue;
            }
            // rel32 fixup: target - (call_site + 4)
            std::int32_t offset = static_cast<std::int32_t>(it->second.code_offset) -
                                  static_cast<std::int32_t>(fixup.call_site + 4);
            emit_.buffer().patch32(fixup.call_site, static_cast<std::uint32_t>(offset));
        }
        call_fixups_.clear();

        return errors_.empty();
    }

    x64::Emitter& emitter() { return emit_; }
    const std::vector<std::string>& errors() const { return errors_; }
    
    const std::unordered_map<std::string, FunctionInfo>& functions() const {
        return functions_;
    }
    
    // Patch IAT references once RDATA_RVA is known (called by PE builder)
    // text_rva: RVA of .text section (usually 0x1000)
    // rdata_rva: RVA of .rdata section (e.g., 0x2000 or 0x3000)
    // user_code_offset: offset of user code within .text (e.g., 0x240)
    void patch_iat_fixups(std::size_t text_rva, std::size_t rdata_rva, std::size_t user_code_offset) {
        auto* buf = emit_.buffer().data();
        for (const auto& fixup : iat_fixups_) {
            // IAT entry absolute RVA
            std::size_t iat_rva = rdata_rva + fixup.iat_offset;
            // Code position absolute RVA (patch_site is in user code buffer, add offset + text_rva)
            std::size_t code_rva = text_rva + user_code_offset + fixup.patch_site + 4;  // +4 for the disp32 we're patching
            // RIP-relative: target - rip
            std::int32_t disp = static_cast<std::int32_t>(iat_rva) - static_cast<std::int32_t>(code_rva);
            
            buf[fixup.patch_site + 0] = static_cast<std::uint8_t>(disp & 0xFF);
            buf[fixup.patch_site + 1] = static_cast<std::uint8_t>((disp >> 8) & 0xFF);
            buf[fixup.patch_site + 2] = static_cast<std::uint8_t>((disp >> 16) & 0xFF);
            buf[fixup.patch_site + 3] = static_cast<std::uint8_t>((disp >> 24) & 0xFF);
        }
    }
    
    bool has_iat_fixups() const { return !iat_fixups_.empty(); }
    bool has_global_slot() const { return has_global_slot_; }
    
    // return fixups using the pe named struct
    std::vector<pe::IatFixup> get_iat_fixups() const {
        std::vector<pe::IatFixup> result;
        for (const auto& f : iat_fixups_) {
            result.push_back({f.patch_site, f.iat_offset});
        }
        return result;
    }
    
    // return line map using the pe named struct
    std::vector<pe::LineMapEntry> get_line_map() const {
        std::vector<pe::LineMapEntry> result;
        for (const auto& e : line_map_) {
            result.push_back({static_cast<std::uint32_t>(e.offset), e.line});
        }
        return result;
    }

private:
    // parallel for threading constants
    static constexpr int PFOR_MAX_THREADS = 64;     // max worker threads we will ever spawn
    static constexpr int PFOR_CTX_SIZE = 48;         // bytes per thread context (fn_ptr, start, end, parent_rbp, result, pad)
    static constexpr int PFOR_CTX_EXTRA_SLOTS = 447; // 8-byte stack slots for context array (define() takes 1, loop takes rest)
    static constexpr int PFOR_HDL_EXTRA_SLOTS = 63;  // 8-byte stack slots for handle array (define() takes 1, loop takes rest)

    // thread context layout offsets (shared by spawn, await, and parallel for)
    static constexpr std::int32_t CTX_FN_PTR  = 0x00;
    static constexpr std::int32_t CTX_ARG0    = 0x08;
    static constexpr std::int32_t CTX_ARG1    = 0x10;
    static constexpr std::int32_t CTX_ARG2    = 0x18;
    static constexpr std::int32_t CTX_RESULT  = 0x20;
    static constexpr std::int32_t CTX_SIZE    = 0x28;

    // shared state between pfor helper functions so we dont pass 15 args around
    struct PforLayout {
        std::int32_t start_off;    // __pfor_start stack offset
        std::int32_t end_off;      // __pfor_end stack offset
        std::int32_t ncores_off;   // __pfor_ncores stack offset
        std::int32_t range_off;    // __pfor_range stack offset
        std::int32_t chunk_off;    // __pfor_chunk stack offset
        std::int32_t rem_off;      // __pfor_rem stack offset (division remainder)
        std::int32_t idx_off;      // __pfor_i stack offset
        std::int32_t ctx_arr_base; // bottom of context array on stack
        std::int32_t hdl_arr_base; // bottom of handle array on stack
        std::string body_fn;       // name of the emitted body function
        std::size_t stub_offset;   // offset of the thread entry stub
    };

    x64::Emitter emit_;
    std::string module_name_;
    std::string source_path_;
    std::unordered_set<std::string> imported_modules_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::vector<std::string> errors_;
    
    Scope* current_scope_ = nullptr;
    std::vector<std::unique_ptr<Scope>> scopes_;
    
    // for loop break/continue - both use patch lists
    std::vector<std::vector<std::size_t>> break_patches_;
    std::vector<std::vector<std::size_t>> continue_patches_;
    
    // forward reference fixups
    struct CallFixup {
        std::size_t call_site;  // where the rel32 lives
        std::string target_fn;  // what function its calling
    };
    std::vector<CallFixup> call_fixups_;
    
    // iat fixups - patched once we know where .data lands
    struct IATFixup {
        std::size_t patch_site;  // disp32 position in code buffer
        std::size_t iat_offset;  // slot in IAT (pe::iat::*)
    };
    std::vector<IATFixup> iat_fixups_;
    
    // emit just the call [rip+disp32] + register fixup (no stack alignment)
    void emit_iat_call_raw(std::size_t iat_offset) {
        emit_.buffer().emit8(0xFF);
        emit_.buffer().emit8(0x15);
        std::size_t patch_site = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        iat_fixups_.push_back({patch_site, iat_offset});
    }
    
    // emit sub rsp,32 + call [rip+disp32] + fixup + add rsp,32
    void emit_iat_call(std::size_t iat_offset) {
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(iat_offset);
        emit_.add_imm(x64::Reg::RSP, 32);
    }
    
    // emit E8 rel32 call to a startup routine (print, set_title, alloc_console, etc)
    // target_offset is the routine offset in .text (e.g. DLL_PRINT_OFFSET)
    void emit_startup_call(std::size_t target_offset) {
        std::size_t current = emit_.buffer().pos();
        std::int32_t rel = static_cast<std::int32_t>(target_offset)
            - static_cast<std::int32_t>(user_code_offset_ + current + 5);
        emit_.buffer().emit8(0xE8);
        emit_.buffer().emit32(static_cast<std::uint32_t>(rel));
    }
    
    // Struct type info
    struct StructInfo {
        std::string name;
        std::vector<std::pair<std::string, Type>> fields;
        std::unordered_map<std::string, std::size_t> field_offsets;
        std::size_t total_size = 0;
        
        void calculate_offsets() {
            std::size_t offset = 0;
            for (const auto& [fname, type] : fields) {
                field_offsets[fname] = offset;
                // all fields are 8 bytes since codegen uses 64-bit mov for everything
                offset += 8;
            }
            total_size = offset;
        }
        
        std::optional<std::size_t> get_field_offset(const std::string& fname) const {
            auto it = field_offsets.find(fname);
            if (it != field_offsets.end()) return it->second;
            return std::nullopt;
        }
    };
    std::unordered_map<std::string, StructInfo> structs_;
    
    // Current class context (for self.field resolution in methods)
    std::string current_class_name_;
    
    // Enum definitions (EnumName -> (VariantName -> Value))
    std::unordered_map<std::string, std::unordered_map<std::string, std::int64_t>> enums_;
    
    // Global variables
    struct GlobalVar {
        std::string name;
        Type type;
        bool is_mut = false;
        std::size_t offset = 0;  // offset in global data section
        const ast::Expr* init = nullptr;  // initializer (may call functions)
    };
    std::unordered_map<std::string, GlobalVar> globals_;
    std::vector<std::string> global_order_;  // tracks insertion order for deterministic init
    std::size_t next_global_offset_ = 0;
    std::size_t global_data_base_ = 0;  // set at runtime/linking
    
    // Line number mapping for debug (instruction_offset -> source_line)
    struct LineMapEntry {
        std::uint32_t offset;
        std::uint32_t line;
    };
    std::vector<LineMapEntry> line_map_;
    
    // String literals (embedded in data section)
    std::vector<std::string> string_literals_;
    std::unordered_map<std::string, std::size_t> string_indices_;
    
    // runtime function pointers from host
    RuntimePointers rt_;

    // DLL mode flag - when true, generates calls to embedded runtime instead of absolute addresses
    bool dll_mode_ = false;
    std::size_t user_code_offset_ = pe::DllGenerator::STARTUP_CODE_SIZE;  // default to debug layout
    
    // runtime routine offsets in .text - set based on exe/dll mode
    std::size_t print_offset_ = pe::DllGenerator::DLL_PRINT_OFFSET;
    std::size_t set_title_offset_ = pe::DllGenerator::DLL_SET_TITLE_OFFSET;
    std::size_t alloc_console_offset_ = pe::DllGenerator::DLL_ALLOC_CONSOLE_OFFSET;
    std::size_t print_hex_offset_ = pe::DllGenerator::DLL_PRINT_HEX_OFFSET;
    std::size_t crash_handler_offset_ = pe::DllGenerator::CRASH_HANDLER_OFFSET;

    // thread entry stubs - one per spawned function
    // maps function name -> offset in code buffer where the stub lives
    std::unordered_map<std::string, std::size_t> thread_stubs_;
    
    // auto-parallelization flag
    bool auto_parallel_ = false;
    
    // monotonic counter for deterministic spawn variable names
    std::size_t spawn_counter_ = 0;
    
    // rip-relative slot in code buffer that holds the global data base pointer
    // __opus_init writes HeapAlloc result here, all global accesses load from here
    std::size_t global_base_slot_ = 0;
    bool has_global_slot_ = false;

    // builtin dispatch tables - lazily initialized on first generate_call
    using BuiltinHandler = std::function<bool(const ast::CallExpr&)>;
    std::unordered_map<std::string_view, BuiltinHandler> dll_builtins_;
    std::unordered_map<std::string_view, BuiltinHandler> jit_builtins_;
    bool builtin_tables_initialized_ = false;

    void init_builtin_tables() {
        if (builtin_tables_initialized_) return;
        builtin_tables_initialized_ = true;

        // ---- dll-mode builtins (checked first when dll_mode_ is true) ----
        dll_builtins_ = {
            {"dll_print",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"print",           [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"dll_set_title",   [this](const ast::CallExpr& c) { return generate_dll_builtin_set_title(c); }},
            {"set_title",       [this](const ast::CallExpr& c) { return generate_dll_builtin_set_title(c); }},
            {"alloc_console",   [this](const ast::CallExpr& c) { return generate_dll_builtin_alloc_console(c); }},
            {"print_int",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_dec(c); }},
            {"print_dec",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_dec(c); }},
            {"print_hex",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_hex(c); }},
            // memory operations - inline, no runtime needed
            {"mem_read",        [this](const ast::CallExpr& c) { return generate_dll_mem_read(c, 8); }},
            {"mem_read_i64",    [this](const ast::CallExpr& c) { return generate_dll_mem_read(c, 8); }},
            {"mem_read_i32",    [this](const ast::CallExpr& c) { return generate_dll_mem_read(c, 4); }},
            {"mem_read_i16",    [this](const ast::CallExpr& c) { return generate_dll_mem_read(c, 2); }},
            {"mem_read_i8",     [this](const ast::CallExpr& c) { return generate_dll_mem_read(c, 1); }},
            {"mem_write",       [this](const ast::CallExpr& c) { return generate_dll_mem_write(c, 8); }},
            {"mem_write_i64",   [this](const ast::CallExpr& c) { return generate_dll_mem_write(c, 8); }},
            {"mem_write_i32",   [this](const ast::CallExpr& c) { return generate_dll_mem_write(c, 4); }},
            {"mem_write_i16",   [this](const ast::CallExpr& c) { return generate_dll_mem_write(c, 2); }},
            {"mem_write_i8",    [this](const ast::CallExpr& c) { return generate_dll_mem_write(c, 1); }},
            // debug / crash handler
            {"crash",                   [this](const ast::CallExpr& c) { return generate_dll_crash(c); }},
            {"install_crash_handler",   [this](const ast::CallExpr& c) { return generate_dll_install_crash_handler(c); }},
            {"trigger_illegal",         [this](const ast::CallExpr&) {
                emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x0B);
                return true;
            }},
            {"trigger_stack_overflow",  [this](const ast::CallExpr&) {
                emit_.buffer().emit8(0xB9);
                emit_.buffer().emit32(0xC00000FD);
                emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
                emit_.xor_(x64::Reg::R8, x64::Reg::R8);
                emit_.xor_(x64::Reg::R9, x64::Reg::R9);
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_iat_call_raw(pe::iat::RaiseException);
                emit_.add_imm(x64::Reg::RSP, 32);
                return true;
            }},
            {"breakpoint",      [this](const ast::CallExpr&) {
                emit_.buffer().emit8(0xCC);
                return true;
            }},
            {"breakpoint_if",   [this](const ast::CallExpr& c) {
                if (c.args.empty()) {
                    error("breakpoint_if requires 1 argument");
                    return false;
                }
                if (!generate_expr(*c.args[0])) return false;
                emit_.test(x64::Reg::RAX, x64::Reg::RAX);
                emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x01);
                emit_.buffer().emit8(0xCC);
                return true;
            }},
            // memory block operations
            {"memcpy",          [this](const ast::CallExpr& c) { return generate_dll_memcpy(c); }},
            {"memset",          [this](const ast::CallExpr& c) { return generate_dll_memset(c); }},
            {"memcmp",          [this](const ast::CallExpr& c) { return generate_dll_memcmp(c); }},
            // timing
            {"sleep",           [this](const ast::CallExpr& c) { return generate_dll_sleep(c); }},
            {"get_tick_count",  [this](const ast::CallExpr& c) { return generate_dll_get_tick_count(c); }},
            // math
            {"sqrt",            [this](const ast::CallExpr& c) { return generate_dll_sqrt(c); }},
            {"sin",             [this](const ast::CallExpr& c) { return generate_dll_sin(c); }},
            {"cos",             [this](const ast::CallExpr& c) { return generate_dll_cos(c); }},
            {"tan",             [this](const ast::CallExpr& c) { return generate_dll_tan(c); }},
            {"atan2",           [this](const ast::CallExpr& c) { return generate_dll_atan2(c); }},
            {"floor",           [this](const ast::CallExpr& c) { return generate_dll_floor(c); }},
            {"ceil",            [this](const ast::CallExpr& c) { return generate_dll_ceil(c); }},
            {"abs",             [this](const ast::CallExpr& c) { return generate_dll_abs(c); }},
            {"fabs",            [this](const ast::CallExpr& c) { return generate_dll_abs(c); }},
            {"pow",             [this](const ast::CallExpr& c) { return generate_dll_pow(c); }},
            // memory allocation
            {"malloc",          [this](const ast::CallExpr& c) { return generate_dll_malloc(c); }},
            {"free",            [this](const ast::CallExpr& c) { return generate_dll_free(c); }},
            // arrays
            {"array_new",       [this](const ast::CallExpr& c) { return generate_dll_array_new(c); }},
            {"new_array",       [this](const ast::CallExpr& c) { return generate_dll_array_new(c); }},
            {"array_get",       [this](const ast::CallExpr& c) { return generate_dll_array_get(c); }},
            {"array_set",       [this](const ast::CallExpr& c) { return generate_dll_array_set(c); }},
            {"array_len",       [this](const ast::CallExpr& c) { return generate_dll_array_len(c); }},
            {"array_free",      [this](const ast::CallExpr& c) { return generate_dll_array_free(c); }},
            // strings
            {"string_length",   [this](const ast::CallExpr& c) { return generate_dll_string_length(c); }},
            {"strlen",          [this](const ast::CallExpr& c) { return generate_dll_string_length(c); }},
            {"string_append",   [this](const ast::CallExpr& c) { return generate_dll_string_append(c); }},
            {"concat",          [this](const ast::CallExpr& c) { return generate_dll_string_append(c); }},
            {"int_to_string",   [this](const ast::CallExpr& c) { return generate_dll_int_to_string(c); }},
            {"itoa",            [this](const ast::CallExpr& c) { return generate_dll_int_to_string(c); }},
            {"string_equals",   [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"streq",           [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"string_substring",[this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"substr",          [this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"print_char",      [this](const ast::CallExpr& c) { return generate_dll_print_char(c); }},
            {"putc",            [this](const ast::CallExpr& c) { return generate_dll_print_char(c); }},
            // memory protection
            {"virtual_protect", [this](const ast::CallExpr& c) { return generate_dll_virtual_protect(c); }},
            {"virtual_alloc",   [this](const ast::CallExpr& c) { return generate_dll_virtual_alloc(c); }},
            {"virtual_free",    [this](const ast::CallExpr& c) { return generate_dll_virtual_free(c); }},
            // pattern scanner
            {"scan",            [this](const ast::CallExpr& c) { return generate_dll_scan(c); }},
            // module functions
            {"get_module",      [this](const ast::CallExpr& c) { return generate_dll_get_module(c); }},
        };

        // ---- shared builtins (both jit and dll mode) ----
        // note: "print" and "println" have dual-mode logic baked into their handlers
        jit_builtins_ = {
            {"print_int",       [this](const ast::CallExpr& c) { return generate_builtin_print_int(c); }},
            {"print",           [this](const ast::CallExpr& c) {
                if (dll_mode_) return generate_dll_builtin_print(c);
                return generate_builtin_one_arg(c, as_void(rt_.print_str));
            }},
            {"println",         [this](const ast::CallExpr& c) {
                if (dll_mode_) {
                    if (!generate_dll_builtin_print(c)) return false;
                    emit_.sub_imm(x64::Reg::RSP, 32);
                    emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x04);
                    emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x0A);
                    emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44);
                    emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x01);
                    emit_.buffer().emit8(0x00);
                    emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
                    emit_startup_call(print_offset_);
                    emit_.add_imm(x64::Reg::RSP, 32);
                } else {
                    if (!generate_builtin_one_arg(c, as_void(rt_.print_str))) return false;
                }
                return true;
            }},
            {"read_file",       [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.read_file)); }},
            {"string_length",   [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.string_length)); }},
            {"strlen",          [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.string_length)); }},
            {"string_get_char", [this](const ast::CallExpr& c) { return generate_builtin_string_get_char(c); }},
            {"char_at",         [this](const ast::CallExpr& c) { return generate_builtin_string_get_char(c); }},
            {"print_string",    [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_string)); }},
            {"puts",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_string)); }},
            {"write_file",      [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.write_file)); }},
            {"malloc",          [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.malloc_fn)); }},
            {"free",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.free_fn)); }},
            {"array_new",       [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.array_new)); }},
            {"new_array",       [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.array_new)); }},
            {"array_get",       [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.array_get)); }},
            {"array_set",       [this](const ast::CallExpr& c) { return generate_builtin_three_arg(c, as_void(rt_.array_set)); }},
            {"array_len",       [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.array_len)); }},
            {"array_free",      [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.array_free)); }},
            {"string_append",   [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_append)); }},
            {"concat",          [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_append)); }},
            {"int_to_string",   [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.int_to_string)); }},
            {"itoa",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.int_to_string)); }},
            {"print_char",      [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_char)); }},
            {"putc",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_char)); }},
            {"string_equals",   [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_equals)); }},
            {"streq",           [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_equals)); }},
            {"string_substring",[this](const ast::CallExpr& c) { return generate_builtin_three_arg(c, as_void(rt_.string_substring)); }},
            {"substr",          [this](const ast::CallExpr& c) { return generate_builtin_three_arg(c, as_void(rt_.string_substring)); }},
            {"is_alpha",        [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.is_alpha)); }},
            {"is_digit",        [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.is_digit)); }},
            {"is_alnum",        [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.is_alnum)); }},
            {"is_whitespace",   [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.is_whitespace)); }},
            {"string_starts_with", [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_starts_with)); }},
            {"starts_with",     [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.string_starts_with)); }},
            {"exit",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.exit_fn)); }},
            {"write_bytes",     [this](const ast::CallExpr& c) { return generate_builtin_three_arg(c, as_void(rt_.write_bytes)); }},
            {"buffer_new",      [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.buffer_new)); }},
            {"buffer_push",     [this](const ast::CallExpr& c) { return generate_builtin_two_arg(c, as_void(rt_.buffer_push)); }},
            {"buffer_len",      [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.buffer_len)); }},
            {"parse_int",       [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.parse_int)); }},
            {"atoi",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.parse_int)); }},
            // memory operations
            {"mem_read",        [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i64(c); }},
            {"mem_read_i64",    [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i64(c); }},
            {"memory_read",     [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i64(c); }},
            {"mem_read_i32",    [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i32(c); }},
            {"mem_read_i16",    [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i16(c); }},
            {"mem_read_i8",     [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i8(c); }},
            {"mem_read_ptr",    [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i64(c); }},
            {"read_ptr",        [this](const ast::CallExpr& c) { return generate_builtin_mem_read_i64(c); }},
            {"mem_write",       [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i64(c); }},
            {"mem_write_i64",   [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i64(c); }},
            {"memory_write",    [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i64(c); }},
            {"mem_write_i32",   [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i32(c); }},
            {"mem_write_i16",   [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i16(c); }},
            {"mem_write_i8",    [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i8(c); }},
            {"mem_write_ptr",   [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i64(c); }},
            {"write_ptr",       [this](const ast::CallExpr& c) { return generate_builtin_mem_write_i64(c); }},
            // ffi - windows api
            {"get_module",      [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "get_module"); }},
            {"load_library",    [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "load_library"); }},
            {"get_proc",        [this](const ast::CallExpr& c) { return generate_builtin_ffi_two_arg(c, "get_proc"); }},
            {"ffi_call",        [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "ffi_call0"); }},
            {"ffi_call0",       [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "ffi_call0"); }},
            {"ffi_call1",       [this](const ast::CallExpr& c) { return generate_builtin_ffi_two_arg(c, "ffi_call1"); }},
            {"ffi_call2",       [this](const ast::CallExpr& c) { return generate_builtin_ffi_three_arg(c, "ffi_call2"); }},
            {"ffi_call3",       [this](const ast::CallExpr& c) { return generate_builtin_ffi_four_arg(c, "ffi_call3"); }},
            {"ffi_call4",       [this](const ast::CallExpr& c) { return generate_builtin_ffi_five_arg(c, "ffi_call4"); }},
            {"msgbox",          [this](const ast::CallExpr& c) { return generate_builtin_ffi_three_arg(c, "msgbox"); }},
            {"get_last_error",  [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("get_last_error"); }},
            {"virtual_protect", [this](const ast::CallExpr& c) { return generate_builtin_ffi_three_arg(c, "virtual_protect"); }},
            {"get_current_process",    [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("get_current_process"); }},
            {"get_current_process_id", [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("get_current_process_id"); }},
            {"getpid",          [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("get_current_process_id"); }},
            {"alloc_console",   [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("alloc_console"); }},
            {"free_console",    [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("free_console"); }},
            {"set_console_title", [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "set_console_title"); }},
            // range() called directly just returns 0
            {"range",           [this](const ast::CallExpr&) {
                emit_.mov_imm32(x64::Reg::RAX, 0);
                return true;
            }},
        };
    }

    void error(std::string_view msg) {
        errors_.push_back(std::string(msg));
    }

    Scope& push_scope() {
        auto scope = std::make_unique<Scope>();
        scope->parent = current_scope_;
        if (current_scope_) {
            scope->next_offset = current_scope_->next_offset;
        }
        current_scope_ = scope.get();
        scopes_.push_back(std::move(scope));
        return *current_scope_;
    }

    void pop_scope() {
        if (current_scope_) {
            current_scope_ = current_scope_->parent;
        }
    }

    // ========================================================================
    // codegen helpers - shared patterns extracted to avoid duplication
    // ========================================================================

    // emit lea <dst>, [rip+disp32] and return the disp32 patch site
    std::size_t emit_lea_rip_disp32(x64::Reg dst) {
        emit_.buffer().emit8(0x48);  // REX.W
        emit_.buffer().emit8(0x8D);  // LEA opcode
        emit_.buffer().emit8(static_cast<std::uint8_t>((static_cast<std::uint8_t>(dst) & 7) << 3) | 0x05);
        std::size_t site = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        return site;
    }

    // emit lea <dst>, [rip+global_base_slot_] with immediate patching
    void emit_lea_rip_slot(x64::Reg dst) {
        std::size_t site = emit_lea_rip_disp32(dst);
        std::int32_t rel = static_cast<std::int32_t>(global_base_slot_)
            - static_cast<std::int32_t>(site + 4);
        emit_.buffer().patch32(site, static_cast<std::uint32_t>(rel));
    }

    // extract struct/class name from a Type, handling both StructType and string variants
    std::optional<std::string> get_struct_name(const Type& type) {
        if (std::holds_alternative<StructType>(type.kind))
            return std::get<StructType>(type.kind).name;
        if (std::holds_alternative<std::string>(type.kind))
            return std::get<std::string>(type.kind);
        return std::nullopt;
    }

    // inline strlen: rcx = string ptr on entry, rax = length on exit
    // clobbers rcx (walks past end of string)
    void emit_inline_strlen() {
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t loop_top = emit_.buffer().pos();
        // cmp byte [rcx], 0
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x39); emit_.buffer().emit8(0x00);
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // inc rcx, inc rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC0);
        emit_.jmp_rel32(static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5));
        emit_.patch_jump(done);
    }

    // inline memcpy: rcx = src, rdx = dst, r8 = count
    // copies r8 bytes from [rcx] to [rdx], advances rcx/rdx past the copied region
    // clobbers rax, rcx, rdx, r8
    void emit_inline_memcpy() {
        std::size_t loop_top = emit_.buffer().pos();
        emit_.test(x64::Reg::R8, x64::Reg::R8);
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // movzx rax, byte [rcx]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01);
        // mov byte [rdx], al
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x02);
        // inc rcx, inc rdx, dec r8
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC8);
        emit_.jmp_rel32(static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5));
        emit_.patch_jump(done);
    }

    // ========================================================================
    // declarations
    // ========================================================================

    [[nodiscard]] bool generate_decl(const ast::Decl& decl) {
        if (decl.is<ast::FnDecl>()) {
            return generate_fn(decl.as<ast::FnDecl>());
        }
        if (decl.is<ast::StructDecl>()) {
            return generate_struct_decl(decl.as<ast::StructDecl>());
        }
        if (decl.is<ast::ClassDecl>()) {
            return generate_class_decl(decl.as<ast::ClassDecl>());
        }
        if (decl.is<ast::EnumDecl>()) {
            return generate_enum_decl(decl.as<ast::EnumDecl>());
        }
        if (decl.is<ast::StaticDecl>()) {
            return register_static_decl(decl.as<ast::StaticDecl>());
        }
        if (decl.is<ast::ImportDecl>()) {
            return generate_import(decl.as<ast::ImportDecl>());
        }
        return true;
    }
    
    // resolve and compile an imported module
    [[nodiscard]] bool generate_import(const ast::ImportDecl& imp) {
        // convert dot-separated path to filesystem path (foo.bar -> foo/bar.op)
        std::string rel_path = imp.path;
        for (auto& c : rel_path) {
            if (c == '.') c = '/';
        }
        rel_path += ".op";

        // resolve relative to current source file
        std::filesystem::path base_dir;
        if (!source_path_.empty()) {
            base_dir = std::filesystem::path(source_path_).parent_path();
        } else {
            base_dir = std::filesystem::current_path();
        }
        auto full_path = std::filesystem::weakly_canonical(base_dir / rel_path);
        std::string canonical = full_path.string();

        // skip if already imported
        if (imported_modules_.contains(canonical)) return true;
        imported_modules_.insert(canonical);

        // read the file
        std::ifstream file(full_path);
        if (!file) {
            error(std::format("cannot find module: {}", full_path.string()));
            return false;
        }
        std::stringstream buf;
        buf << file.rdbuf();
        std::string source = buf.str();

        // lex
        Lexer lexer(source, canonical);
        auto tokens = lexer.tokenize_all();
        for (const auto& tok : tokens) {
            if (tok.kind == TokenKind::Error) {
                error(std::format("{}:{}:{}: lexer error in imported module: {}",
                    tok.loc.file, tok.loc.line, tok.loc.column, tok.text));
                return false;
            }
        }

        // parse
        Parser parser(std::move(tokens), SyntaxMode::CStyle);
        auto mod_result = parser.parse_module(canonical);
        if (!mod_result) {
            for (const auto& err : mod_result.error()) {
                error(std::format("error in imported module {}: {}", imp.path, err.to_string()));
            }
            return false;
        }

        // save current source path, set to imported file
        auto saved_path = source_path_;
        source_path_ = canonical;

        // first pass: collect function signatures from imported module
        for (const auto& decl : mod_result->decls) {
            if (decl->is<ast::FnDecl>()) {
                const auto& fn = decl->as<ast::FnDecl>();
                if (!functions_.contains(fn.name)) {
                    functions_[fn.name] = FunctionInfo{
                        .name = fn.name,
                        .return_type = fn.return_type.clone()
                    };
                }
            }
        }

        // second pass: compile declarations
        for (const auto& decl : mod_result->decls) {
            if (!generate_decl(*decl)) {
                source_path_ = saved_path;
                return false;
            }
        }

        source_path_ = saved_path;
        return true;
    }

    // track global variables for later initialization
    bool register_static_decl(const ast::StaticDecl& sd) {
        // skip if already registered (first pass collects them early)
        if (globals_.contains(sd.name)) return true;
        
        GlobalVar gv;
        gv.name = sd.name;
        gv.type = sd.type.clone();
        gv.is_mut = sd.is_mut;
        gv.offset = next_global_offset_;
        next_global_offset_ += 8;
        
        if (sd.init) {
            gv.init = sd.init.value().get();
        }
        
        globals_[sd.name] = std::move(gv);
        global_order_.push_back(sd.name);
        return true;
    }
    
    // generate __opus_init function that allocates globals and runs initializers
    void generate_global_init() {
        // register function
        functions_["__opus_init"] = FunctionInfo{
            .name = "__opus_init",
            .return_type = Type::make_primitive(PrimitiveType::Void)
        };
        
        auto& info = functions_["__opus_init"];
        info.code_offset = emit_.buffer().pos();
        
        push_scope();
        emit_.prologue(256);
        
        if (dll_mode_ && next_global_offset_ > 0) {
            // allocate global storage via HeapAlloc
            emit_.sub_imm(x64::Reg::RSP, 32);
            
            emit_iat_call_raw(pe::iat::GetProcessHeap);
            
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
            emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
            emit_.mov_imm32(x64::Reg::R8, static_cast<std::int32_t>(next_global_offset_));
            
            emit_iat_call_raw(pe::iat::HeapAlloc);
            
            emit_.add_imm(x64::Reg::RSP, 32);
            
            // store base pointer to rip-relative slot so other functions can find it
            emit_lea_rip_slot(x64::Reg::RCX);
            emit_.mov_store(x64::Reg::RCX, 0, x64::Reg::RAX);
            
            // run initializers
            for (const auto& name : global_order_) {
                auto& gv = globals_[name];
                if (gv.init) {
                    // evaluate initializer -> RAX
                    if (!generate_expr(*gv.init)) return;
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX); // save value
                    
                    // reload base from slot
                    emit_lea_rip_slot(x64::Reg::RAX);
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0); // deref: rax = *slot
                    
                    // store value at base + offset
                    emit_.mov_store(x64::Reg::RAX, static_cast<std::int32_t>(gv.offset), x64::Reg::RCX);
                }
            }
        }
        
        emit_.epilogue();
        pop_scope();
        
        info.code_size = emit_.buffer().pos() - info.code_offset;
    }

    [[nodiscard]] bool generate_struct_decl(const ast::StructDecl& s) {
        StructInfo info;
        info.name = s.name;
        for (const auto& [name, type] : s.fields) {
            info.fields.emplace_back(name, type.clone());
        }
        info.calculate_offsets();
        structs_[s.name] = std::move(info);
        return true;
    }
    
    [[nodiscard]] bool generate_class_decl(const ast::ClassDecl& c) {
        // Register fields (same as struct)
        StructInfo info;
        info.name = c.name;
        for (const auto& [name, type] : c.fields) {
            info.fields.emplace_back(name, type.clone());
        }
        info.calculate_offsets();
        structs_[c.name] = std::move(info);
        
        // methods get mangled as ClassName_MethodName
        for (const auto& method : c.methods) {
            std::string mangled_name = c.name + "_" + method.name;
            
            // Register function info first
            functions_[mangled_name] = FunctionInfo{
                .name = mangled_name,
                .return_type = method.return_type.clone()
            };
            
            // Generate the method code
            auto& func_info = functions_[mangled_name];
            func_info.code_offset = emit_.buffer().pos();
            
            push_scope();
            
            // Prologue
            emit_.prologue(1024);
            
            // implicit self param in rcx (windows x64 convention)
            current_class_name_ = c.name;
            Type self_type;
            self_type.kind = c.name;  // Named type = class name
            Symbol& self_sym = current_scope_->define("self", std::move(self_type), true);
            self_sym.is_param = true;
            emit_.mov_store(x64::Reg::RBP, self_sym.stack_offset, x64::Reg::RCX);
            
            // explicit params: rdx, r8, r9 for first 3, then stack for rest
            const x64::Reg param_regs[] = {x64::Reg::RDX, x64::Reg::R8, x64::Reg::R9};
            
            for (std::size_t i = 0; i < method.params.size(); ++i) {
                const auto& param = method.params[i];
                Symbol& sym = current_scope_->define(param.name, param.type.clone(), param.is_mut);
                sym.is_param = true;
                if (i < 3) {
                    emit_.mov_store(x64::Reg::RBP, sym.stack_offset, param_regs[i]);
                } else {
                    // stack params: total arg index = i+1 (self is 0)
                    // caller puts them at [RSP+32+(idx-4)*8] before call
                    // after push rbp + mov rbp,rsp thats [RBP+16+(i+1)*8]
                    std::int32_t src_offset = 16 + static_cast<std::int32_t>(i + 1) * 8;
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, src_offset);
                    emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
                }
            }
            
            // Generate body
            for (const auto& stmt : method.body) {
                if (!generate_stmt(*stmt)) {
                    pop_scope();
                    return false;
                }
            }
            
            // Default return if no explicit return
            emit_.mov_imm32(x64::Reg::RAX, 0);
            emit_.epilogue();
            
            pop_scope();
            
            func_info.code_size = emit_.buffer().pos() - func_info.code_offset;
        }
        
        current_class_name_.clear();
        return true;
    }

    [[nodiscard]] bool generate_enum_decl(const ast::EnumDecl& e) {
        std::unordered_map<std::string, std::int64_t> variants;
        std::int64_t next_value = 0;
        for (const auto& [name, value] : e.variants) {
            if (value) {
                next_value = *value;
            }
            variants[name] = next_value;
            next_value++;
        }
        enums_[e.name] = std::move(variants);
        return true;
    }

    [[nodiscard]] bool generate_fn(const ast::FnDecl& fn) {
        if (fn.is_extern) {
            // External functions don't need code generation
            return true;
        }

        // Record function start
        auto& info = functions_[fn.name];
        info.code_offset = emit_.buffer().pos();

        push_scope();

        emit_.prologue(1024);  // 128 locals worth of stack

        // if this is main and we have globals, call __opus_init first
        if (fn.name == "main" && !globals_.empty()) {
            emit_.sub_imm(x64::Reg::RSP, 32); // shadow space
            emit_.buffer().emit8(0xE8); // CALL rel32
            std::size_t init_fixup = emit_.buffer().pos();
            emit_.buffer().emit32(0);
            call_fixups_.push_back(CallFixup{
                .call_site = init_fixup,
                .target_fn = "__opus_init"
            });
            emit_.add_imm(x64::Reg::RSP, 32);
        }

        // windows x64: first 4 args in rcx, rdx, r8, r9
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            const auto& param = fn.params[i];
            auto& sym = current_scope_->define(param.name, param.type.clone(), param.is_mut);
            sym.is_param = true;
            
            // spill param register to stack slot
            if (i < 4) {
                x64::Reg param_reg = x64::ARG_REGS[i];
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, param_reg);
            } else {
                // stack params: caller puts them at [RSP+32+(i-4)*8] before call
                // after push rbp + mov rbp,rsp thats [RBP+16+i*8]
                std::int32_t src_offset = 16 + static_cast<std::int32_t>(i) * 8;
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, src_offset);
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
            }
        }

        // Generate body
        for (const auto& stmt : fn.body) {
            if (!generate_stmt(*stmt)) {
                pop_scope();
                return false;
            }
        }

        // If no explicit return, add one
        if (fn.return_type.is_void()) {
            emit_.epilogue();
        } else {
            // Default return 0
            emit_.mov_imm32(x64::Reg::RAX, 0);
            emit_.epilogue();
        }

        info.code_size = emit_.buffer().pos() - info.code_offset;

        pop_scope();
        return true;
    }

    // ========================================================================
    // statements
    // ========================================================================

    [[nodiscard]] bool generate_stmt(const ast::Stmt& stmt) {
        if (stmt.span.start.line > 0) {
            line_map_.push_back({
                static_cast<std::uint32_t>(emit_.buffer().pos()),
                stmt.span.start.line
            });
        }

        return std::visit(overloaded{
            [&](const ast::LetStmt& s)         { return generate_let(s); },
            [&](const ast::ReturnStmt& s)      { return generate_return(s); },
            [&](const ast::ExprStmt& s)        { return generate_expr_stmt(s); },
            [&](const ast::IfStmt& s)          { return generate_if(s); },
            [&](const ast::WhileStmt& s)       { return generate_while(s); },
            [&](const ast::LoopStmt& s)        { return generate_loop(s); },
            [&](const ast::ForStmt& s)         { return generate_for(s); },
            [&](const ast::ParallelForStmt& s) { return generate_parallel_for(s); },
            [&](const ast::BreakStmt&)         { return generate_break(); },
            [&](const ast::ContinueStmt&)      { return generate_continue(); },
            [&](const ast::BlockStmt& block) -> bool {
                push_scope();
                for (const auto& s : block.stmts) {
                    if (!generate_stmt(*s)) {
                        pop_scope();
                        return false;
                    }
                }
                pop_scope();
                return true;
            },
        }, stmt.kind);
    }

    [[nodiscard]] bool generate_let(const ast::LetStmt& let) {
        Type type;
        if (let.type) {
            type = let.type->clone();
        } else if (let.init) {
            // Infer type from init expression
            if (let.init.value()->is<ast::StructExpr>()) {
                // Struct literal - use struct name as type
                const auto& struct_lit = let.init.value()->as<ast::StructExpr>();
                type.kind = struct_lit.name;  // Named type = struct name
            } else {
                type = Type::make_primitive(PrimitiveType::I64);
            }
        } else {
            error("cannot infer type for variable without type or initializer");
            return false;
        }

        auto& sym = current_scope_->define(let.name, std::move(type), let.is_mut);

        if (let.init) {
            if (!generate_expr(*let.init.value())) return false;
            emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
        }

        return true;
    }

    [[nodiscard]] bool generate_return(const ast::ReturnStmt& ret) {
        if (ret.value) {
            if (!generate_expr(*ret.value.value())) return false;
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] bool generate_expr_stmt(const ast::ExprStmt& stmt) {
        return generate_expr(*stmt.expr);
    }

    [[nodiscard]] bool generate_if(const ast::IfStmt& if_stmt) {
        if (!generate_expr(*if_stmt.condition)) return false;
        
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t jz_patch = emit_.jcc_rel32(x64::Emitter::CC_E);

        push_scope();
        for (const auto& stmt : if_stmt.then_block) {
            if (!generate_stmt(*stmt)) {
                pop_scope();
                return false;
            }
        }
        pop_scope();

        if (!if_stmt.else_block.empty()) {
            std::size_t jmp_patch = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(jz_patch);

            push_scope();
            for (const auto& stmt : if_stmt.else_block) {
                if (!generate_stmt(*stmt)) {
                    pop_scope();
                    return false;
                }
            }
            pop_scope();

            emit_.patch_jump(jmp_patch);
        } else {
            emit_.patch_jump(jz_patch);
        }

        return true;
    }

    [[nodiscard]] bool generate_while(const ast::WhileStmt& while_stmt) {
        break_patches_.push_back({});
        continue_patches_.push_back({});
        std::size_t loop_start = emit_.buffer().pos();

        if (!generate_expr(*while_stmt.condition)) return false;

        // Test RAX
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);

        // Jump to end if zero
        std::size_t jz_patch = emit_.jcc_rel32(x64::Emitter::CC_E);

        // Body (with new scope)
        push_scope();
        for (const auto& stmt : while_stmt.body) {
            if (!generate_stmt(*stmt)) {
                pop_scope();
                return false;
            }
        }
        pop_scope();

        // continue target = loop_start (re-evaluate condition)
        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        // Jump back to start
        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        // Patch the jz to here (end)
        emit_.patch_jump(jz_patch);

        // Patch all breaks
        for (std::size_t patch : break_patches_.back()) {
            emit_.patch_jump(patch);
        }
        break_patches_.pop_back();

        return true;
    }

    [[nodiscard]] bool generate_loop(const ast::LoopStmt& loop_stmt) {
        push_scope();
        break_patches_.push_back({});
        continue_patches_.push_back({});
        std::size_t loop_start = emit_.buffer().pos();

        for (const auto& stmt : loop_stmt.body) {
            if (!generate_stmt(*stmt)) { pop_scope(); return false; }
        }

        // continue target = loop_start
        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        // Jump back to start
        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        // Patch all breaks
        for (std::size_t patch : break_patches_.back()) {
            emit_.patch_jump(patch);
        }
        break_patches_.pop_back();

        pop_scope();
        return true;
    }

    [[nodiscard]] bool generate_for(const ast::ForStmt& for_stmt) {
        push_scope();
        break_patches_.push_back({});
        continue_patches_.push_back({});
        
        // evaluate range bounds as full expressions
        if (!for_stmt.iterable->is<ast::CallExpr>()) {
            error("for-in requires a range() call");
            pop_scope();
            return false;
        }
        const auto& call = for_stmt.iterable->as<ast::CallExpr>();
        if (!call.callee->is<ast::IdentExpr>() || 
            call.callee->as<ast::IdentExpr>().name != "range") {
            error("for-in requires a range() call");
            pop_scope();
            return false;
        }
        
        if (call.args.size() > 2) {
            error("range() accepts 1 or 2 arguments");
            pop_scope();
            return false;
        }
        
        // evaluate start
        auto& start_sym = current_scope_->define("__for_start",
            Type::make_primitive(PrimitiveType::I64), true);
        if (call.args.size() >= 2) {
            if (!generate_expr(*call.args[0])) { pop_scope(); return false; }
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        emit_.mov_store(x64::Reg::RBP, start_sym.stack_offset, x64::Reg::RAX);
        
        // evaluate end
        auto& end_sym = current_scope_->define("__for_end",
            Type::make_primitive(PrimitiveType::I64), true);
        if (call.args.size() >= 2) {
            if (!generate_expr(*call.args[1])) { pop_scope(); return false; }
        } else if (call.args.size() == 1) {
            if (!generate_expr(*call.args[0])) { pop_scope(); return false; }
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        emit_.mov_store(x64::Reg::RBP, end_sym.stack_offset, x64::Reg::RAX);
        
        // loop variable = start
        auto& sym = current_scope_->define(for_stmt.name, 
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, start_sym.stack_offset);
        emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
        
        // loop top: compare against end
        std::size_t loop_start = emit_.buffer().pos();
        
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, end_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t jge_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        
        // body
        for (const auto& stmt : for_stmt.body) {
            if (!generate_stmt(*stmt)) {
                pop_scope();
                return false;
            }
        }
        
        // increment - continue jumps here (skip body, do increment, loop back)
        std::size_t increment_pos = emit_.buffer().pos();
        patch_jumps_to(continue_patches_.back(), increment_pos);
        continue_patches_.pop_back();
        
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
        
        // jump back to condition check
        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);
        
        // patch exit jump
        emit_.patch_jump(jge_patch);
        
        // patch all breaks
        for (std::size_t patch : break_patches_.back()) {
            emit_.patch_jump(patch);
        }
        break_patches_.pop_back();
        
        pop_scope();
        return true;
    }

    // patch a list of jmp placeholders to jump to a specific target address
    void patch_jumps_to(const std::vector<std::size_t>& patches, std::size_t target) {
        for (std::size_t patch : patches) {
            std::int32_t rel = static_cast<std::int32_t>(target - patch - 4);
            emit_.buffer().patch32(patch, static_cast<std::uint32_t>(rel));
        }
    }

    [[nodiscard]] bool generate_break() {
        if (break_patches_.empty()) {
            error("break outside of loop");
            return false;
        }
        std::size_t patch = emit_.jmp_rel32_placeholder();
        break_patches_.back().push_back(patch);
        return true;
    }

    [[nodiscard]] bool generate_continue() {
        if (continue_patches_.empty()) {
            error("continue outside of loop");
            return false;
        }
        std::size_t patch = emit_.jmp_rel32_placeholder();
        continue_patches_.back().push_back(patch);
        return true;
    }

    // ========================================================================
    // expressions - result always in rax
    // ========================================================================

    [[nodiscard]] bool generate_expr(const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr& e) { return generate_literal(e); },
            [&](const ast::IdentExpr& e)   { return generate_ident(e); },
            [&](const ast::BinaryExpr& e)  { return generate_binary(e); },
            [&](const ast::UnaryExpr& e)   { return generate_unary(e); },
            [&](const ast::CallExpr& e)    { return generate_call(e); },
            [&](const ast::IndexExpr& e)   { return generate_index(e); },
            [&](const ast::FieldExpr& e)   { return generate_field(e); },
            [&](const ast::CastExpr& e)    { return generate_expr(*e.expr); },
            [&](const ast::StructExpr& e)  { return generate_struct_literal(e); },
            [&](const ast::SpawnExpr& e)   { return generate_spawn(e); },
            [&](const ast::AwaitExpr& e)   { return generate_await(e); },
            [&](const ast::AtomicOpExpr& e){ return generate_atomic_op(e); },
            [&](const auto&) -> bool {
                error("unknown expression type");
                return false;
            },
        }, expr.kind);
    }

    // resolve the struct type name that a field expression evaluates to
    // e.g. for `outer.inner` where outer is Outer and inner is of type Inner, returns "Inner"
    std::optional<std::string> resolve_field_type(const ast::FieldExpr& field) {
        std::string base_struct;
        
        if (field.base->is<ast::IdentExpr>()) {
            const auto& name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(name);
            if (!sym) return std::nullopt;
            auto sn = get_struct_name(sym->type);
            if (!sn) return std::nullopt;
            base_struct = *sn;
        } else if (field.base->is<ast::FieldExpr>()) {
            auto inner = resolve_field_type(field.base->as<ast::FieldExpr>());
            if (!inner) return std::nullopt;
            base_struct = *inner;
        } else {
            return std::nullopt;
        }
        
        // look up the field type on base_struct
        auto it = structs_.find(base_struct);
        if (it == structs_.end()) return std::nullopt;
        
        for (const auto& [fname, ftype] : it->second.fields) {
            if (fname == field.field) {
                return get_struct_name(ftype);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool generate_field(const ast::FieldExpr& field) {
        // check if this is an enum variant access (EnumName.Variant)
        if (field.base->is<ast::IdentExpr>()) {
            const std::string& base_name = field.base->as<ast::IdentExpr>().name;
            auto enum_it = enums_.find(base_name);
            if (enum_it != enums_.end()) {
                auto variant_it = enum_it->second.find(field.field);
                if (variant_it != enum_it->second.end()) {
                    emit_.mov_imm64(x64::Reg::RAX, static_cast<std::uint64_t>(variant_it->second));
                    return true;
                }
                error(std::format("enum '{}' has no variant '{}'", base_name, field.field));
                return false;
            }
        }
        
        // resolve the struct type of the base expression
        std::string struct_name;
        
        if (field.base->is<ast::IdentExpr>()) {
            const std::string& var_name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(var_name);
            if (!sym) {
                error(std::format("undefined variable: {}", var_name));
                return false;
            }
            auto sn = get_struct_name(sym->type);
            if (!sn) {
                error(std::format("variable '{}' is not a struct type", var_name));
                return false;
            }
            struct_name = *sn;
            // load base pointer from variable
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
        } else {
            // chained field access: evaluate base expression to get pointer in RAX
            // first figure out the struct type of the base
            if (field.base->is<ast::FieldExpr>()) {
                auto base_type = resolve_field_type(field.base->as<ast::FieldExpr>());
                if (!base_type) {
                    error("cannot resolve type of chained field access");
                    return false;
                }
                struct_name = *base_type;
            } else {
                error("unsupported base expression for field access");
                return false;
            }
            // evaluate base expression -> RAX = pointer to intermediate struct
            if (!generate_expr(*field.base)) return false;
        }
        
        // look up field offset
        auto struct_it = structs_.find(struct_name);
        if (struct_it == structs_.end()) {
            error(std::format("unknown struct type: {}", struct_name));
            return false;
        }
        
        auto offset_opt = struct_it->second.get_field_offset(field.field);
        if (!offset_opt) {
            error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
            return false;
        }
        
        std::size_t offset = *offset_opt;
        
        // load field value: RAX already has base pointer
        if (offset > 0) {
            emit_.add_imm(x64::Reg::RAX, static_cast<std::int32_t>(offset));
        }
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        
        return true;
    }

    [[nodiscard]] bool generate_struct_literal(const ast::StructExpr& lit) {
        // Look up struct definition
        auto it = structs_.find(lit.name);
        if (it == structs_.end()) {
            error(std::format("unknown struct/class: {}", lit.name));
            return false;
        }
        
        const StructInfo& info = it->second;
        
        // Allocate memory for struct
        std::size_t size = info.total_size > 0 ? info.total_size : 8;  // min 8 bytes
        
        if (dll_mode_) {
            // Use HeapAlloc like generate_dll_malloc
            emit_.sub_imm(x64::Reg::RSP, 32);
            
            emit_iat_call_raw(pe::iat::GetProcessHeap);
            
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap -> RCX
            emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
            emit_.mov_imm32(x64::Reg::R8, static_cast<std::int32_t>(size));  // size -> R8
            
            emit_iat_call_raw(pe::iat::HeapAlloc);
            
            emit_.add_imm(x64::Reg::RSP, 32);
        } else if (rt_.malloc_fn) {
            emit_.mov_imm32(x64::Reg::RCX, static_cast<std::int32_t>(size));
            emit_.sub_imm(x64::Reg::RSP, 32);
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.malloc_fn));
            emit_.call(x64::Reg::RAX);
            emit_.add_imm(x64::Reg::RSP, 32);
        }
        
        // RAX now contains pointer to allocated struct
        // Save it to stack temporarily
        emit_.push(x64::Reg::RAX);
        
        // Initialize fields
        for (const auto& [field_name, value_expr] : lit.fields) {
            auto offset_opt = info.get_field_offset(field_name);
            if (!offset_opt) {
                error(std::format("struct '{}' has no field '{}'", lit.name, field_name));
                emit_.pop(x64::Reg::RAX); // clean up struct pointer before bailing
                return false;
            }
            
            std::size_t offset = *offset_opt;
            
            // Generate value expression -> RAX
            if (!generate_expr(*value_expr)) {
                emit_.pop(x64::Reg::RAX); // clean up struct pointer before bailing
                return false;
            }
            
            // Save value
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
            
            // Get struct pointer from stack (don't pop - may need it again)
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 0);
            
            // Store value at field offset
            if (offset > 0) {
                emit_.add_imm(x64::Reg::RAX, static_cast<std::int32_t>(offset));
            }
            emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);
        }
        
        // Pop struct pointer back to RAX as result
        emit_.pop(x64::Reg::RAX);
        
        return true;
    }

    [[nodiscard]] bool generate_index(const ast::IndexExpr& idx) {
        // Generate base address -> RAX
        if (!generate_expr(*idx.base)) return false;
        emit_.push(x64::Reg::RAX);  // Save base
        
        // Generate index -> RAX
        if (!generate_expr(*idx.index)) return false;
        
        // Calculate offset: index * 8 (assuming 64-bit elements)
        emit_.imul_imm(x64::Reg::RAX, x64::Reg::RAX, 8);
        
        // Pop base into RCX
        emit_.pop(x64::Reg::RCX);
        
        // Add: base + (index * 8)
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        
        // Dereference: load value at that address
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        
        return true;
    }

    [[nodiscard]] bool generate_literal(const ast::LiteralExpr& lit) {
        if (auto* i = std::get_if<std::int64_t>(&lit.value)) {
            if (*i >= std::numeric_limits<std::int32_t>::min() && *i <= std::numeric_limits<std::int32_t>::max()) {
                emit_.mov_imm32(x64::Reg::RAX, static_cast<std::int32_t>(*i));
            } else {
                emit_.mov_imm64(x64::Reg::RAX, static_cast<std::uint64_t>(*i));
            }
            return true;
        }
        if (auto* u = std::get_if<std::uint64_t>(&lit.value)) {
            emit_.mov_imm64(x64::Reg::RAX, *u);
            return true;
        }
        if (auto* b = std::get_if<bool>(&lit.value)) {
            emit_.mov_imm32(x64::Reg::RAX, *b ? 1 : 0);
            return true;
        }
        if (auto* d = std::get_if<double>(&lit.value)) {
            emit_.mov_imm64(x64::Reg::RAX, std::bit_cast<std::uint64_t>(*d));
            return true;
        }
        if (auto* c = std::get_if<char32_t>(&lit.value)) {
            // Char as integer
            emit_.mov_imm32(x64::Reg::RAX, static_cast<std::int32_t>(*c));
            return true;
        }
        if (auto* s = std::get_if<std::string>(&lit.value)) {
            // String literal handling
            
            if (dll_mode_) {
                // In DLL mode, we need to embed the string inline
                // Strategy: emit the string AFTER a jump, then reference it
                // 
                //   jmp .after_string
                //   string_data: db "Hello", 0
                // .after_string:
                //   lea rax, [rip + string_data_offset]
                
                const std::string& str = *s;
                
                // Detect if this is a hex string (binary data) or regular string
                // Hex strings don't need null terminator
                bool is_hex = false;
                for (char c : str) {
                    if (static_cast<unsigned char>(c) > 127 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
                        is_hex = true;
                        break;
                    }
                }
                
                std::size_t str_len = is_hex ? str.length() : (str.length() + 1);
                
                // Use short jump for small strings, long jump for large
                bool use_long_jmp = str_len > 127;
                
                if (use_long_jmp) {
                    // Emit: jmp near .after (0xE9 + 32-bit offset)
                    emit_.buffer().emit8(0xE9);
                    std::int32_t offset = static_cast<std::int32_t>(str_len);
                    emit_.buffer().emit8(static_cast<std::uint8_t>(offset & 0xFF));
                    emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 8) & 0xFF));
                    emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 16) & 0xFF));
                    emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 24) & 0xFF));
                } else {
                    // Emit: jmp short .after (0xEB + 8-bit offset)
                    emit_.buffer().emit8(0xEB);
                    emit_.buffer().emit8(static_cast<std::uint8_t>(str_len));
                }
                
                // Record position of string data
                std::size_t string_start = emit_.buffer().pos();
                
                // Emit string bytes
                for (char c : str) {
                    emit_.buffer().emit8(static_cast<std::uint8_t>(c));
                }
                if (!is_hex) {
                    emit_.buffer().emit8(0);  // Null terminator for regular strings only
                }
                
                // Now emit: lea rax, [rip - string_len_with_lea]
                std::size_t current_pos = emit_.buffer().pos();
                std::int32_t offset = static_cast<std::int32_t>(string_start) - static_cast<std::int32_t>(current_pos + 7);
                
                emit_.buffer().emit8(0x48);  // REX.W
                emit_.buffer().emit8(0x8D);  // LEA
                emit_.buffer().emit8(0x05);  // RAX, [RIP+disp32]
                emit_.buffer().emit8(static_cast<std::uint8_t>(offset & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 8) & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 16) & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 24) & 0xFF));
            }
            else if (rt_.make_string) {
                // Normal mode: use runtime string table
                auto make_string = rt_.make_string;
                std::int64_t handle = make_string(s->c_str());
                emit_.mov_imm64(x64::Reg::RAX, static_cast<std::uint64_t>(handle));
            } else {
                // Fallback: return 0
                emit_.mov_imm32(x64::Reg::RAX, 0);
            }
            return true;
        }
        
        error("unsupported literal type");
        return false;
    }

    [[nodiscard]] bool generate_ident(const ast::IdentExpr& ident) {
        // try local scope first
        Symbol* sym = current_scope_->lookup(ident.name);
        if (sym) {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
            return true;
        }
        
        // try globals
        auto it = globals_.find(ident.name);
        if (it != globals_.end()) {
            const auto& gv = it->second;
            
            if (has_global_slot_ && dll_mode_) {
                // load base pointer from rip-relative slot, then load value at offset
                emit_lea_rip_slot(x64::Reg::RAX);
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0); // deref slot -> base ptr
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, static_cast<std::int32_t>(gv.offset));
                return true;
            }
            
            // fallback: literal init optimization for non-dll mode
            if (gv.init && gv.init->is<ast::LiteralExpr>()) {
                const auto& lit = gv.init->as<ast::LiteralExpr>();
                if (auto* i = std::get_if<std::int64_t>(&lit.value)) {
                    emit_.mov_imm64(x64::Reg::RAX, static_cast<std::uint64_t>(*i));
                    return true;
                }
            }
            
            emit_.mov_imm64(x64::Reg::RAX, 0);
            return true;
        }
        
        error(std::format("undefined variable: {}", ident.name));
        return false;
    }

    [[nodiscard]] bool generate_binary(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;

        // Handle assignment and compound assignment specially
        if (bin.op == Op::Assign) {
            return generate_assignment(bin);
        }
        if (bin.op == Op::AddAssign || bin.op == Op::SubAssign || 
            bin.op == Op::MulAssign || bin.op == Op::DivAssign ||
            bin.op == Op::ModAssign) {
            return generate_compound_assign(bin);
        }

        // short-circuit && - dont eval rhs if lhs is false
        if (bin.op == Op::And) {
            if (!generate_expr(*bin.lhs)) return false;
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
            std::size_t skip_rhs = emit_.jcc_rel32(x64::Emitter::CC_E); // lhs false -> skip
            if (!generate_expr(*bin.rhs)) return false;
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
            emit_.setcc(x64::Emitter::CC_NE, x64::Reg::RAX);
            emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
            std::size_t done = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(skip_rhs);
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX); // result = 0
            emit_.patch_jump(done);
            return true;
        }

        // short-circuit || - dont eval rhs if lhs is true
        if (bin.op == Op::Or) {
            if (!generate_expr(*bin.lhs)) return false;
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
            std::size_t skip_rhs = emit_.jcc_rel32(x64::Emitter::CC_NE); // lhs true -> skip
            if (!generate_expr(*bin.rhs)) return false;
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
            emit_.setcc(x64::Emitter::CC_NE, x64::Reg::RAX);
            emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
            std::size_t done = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(skip_rhs);
            emit_.mov_imm32(x64::Reg::RAX, 1); // result = 1
            emit_.patch_jump(done);
            return true;
        }

        // Generate left operand -> RAX
        if (!generate_expr(*bin.lhs)) return false;
        
        // Save to stack temporarily
        emit_.push(x64::Reg::RAX);

        // Generate right operand -> RAX
        if (!generate_expr(*bin.rhs)) return false;

        // Move right to RCX, pop left to RAX
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RAX);

        // Perform operation
        switch (bin.op) {
            case Op::Add:
                emit_.add(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::Sub:
                emit_.sub(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::Mul:
                emit_.imul(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::Div:
                emit_.cqo();  // Sign extend RAX -> RDX:RAX
                emit_.idiv(x64::Reg::RCX);
                break;
            case Op::Mod:
                emit_.cqo();
                emit_.idiv(x64::Reg::RCX);
                emit_.mov(x64::Reg::RAX, x64::Reg::RDX);  // Remainder in RDX
                break;
            case Op::BitAnd:
                emit_.and_(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::BitOr:
                emit_.or_(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::BitXor:
                emit_.xor_(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::Shl:
                emit_.shl_cl(x64::Reg::RAX);
                break;
            case Op::Shr:
                emit_.sar_cl(x64::Reg::RAX);
                break;
            case Op::Eq:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_E, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::Ne:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_NE, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::Lt:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_L, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::Gt:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_G, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::Le:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_LE, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::Ge:
                emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
                emit_.setcc(x64::Emitter::CC_GE, x64::Reg::RAX);
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            default:
                error("unsupported binary operator");
                return false;
        }

        return true;
    }

    [[nodiscard]] bool generate_assignment(const ast::BinaryExpr& bin) {
        // Right side -> RAX
        if (!generate_expr(*bin.rhs)) return false;

        // handle field assignment (e.g., player.health = 100 or outer.inner.x = 42)
        if (bin.lhs->is<ast::FieldExpr>()) {
            const auto& field = bin.lhs->as<ast::FieldExpr>();
            
            // rax has the value to store, save it
            emit_.push(x64::Reg::RAX);
            
            // figure out the struct type that contains the final field
            std::string struct_name;
            
            if (field.base->is<ast::IdentExpr>()) {
                // simple case: var.field = value
                const std::string& var_name = field.base->as<ast::IdentExpr>().name;
                Symbol* sym = current_scope_->lookup(var_name);
                if (!sym) {
                    error(std::format("undefined variable: {}", var_name));
                    return false;
                }
                if (!sym->is_mut) {
                    error(std::format("cannot assign to field of immutable variable: {}", var_name));
                    return false;
                }
                auto sn = get_struct_name(sym->type);
                if (!sn) {
                    error(std::format("variable '{}' is not a struct type", var_name));
                    return false;
                }
                struct_name = *sn;
                // load base pointer
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
            } else if (field.base->is<ast::FieldExpr>()) {
                // chained case: a.b.c = value -> evaluate a.b to get pointer, then write at c offset
                auto base_type = resolve_field_type(field.base->as<ast::FieldExpr>());
                if (!base_type) {
                    error("cannot resolve type of chained field assignment target");
                    return false;
                }
                struct_name = *base_type;
                // evaluate the base chain but we need the POINTER not the loaded value
                // so we use generate_field_addr instead of generate_expr
                // actually we need to evaluate the chain up to the parent and get its address
                // generate_expr on a FieldExpr loads the value, but we need the pointer
                // so lets manually walk the chain to get the intermediate pointer
                if (!generate_expr(*field.base)) return false;
                // wait - generate_expr on a.b loads the VALUE at a.b, but a.b is a struct pointer
                // so RAX = pointer to the inner struct, which is what we want
            } else {
                error("unsupported assignment target for field access");
                return false;
            }
            
            // look up field offset on the resolved struct
            auto struct_it = structs_.find(struct_name);
            if (struct_it == structs_.end()) {
                error(std::format("unknown struct type: {}", struct_name));
                return false;
            }
            auto offset_opt = struct_it->second.get_field_offset(field.field);
            if (!offset_opt) {
                error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
                return false;
            }
            std::size_t offset = *offset_opt;
            
            // rax = base pointer, add field offset
            if (offset > 0) {
                emit_.add_imm(x64::Reg::RAX, static_cast<std::int32_t>(offset));
            }
            
            // pop value into rcx, store at [rax]
            emit_.pop(x64::Reg::RCX);
            emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);
            
            return true;
        }

        // Left side must be an identifier
        if (!bin.lhs->is<ast::IdentExpr>()) {
            error("left side of assignment must be an identifier or field access");
            return false;
        }

        const auto& ident = bin.lhs->as<ast::IdentExpr>();
        
        // try local scope first
        Symbol* sym = current_scope_->lookup(ident.name);
        if (sym) {
            if (!sym->is_mut) {
                error(std::format("cannot assign to immutable variable: {}", ident.name));
                return false;
            }
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
            return true;
        }
        
        // try globals
        auto it = globals_.find(ident.name);
        if (it != globals_.end()) {
            if (!it->second.is_mut) {
                error(std::format("cannot assign to immutable global: {}", ident.name));
                return false;
            }
            
            if (has_global_slot_ && dll_mode_) {
                // value is in RAX, save it
                emit_.push(x64::Reg::RAX);
                
                // load base pointer from rip-relative slot
                emit_lea_rip_slot(x64::Reg::RAX);
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0); // deref slot -> base ptr
                
                // pop value into RCX, store at base + offset
                emit_.pop(x64::Reg::RCX);
                emit_.mov_store(x64::Reg::RAX, static_cast<std::int32_t>(it->second.offset), x64::Reg::RCX);
                return true;
            }
            
            error("global variable assignment not supported in JIT mode");
            return false;
        }
        
        error(std::format("undefined variable: {}", ident.name));
        return false;
    }

    [[nodiscard]] bool generate_compound_assign(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;
        
        // handle field compound assign: player.health += 10
        if (bin.lhs->is<ast::FieldExpr>()) {
            const auto& field = bin.lhs->as<ast::FieldExpr>();
            if (!field.base->is<ast::IdentExpr>()) {
                error("compound assignment on chained fields not yet supported");
                return false;
            }
            
            const std::string& var_name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(var_name);
            if (!sym) {
                error(std::format("undefined variable: {}", var_name));
                return false;
            }
            
            std::string struct_name;
            auto sn = get_struct_name(sym->type);
            if (!sn) {
                error(std::format("variable '{}' is not a struct type", var_name));
                return false;
            }
            struct_name = *sn;
            
            auto struct_it = structs_.find(struct_name);
            if (struct_it == structs_.end()) {
                error(std::format("unknown struct: {}", struct_name));
                return false;
            }
            
            auto offset_opt = struct_it->second.get_field_offset(field.field);
            if (!offset_opt) {
                error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
                return false;
            }
            std::int32_t foff = static_cast<std::int32_t>(*offset_opt);
            
            // eval rhs
            if (!generate_expr(*bin.rhs)) return false;
            emit_.push(x64::Reg::RAX); // save rhs
            
            // load base pointer and current field value
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, foff); // current value
            
            // pop rhs into rcx
            emit_.pop(x64::Reg::RCX);
            
            // apply op
            switch (bin.op) {
                case Op::AddAssign: emit_.add(x64::Reg::RAX, x64::Reg::RCX); break;
                case Op::SubAssign: emit_.sub(x64::Reg::RAX, x64::Reg::RCX); break;
                case Op::MulAssign: emit_.imul(x64::Reg::RAX, x64::Reg::RCX); break;
                case Op::DivAssign: emit_.cqo(); emit_.idiv(x64::Reg::RCX); break;
                case Op::ModAssign: emit_.cqo(); emit_.idiv(x64::Reg::RCX); emit_.mov(x64::Reg::RAX, x64::Reg::RDX); break;
                default: error("unsupported compound op"); return false;
            }
            
            // store result back: reload base, write at offset
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX); // save result
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset); // reload base
            emit_.mov_store(x64::Reg::RAX, foff, x64::Reg::RCX);
            
            return true;
        }
        
        // left side must be an identifier
        if (!bin.lhs->is<ast::IdentExpr>()) {
            error("left side of compound assignment must be an identifier or field access");
            return false;
        }

        const auto& ident = bin.lhs->as<ast::IdentExpr>();
        Symbol* sym = current_scope_->lookup(ident.name);
        if (!sym) {
            error(std::format("undefined variable: {}", ident.name));
            return false;
        }
        if (!sym->is_mut) {
            error(std::format("cannot assign to immutable variable: {}", ident.name));
            return false;
        }

        // Generate right side -> RAX
        if (!generate_expr(*bin.rhs)) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // Save RHS to RCX
        
        // Load current value -> RAX
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
        
        // Apply operation
        switch (bin.op) {
            case Op::AddAssign:
                emit_.add(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::SubAssign:
                emit_.sub(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::MulAssign:
                emit_.imul(x64::Reg::RAX, x64::Reg::RCX);
                break;
            case Op::DivAssign:
                emit_.cqo();
                emit_.idiv(x64::Reg::RCX);
                break;
            case Op::ModAssign:
                emit_.cqo();
                emit_.idiv(x64::Reg::RCX);
                emit_.mov(x64::Reg::RAX, x64::Reg::RDX);  // remainder lives in rdx
                break;
            default:
                error("unsupported compound assignment operator");
                return false;
        }
        
        // Store result
        emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool generate_unary(const ast::UnaryExpr& un) {
        using Op = ast::UnaryExpr::Op;

        // Generate operand -> RAX
        if (!generate_expr(*un.operand)) return false;

        switch (un.op) {
            case Op::Neg:
                emit_.neg(x64::Reg::RAX);
                break;
            case Op::Not:
                emit_.test(x64::Reg::RAX, x64::Reg::RAX);
                emit_.setcc(x64::Emitter::CC_E, x64::Reg::RAX);  // Set if zero
                emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
                break;
            case Op::BitNot:
                emit_.not_(x64::Reg::RAX);
                break;
            case Op::Deref:
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
                break;
            case Op::AddrOf:
            case Op::AddrOfMut:
                // For identifiers, load the address
                if (un.operand->is<ast::IdentExpr>()) {
                    const auto& ident = un.operand->as<ast::IdentExpr>();
                    Symbol* sym = current_scope_->lookup(ident.name);
                    if (!sym) {
                        error(std::format("undefined variable: {}", ident.name));
                        return false;
                    }
                    emit_.lea(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                } else {
                    error("cannot take address of this expression");
                    return false;
                }
                break;
            case Op::PreInc:
            case Op::PreDec:
                // ++x or --x: increment then return
                if (un.operand->is<ast::IdentExpr>()) {
                    const auto& ident = un.operand->as<ast::IdentExpr>();
                    Symbol* sym = current_scope_->lookup(ident.name);
                    if (!sym) {
                        error(std::format("undefined variable: {}", ident.name));
                        return false;
                    }
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                    if (un.op == Op::PreInc) {
                        emit_.add_imm(x64::Reg::RAX, 1);
                    } else {
                        emit_.sub_imm(x64::Reg::RAX, 1);
                    }
                    emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
                } else {
                    error("increment/decrement requires a variable");
                    return false;
                }
                break;
            case Op::PostInc:
            case Op::PostDec:
                // x++ or x--: return then increment
                if (un.operand->is<ast::IdentExpr>()) {
                    const auto& ident = un.operand->as<ast::IdentExpr>();
                    Symbol* sym = current_scope_->lookup(ident.name);
                    if (!sym) {
                        error(std::format("undefined variable: {}", ident.name));
                        return false;
                    }
                    // Load current value (this is what we return)
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                    // Save to temp register
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                    // Increment/decrement
                    if (un.op == Op::PostInc) {
                        emit_.add_imm(x64::Reg::RCX, 1);
                    } else {
                        emit_.sub_imm(x64::Reg::RCX, 1);
                    }
                    // Store incremented value
                    emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RCX);
                    // RAX still has original value
                } else {
                    error("increment/decrement requires a variable");
                    return false;
                }
                break;
            default:
                error("unsupported unary operator");
                return false;
        }

        return true;
    }

    [[nodiscard]] bool generate_method_call(const ast::CallExpr& call) {
        // method call: player.damage(50) -> Player_damage(player, 50)
        // self is implicit first arg, so total args = 1 + call.args.size()
        const auto& field = call.callee->as<ast::FieldExpr>();
        
        if (!field.base->is<ast::IdentExpr>()) {
            error("method calls only supported on variables");
            return false;
        }
        
        const std::string& var_name = field.base->as<ast::IdentExpr>().name;
        const std::string& method_name = field.field;
        
        Symbol* sym = current_scope_->lookup(var_name);
        if (!sym) {
            error(std::format("undefined variable: {}", var_name));
            return false;
        }
        
        std::string class_name;
        auto sn = get_struct_name(sym->type);
        if (!sn) {
            error(std::format("variable '{}' is not a class type", var_name));
            return false;
        }
        class_name = *sn;
        
        std::string mangled_name = class_name + "_" + method_name;
        
        auto fn_it = functions_.find(mangled_name);
        if (fn_it == functions_.end()) {
            error(std::format("class '{}' has no method '{}'", class_name, method_name));
            return false;
        }
        
        // use same pattern as generate_call: push all args, alloc frame, load from pushed values
        std::size_t nargs = 1 + call.args.size(); // total including self
        
        // push self first
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
        emit_.push(x64::Reg::RAX);
        
        // push explicit args left to right
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }
        
        // allocate call frame
        std::size_t reg_args = nargs < 4 ? nargs : 4;
        std::size_t stack_args = nargs > 4 ? nargs - 4 : 0;
        std::size_t alloc = 32 + stack_args * 8;
        alloc = (alloc + 15) & ~15;
        emit_.sub_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc));
        
        // pushed args are at [RSP + alloc + 0] (last pushed = arg n-1) 
        // through [RSP + alloc + (n-1)*8] (first pushed = self = arg 0)
        // arg[i] is at [RSP + alloc + (nargs - 1 - i) * 8]
        
        // load first 4 into registers (RCX=self, RDX=arg0, R8=arg1, R9=arg2)
        for (std::size_t i = 0; i < reg_args; ++i) {
            std::int32_t off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            emit_.mov_load(x64::ARG_REGS[i], x64::Reg::RSP, off);
        }
        
        // load stack args (index 4+) into call frame
        for (std::size_t i = 4; i < nargs; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, src_off);
            emit_.mov_store(x64::Reg::RSP, static_cast<std::int32_t>(32 + (i - 4) * 8), x64::Reg::RAX);
        }
        
        // call
        emit_.buffer().emit8(0xE8);
        std::size_t fixup_site = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        call_fixups_.push_back({fixup_site, mangled_name});
        
        // clean up frame + pushed args
        emit_.add_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));
        
        return true;
    }

    [[nodiscard]] bool generate_call(const ast::CallExpr& call) {
        // Handle method calls: player.damage(50) -> Player_damage(player, 50)
        if (call.callee->is<ast::FieldExpr>()) {
            return generate_method_call(call);
        }
        
        // For regular calls, only support direct calls to known functions
        if (!call.callee->is<ast::IdentExpr>()) {
            error("only direct function calls are supported");
            return false;
        }

        const std::string& raw_fn_name = call.callee->as<ast::IdentExpr>().name;
        
        // normalize for builtin lookup only (dots -> underscores, lowercase)
        // user-defined calls keep their original casing
        std::string normalized;
        normalized.reserve(raw_fn_name.size());
        for (char c : raw_fn_name) {
            if (c == '.') {
                normalized += '_';
            } else if (c >= 'A' && c <= 'Z') {
                normalized += static_cast<char>(c + 32);
            } else {
                normalized += c;
            }
        }
        
        // builtin dispatch via lookup tables
        init_builtin_tables();

        // dll-mode builtins get first crack at the name
        if (dll_mode_) {
            auto dit = dll_builtins_.find(normalized);
            if (dit != dll_builtins_.end()) return dit->second(call);
        }

        // shared builtins (jit mode, or dll fallthrough for things like println)
        {
            auto jit = jit_builtins_.find(normalized);
            if (jit != jit_builtins_.end()) return jit->second(call);
        }
        
        // user-defined function - use original name to preserve casing
        auto it = functions_.find(raw_fn_name);
        if (it == functions_.end()) {
            error(std::format("undefined function: {}", raw_fn_name));
            return false;
        }

        // evaluate all args and push to temp stack
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }

        // pop all args into temp stack slots on our frame
        // we need to pop in reverse to get them in order
        std::size_t nargs = call.args.size();
        std::size_t reg_args = nargs < 4 ? nargs : 4;
        std::size_t stack_args = nargs > 4 ? nargs - 4 : 0;
        
        // pop into registers (reverse order)
        // but we need to handle stack args too, so pop everything first
        // strategy: pop all into temp, then set up call frame
        
        // pop in reverse into correct registers
        // args were pushed 0,1,2,...,n-1 so top of stack is arg[n-1]
        
        // for stack args: pop them into R10/R11 temp then store after frame setup
        // actually simpler: pop all into regs we can, save extras
        
        // simplest correct approach: pop all back, save to local slots, then set up call
        // but thats wasteful. lets just be smart about it.
        
        // pop args n-1 down to 4 into temp saves (these are stack args)
        // then pop args 3,2,1,0 into R9,R8,RDX,RCX
        
        // first handle stack args (pop in reverse = highest index first)
        // we need to save them somewhere temporarily
        // use R10, R11 for 2 stack args, or just push/pop around the frame setup
        
        // cleanest: allocate frame first, then copy from our pushed values
        // the pushed values are at [RSP+0], [RSP+8], ... (arg n-1 at top)
        
        // actually lets just do it the simple way:
        // 1. pop all args back into local stack slots
        // 2. set up call frame
        // 3. load from local slots into regs/stack positions
        
        // nah even simpler - just pop into regs for first 4, and for extras
        // we pop into RAX and immediately store to the call frame
        
        // allocate call frame FIRST, then reach past it to pop our saved args
        std::size_t alloc = 32 + stack_args * 8;
        alloc = (alloc + 15) & ~15;
        emit_.sub_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc));
        
        // our pushed args are now at [RSP + alloc + 0] (arg n-1) through [RSP + alloc + (n-1)*8] (arg 0)
        // arg[i] is at [RSP + alloc + (nargs - 1 - i) * 8]
        
        // load first 4 into registers
        for (std::size_t i = 0; i < reg_args; ++i) {
            std::int32_t off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            emit_.mov_load(x64::ARG_REGS[i], x64::Reg::RSP, off);
        }
        
        // load stack args into their positions
        for (std::size_t i = 4; i < nargs; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, src_off);
            emit_.mov_store(x64::Reg::RSP, static_cast<std::int32_t>(32 + (i - 4) * 8), x64::Reg::RAX);
        }

        // Emit call with placeholder offset (will be patched in fixup pass)
        emit_.buffer().emit8(0xE8);  // CALL rel32
        std::size_t fixup_site = emit_.buffer().pos();
        emit_.buffer().emit32(0);  // Placeholder - will be patched
        
        // Record fixup for later patching
        call_fixups_.push_back(CallFixup{
            .call_site = fixup_site,
            .target_fn = raw_fn_name
        });
        
        // Restore stack (alloc + pushed args)
        emit_.add_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));

        return true;
    }

    [[nodiscard]] bool generate_builtin_print_int(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("print_int requires an argument");
            return false;
        }

        // Generate argument -> RAX
        if (!generate_expr(*call.args[0])) return false;
        
        // Move to RCX (first arg for Windows x64)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        // Allocate shadow space (32 bytes for Windows x64)
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        // Load function pointer and call
        if (rt_.print_int) {
            // Load function pointer into RAX
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.print_int));
            // Call through RAX
            emit_.call(x64::Reg::RAX);
        }
        
        // Clean up shadow space
        emit_.add_imm(x64::Reg::RSP, 32);
        
        // Return 0 (print returns nothing useful)
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    // ========================================================================
    // dll mode builtins - relative calls into the embedded startup routines
    // offsets come from pe::DllGenerator constexpr chain
    // ========================================================================
    
    [[nodiscard]] bool generate_dll_builtin_print(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("dll_print requires a string argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(print_offset_);
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_set_title(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("set_title requires a string argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(set_title_offset_);
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_alloc_console([[maybe_unused]] const ast::CallExpr& call) {
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(alloc_console_offset_);
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_print_hex(const ast::CallExpr& call) {
        // prints value as "0x" + 8 hex digits + newline
        if (call.args.empty()) {
            error("print_int requires an argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        
        emit_.sub_imm(x64::Reg::RSP, 64);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        // build "0x" prefix at [rsp+0x20]
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24); 
        emit_.buffer().emit8(0x20); emit_.buffer().emit8(0x30);  // mov byte [rsp+0x20], '0'
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24); 
        emit_.buffer().emit8(0x21); emit_.buffer().emit8(0x78);  // mov byte [rsp+0x21], 'x'
        
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x54);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x22);  // lea rdx, [rsp+0x22]
        emit_.buffer().emit8(0x41); emit_.buffer().emit8(0xB0); emit_.buffer().emit8(0x08);  // mov r8b, 8
        
        // nibble extraction loop: shift top nibble out, convert to ascii, store
        std::size_t loop_start = emit_.buffer().pos();
        
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xCE);  // mov rsi, rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1); emit_.buffer().emit8(0xEE); emit_.buffer().emit8(0x1C);  // shr rsi, 28
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xE6); emit_.buffer().emit8(0x0F);  // and rsi, 0xF
        
        // nibble >= 10 ? 'A'-10 : '0'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xFE); emit_.buffer().emit8(0x0A);  // cmp rsi, 10
        emit_.buffer().emit8(0x72); emit_.buffer().emit8(0x05);                                                           // jb .digit
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x37);  // add rsi, 'A'-10
        emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x04);                                                           // jmp .store
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x30);  // add rsi, '0'
        
        emit_.buffer().emit8(0x40); emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x32);  // mov [rdx], sil
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);  // inc rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1); emit_.buffer().emit8(0xE1); emit_.buffer().emit8(0x04);  // shl rcx, 4
        
        emit_.buffer().emit8(0x41); emit_.buffer().emit8(0xFE); emit_.buffer().emit8(0xC8);  // dec r8b
        emit_.buffer().emit8(0x75);  // jnz
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().pos() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));
        
        // terminate and print
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x0A);  // mov byte [rdx], '\n'
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x42); emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00);  // mov byte [rdx+1], 0
        
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x4C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);  // lea rcx, [rsp+0x20]
        
        emit_startup_call(print_offset_);
        
        emit_.add_imm(x64::Reg::RSP, 64);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_print_dec(const ast::CallExpr& call) {
        // signed decimal print with negative handling
        if (call.args.empty()) {
            error("print_dec requires an argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        
        emit_.push(x64::Reg::RDI);
        emit_.push(x64::Reg::RSI);
        
        emit_.sub_imm(x64::Reg::RSP, 64);
        emit_.mov(x64::Reg::R8, x64::Reg::RAX);
        
        // r9 = is_negative flag
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xC9);  // xor r9, r9
        
        // check sign
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jns_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0x79); emit_.buffer().emit8(0x00);  // jns .positive
        
        // negative: negate r8, set r9=1
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xC7); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00);  // mov r9, 1
        
        // .positive:
        std::size_t positive_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(positive_label) - static_cast<std::int64_t>(jns_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jns_pos + 1] = static_cast<std::uint8_t>(off);
        }
        
        // rdi = end of stack buffer, fill backwards
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x7C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x3A);  // lea rdi, [rsp+0x3A]
        
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x00);  // mov byte [rdi], 0
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x0A);  // mov byte [rdi], '\n'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        
        // zero check
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jnz_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0x75); emit_.buffer().emit8(0x00);  // jnz .nonzero
        
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x30);  // mov byte [rdi], '0'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        std::size_t jmp_to_print_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x00);  // jmp .print
        
        // .nonzero:
        std::size_t after_zero = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(after_zero) - static_cast<std::int64_t>(jnz_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jnz_pos + 1] = static_cast<std::uint8_t>(off);
        }
        
        // div-by-10 loop on the now-positive value in r8
        emit_.buffer().emit8(0xB9); emit_.buffer().emit32(10);  // mov ecx, 10
        
        std::size_t loop_start = emit_.buffer().pos();
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov rax, r8
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xF1);  // div rcx
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov r8, rax (quotient)
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xC2); emit_.buffer().emit8(0x30);  // add dl, '0'
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x17);  // mov [rdi], dl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        emit_.buffer().emit8(0x75);  // jnz
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().pos() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));
        
        // prepend '-' if negative
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC9);  // test r9, r9
        std::size_t jz_nosign = emit_.buffer().pos();
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x00);  // jz .no_sign
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x2D);  // mov byte [rdi], '-'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        // .no_sign:
        std::size_t nosign_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(nosign_label) - static_cast<std::int64_t>(jz_nosign) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jz_nosign + 1] = static_cast<std::uint8_t>(off);
        }
        
        // .print:
        std::size_t print_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(print_label) - static_cast<std::int64_t>(jmp_to_print_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jmp_to_print_pos + 1] = static_cast<std::uint8_t>(off);
        }
        
        // inc rdi (point to first char)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC7);  // inc rdi
        
        // call dll_print_impl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xF9);  // mov rcx, rdi
        
        emit_startup_call(print_offset_);
        
        emit_.add_imm(x64::Reg::RSP, 64);
        
        emit_.pop(x64::Reg::RSI);
        emit_.pop(x64::Reg::RDI);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_crash([[maybe_unused]] const ast::CallExpr& call) {
        // write to null pointer - triggers access violation for crash handler testing
        emit_.buffer().emit8(0x48);  // REX.W
        emit_.buffer().emit8(0xC7);  // MOV r/m64, imm32
        emit_.buffer().emit8(0x04);  // SIB follows
        emit_.buffer().emit8(0x25);  // [disp32]
        emit_.buffer().emit32(0);    // address = 0
        emit_.buffer().emit32(0);    // value = 0
        return true;
    }
    
    [[nodiscard]] bool generate_dll_install_crash_handler([[maybe_unused]] const ast::CallExpr& call) {
        // register our VEH as first handler so it catches before anything else
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_.buffer().emit8(0xB9);
        emit_.buffer().emit32(1);  // ecx = 1 (first handler)
        
        // lea rdx to the crash handler in .text via rip-relative
        std::size_t current_offset = emit_.buffer().pos();
        std::int32_t crash_handler_rel = static_cast<std::int32_t>(crash_handler_offset_)
            - static_cast<std::int32_t>(user_code_offset_ + current_offset + 7);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x15);
        emit_.buffer().emit32(static_cast<std::uint32_t>(crash_handler_rel));
        
        emit_iat_call_raw(pe::iat::AddVectoredExceptionHandler);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }
    
    [[nodiscard]] bool generate_dll_mem_read(const ast::CallExpr& call, int size) {
        // mem_read(addr) - read value from memory
        if (call.args.empty()) {
            error("mem_read requires an address argument");
            return false;
        }
        
        // Generate address into RAX
        if (!generate_expr(*call.args[0])) return false;
        
        // Read from [RAX] into RAX
        switch (size) {
            case 1:
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xB6);
                emit_.buffer().emit8(0x00);  // movzx rax, byte [rax]
                break;
            case 2:
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xB7);
                emit_.buffer().emit8(0x00);  // movzx rax, word [rax]
                break;
            case 4:
                emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x00);  // mov eax, [rax] (zero-extends)
                break;
            case 8:
            default:
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x00);  // mov rax, [rax]
                break;
        }
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_mem_write(const ast::CallExpr& call, int size) {
        // mem_write(addr, value) - write value to memory
        if (call.args.size() < 2) {
            error("mem_write requires address and value arguments");
            return false;
        }
        
        // Generate value first, save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // Save value
        
        // Generate address
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // RCX = address
        
        emit_.pop(x64::Reg::RAX);  // RAX = value
        
        // Write to [RCX] from RAX
        switch (size) {
            case 1:
                emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x01);  // mov [rcx], al
                break;
            case 2:
                emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0x01);  // mov [rcx], ax
                break;
            case 4:
                emit_.buffer().emit8(0x89); emit_.buffer().emit8(0x01);  // mov [rcx], eax
                break;
            case 8:
            default:
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0x01);  // mov [rcx], rax
                break;
        }
        
        return true;
    }

    [[nodiscard]] bool generate_dll_memcpy(const ast::CallExpr& call) {
        // memcpy(dst, src, len) - copy memory
        if (call.args.size() < 3) {
            error("memcpy requires dst, src, and len arguments");
            return false;
        }
        
        // Save args to stack, then use rep movsb
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // len
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // src
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RDI, x64::Reg::RAX);  // dst -> RDI
        emit_.pop(x64::Reg::RSI);  // src -> RSI
        emit_.pop(x64::Reg::RCX);  // len -> RCX
        
        // rep movsb
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0xA4);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_memset(const ast::CallExpr& call) {
        // memset(ptr, value, len) - fill memory
        if (call.args.size() < 3) {
            error("memset requires ptr, value, and len arguments");
            return false;
        }
        
        emit_.push(x64::Reg::RDI);
        emit_.push(x64::Reg::RSI);
        
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // len
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // value
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RDI, x64::Reg::RAX);  // ptr -> RDI
        emit_.pop(x64::Reg::RAX);  // value -> AL
        emit_.pop(x64::Reg::RCX);  // len -> RCX
        
        // rep stosb
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0xAA);
        
        emit_.pop(x64::Reg::RSI);
        emit_.pop(x64::Reg::RDI);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_memcmp(const ast::CallExpr& call) {
        // memcmp(a, b, len) -> int - compare memory, returns 0 if equal
        if (call.args.size() < 3) {
            error("memcmp requires a, b, and len arguments");
            return false;
        }
        
        emit_.push(x64::Reg::RDI);
        emit_.push(x64::Reg::RSI);
        
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // len
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // b
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RSI, x64::Reg::RAX);  // a -> RSI
        emit_.pop(x64::Reg::RDI);  // b -> RDI
        emit_.pop(x64::Reg::RCX);  // len -> RCX
        
        // repe cmpsb
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0xA6);
        
        // setz al, movzx eax, al
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x94); emit_.buffer().emit8(0xC0);  // sete al
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0xC0);  // movzx eax, al
        // If equal, returns 1; otherwise 0. Invert for C-style (0 = equal)
        emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xF0); emit_.buffer().emit8(0x01);  // xor eax, 1
        
        emit_.pop(x64::Reg::RSI);
        emit_.pop(x64::Reg::RDI);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_sleep(const ast::CallExpr& call) {
        // sleep(ms) - call Windows Sleep
        if (call.args.empty()) {
            error("sleep requires milliseconds argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // ms -> RCX
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::Sleep);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_get_tick_count([[maybe_unused]] const ast::CallExpr& call) {
        // get_tick_count() -> int - call Windows GetTickCount64
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::GetTickCount64);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        // Result in RAX
        
        return true;
    }
    
    // Math functions using x87 FPU
    [[nodiscard]] bool generate_dll_sqrt(const ast::CallExpr& call) {
        if (call.args.empty()) { error("sqrt requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // Convert int to float, compute sqrt, convert back
        // For now: use SSE2 sqrtsd
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x51);
        emit_.buffer().emit8(0xC0);  // sqrtsd xmm0, xmm0
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_sin(const ast::CallExpr& call) {
        if (call.args.empty()) { error("sin requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // use x87: fld, fsin, fstp
        emit_.sub_imm(x64::Reg::RSP, 16);
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp]
        emit_.buffer().emit8(0xD9); emit_.buffer().emit8(0xFE);  // fsin
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x1C); emit_.buffer().emit8(0x24);  // fstp qword [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        emit_.add_imm(x64::Reg::RSP, 16);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_cos(const ast::CallExpr& call) {
        if (call.args.empty()) { error("cos requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        emit_.sub_imm(x64::Reg::RSP, 16);
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp]
        emit_.buffer().emit8(0xD9); emit_.buffer().emit8(0xFF);  // fcos
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x1C); emit_.buffer().emit8(0x24);  // fstp qword [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        emit_.add_imm(x64::Reg::RSP, 16);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_tan(const ast::CallExpr& call) {
        if (call.args.empty()) { error("tan requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        emit_.sub_imm(x64::Reg::RSP, 16);
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp]
        emit_.buffer().emit8(0xD9); emit_.buffer().emit8(0xF2);  // fptan
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0xD8);  // fstp st(0) - pop the 1.0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x1C); emit_.buffer().emit8(0x24);  // fstp qword [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        emit_.add_imm(x64::Reg::RSP, 16);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_atan2(const ast::CallExpr& call) {
        if (call.args.size() < 2) { error("atan2 requires y and x arguments"); return false; }
        
        // atan2(y, x) via x87 fpatan
        // allocate scratch space up front so all addressing is rsp-relative and stable
        emit_.sub_imm(x64::Reg::RSP, 16);
        
        // evaluate x, store to [rsp+8]
        if (!generate_expr(*call.args[1])) return false;
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x08);  // mov [rsp+8], rax
        
        // evaluate y into rax
        if (!generate_expr(*call.args[0])) return false;
        
        // convert y to double, load onto x87 stack
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp] - y
        
        // load x from saved slot, convert to double, load onto x87 stack
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x08);  // mov rax, [rsp+8]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp] - x
        
        // fpatan: st(1)/st(0) = y/x
        emit_.buffer().emit8(0xD9); emit_.buffer().emit8(0xF3);  // fpatan
        
        // store result back
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x1C); emit_.buffer().emit8(0x24);  // fstp qword [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        emit_.add_imm(x64::Reg::RSP, 16);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_floor(const ast::CallExpr& call) {
        if (call.args.empty()) { error("floor requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // roundsd with mode 1 (round down)
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x3A);
        emit_.buffer().emit8(0x0B); emit_.buffer().emit8(0xC0); emit_.buffer().emit8(0x01);  // roundsd xmm0, xmm0, 1
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_ceil(const ast::CallExpr& call) {
        if (call.args.empty()) { error("ceil requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // roundsd with mode 2 (round up)
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x3A);
        emit_.buffer().emit8(0x0B); emit_.buffer().emit8(0xC0); emit_.buffer().emit8(0x02);  // roundsd xmm0, xmm0, 2
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_abs(const ast::CallExpr& call) {
        if (call.args.empty()) { error("abs requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // Integer abs: mov rdx, rax; neg rax; cmovs rax, rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC2);  // mov rdx, rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x48);
        emit_.buffer().emit8(0xC2);  // cmovs rax, rdx
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_pow(const ast::CallExpr& call) {
        // pow(base, exp) - simple integer power for now
        if (call.args.size() < 2) { error("pow requires base and exponent arguments"); return false; }
        
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // exp
        if (!generate_expr(*call.args[0])) return false;
        // RAX = base, [rsp] = exp
        
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // base -> RCX
        emit_.pop(x64::Reg::RDX);  // exp -> RDX
        emit_.mov_imm32(x64::Reg::RAX, 1);  // result = 1
        
        // Loop: while (rdx > 0) { rax *= rcx; rdx--; }
        std::size_t loop_start = emit_.buffer().pos();
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xD2);  // test rdx, rdx
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x09);  // jz +9 (exit loop)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xAF);
        emit_.buffer().emit8(0xC1);  // imul rax, rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCA);  // dec rdx
        emit_.buffer().emit8(0xEB);  // jmp
        std::int8_t offset = static_cast<std::int8_t>(loop_start - emit_.buffer().pos() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(offset));
        
        return true;
    }

    [[nodiscard]] bool generate_dll_malloc(const ast::CallExpr& call) {
        // malloc(size) -> HeapAlloc(GetProcessHeap(), 0, size)
        if (call.args.empty()) {
            error("malloc requires size argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // save size on stack
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        // set up HeapAlloc args: rcx=heap, rdx=0, r8=size
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // restore size
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_free(const ast::CallExpr& call) {
        // free(ptr) - use Windows HeapFree  
        // HeapFree(GetProcessHeap(), 0, ptr)
        if (call.args.empty()) {
            error("free requires pointer argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // save ptr (GetProcessHeap clobbers volatiles)
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // ptr from stack
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }

    [[nodiscard]] bool generate_dll_array_new(const ast::CallExpr& call) {
        // array_new(capacity) -> heap ptr past 16-byte header
        // layout: [length 8b][capacity 8b][elements...]
        if (call.args.empty()) {
            error("array_new requires capacity argument");
            return false;
        }

        // eval capacity into rax, save it for header init later
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // [stack: capacity]

        // compute total alloc size: 16 + capacity * 8
        // shl rax, 3
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03);
        emit_.add_imm(x64::Reg::RAX, 16);
        emit_.push(x64::Reg::RAX);  // [stack: capacity, total_size]

        // get process heap
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);

        // HeapAlloc(heap, 0, total_size)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // total_size  [stack: capacity]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = allocation base

        // zero length: [rax+0] = 0
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);

        // set capacity: [rax+8] = capacity
        emit_.pop(x64::Reg::RCX);  // restore capacity  [stack: empty]
        emit_.mov_store(x64::Reg::RAX, 8, x64::Reg::RCX);

        // return ptr past header
        emit_.add_imm(x64::Reg::RAX, 16);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_get(const ast::CallExpr& call) {
        // array_get(arr, index) -> load 64-bit value at arr + index*8
        if (call.args.size() < 2) {
            error("array_get requires arr and index arguments");
            return false;
        }

        // eval arr, stash it
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);

        // eval index into rax
        if (!generate_expr(*call.args[1])) return false;

        // rax = index, pop arr into rcx
        emit_.pop(x64::Reg::RCX);

        // index * 8
        // shl rax, 3
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03);

        // arr + index*8
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);

        // load value at that address
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_set(const ast::CallExpr& call) {
        // array_set(arr, index, value) -> store value at arr + index*8, update length
        if (call.args.size() < 3) {
            error("array_set requires arr, index, and value arguments");
            return false;
        }

        // eval arr, index, value onto stack
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // arr

        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // index

        if (!generate_expr(*call.args[2])) return false;
        // rax = value, pop index and arr
        emit_.mov(x64::Reg::R8, x64::Reg::RAX);  // r8 = value
        emit_.pop(x64::Reg::RCX);  // rcx = index
        emit_.pop(x64::Reg::RDX);  // rdx = arr

        // store value at arr + index*8
        emit_.push(x64::Reg::RDX);  // save arr
        emit_.push(x64::Reg::RCX);  // save index
        emit_.mov(x64::Reg::RAX, x64::Reg::RCX);
        // shl rax, 3
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03);
        emit_.add(x64::Reg::RAX, x64::Reg::RDX);  // rax = arr + index*8
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::R8);  // [arr + index*8] = value

        // update length = max(current_length, index + 1)
        emit_.pop(x64::Reg::RCX);  // rcx = index
        emit_.pop(x64::Reg::RDX);  // rdx = arr
        emit_.add_imm(x64::Reg::RCX, 1);  // rcx = index + 1
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RDX, -16);  // rax = current length
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t skip_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);  // if current >= index+1, skip
        emit_.mov_store(x64::Reg::RDX, -16, x64::Reg::RCX);  // update length
        emit_.patch_jump(skip_patch);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_len(const ast::CallExpr& call) {
        // array_len(arr) -> load length from header at arr - 16
        if (call.args.empty()) {
            error("array_len requires arr argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = arr pointer, length is at arr - 16
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, -16);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_free(const ast::CallExpr& call) {
        // array_free(arr) -> HeapFree(heap, 0, arr - 16)
        if (call.args.empty()) {
            error("array_free requires arr argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = arr pointer, real alloc base is arr - 16
        emit_.sub_imm(x64::Reg::RAX, 16);
        emit_.push(x64::Reg::RAX);  // save alloc base (GetProcessHeap clobbers volatiles)

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);

        // HeapFree(heap, 0, alloc_ptr)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // alloc base from stack

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_imm(x64::Reg::RSP, 32);

        return true;
    }

    [[nodiscard]] bool generate_dll_string_length(const ast::CallExpr& call) {
        // inline strlen: scan bytes from str ptr until null, count in rax
        if (call.args.empty()) {
            error("string_length requires str argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = str pointer
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // rcx = ptr (walks the string)
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);  // rax = counter = 0

        // loop top
        emit_inline_strlen();

        return true;
    }

    [[nodiscard]] bool generate_dll_string_append(const ast::CallExpr& call) {
        // string_append(a, b) -> heap-allocated concatenation of a and b
        if (call.args.size() < 2) {
            error("string_append requires two string arguments");
            return false;
        }

        // eval both args onto stack
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // str_a
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // str_b
        // stack: [str_a, str_b]

        // --- inline strlen(a) ---
        // load str_a from [rsp+8]
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 8);
        emit_inline_strlen();
        emit_.push(x64::Reg::RAX);  // save len_a
        // stack: [str_a, str_b, len_a]

        // --- inline strlen(b) ---
        // load str_b from [rsp+8]
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 8);
        emit_inline_strlen();
        emit_.push(x64::Reg::RAX);  // save len_b
        // stack: [str_a, str_b, len_a, len_b]

        // --- HeapAlloc(len_a + len_b + 1) ---
        // compute total size in r8
        emit_.mov(x64::Reg::R8, x64::Reg::RAX);           // r8 = len_b
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 8);  // rax = len_a
        emit_.add(x64::Reg::R8, x64::Reg::RAX);            // r8 = len_a + len_b
        emit_.add_imm(x64::Reg::R8, 1);                     // r8 = len_a + len_b + 1
        emit_.push(x64::Reg::R8);  // save total_size (need it? no, but keeps stack aligned for reasoning)
        // stack: [str_a, str_b, len_a, len_b, total_size]

        // GetProcessHeap
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = heap handle

        // HeapAlloc(heap, 0, total_size)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // total_size -> r8
        // stack: [str_a, str_b, len_a, len_b]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = buffer ptr

        emit_.push(x64::Reg::RAX);  // save buffer
        // stack: [str_a, str_b, len_a, len_b, buffer]
        // offsets: buffer=[rsp+0], len_b=[rsp+8], len_a=[rsp+16], str_b=[rsp+24], str_a=[rsp+32]

        // --- memcpy a into buffer ---
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 32);  // str_a
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);             // buffer
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 16);    // len_a
        emit_inline_memcpy();
        // rdx now points to buffer + len_a

        // --- memcpy b into buffer + len_a ---
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 24);  // str_b
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 8);     // len_b
        emit_inline_memcpy();

        // null terminate: mov byte [rdx], 0
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);

        // return buffer ptr
        emit_.pop(x64::Reg::RAX);   // buffer
        emit_.add_imm(x64::Reg::RSP, 32);  // pop len_b, len_a, str_b, str_a
        // stack: clean

        return true;
    }

    [[nodiscard]] bool generate_dll_int_to_string(const ast::CallExpr& call) {
        // int_to_string(n) -> heap-allocated decimal string
        if (call.args.empty()) {
            error("int_to_string requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = value to convert
        emit_.push(x64::Reg::RAX);  // save value

        // heap alloc 24 bytes (enough for "-9223372036854775808\0")
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.mov_imm32(x64::Reg::R8, 24);  // 24 bytes
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = buffer

        emit_.push(x64::Reg::RAX);  // save buffer
        // stack: [value, buffer]

        // rdi = buffer + 22 (write backwards, leave room for null terminator)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x78); emit_.buffer().emit8(0x16);  // lea rdi, [rax+22]
        // null terminate
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x00);  // mov byte [rdi], 0
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi

        // load original value into r8
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 8);  // r8 = value from stack

        // check sign, r9 = is_negative flag
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xC9);  // xor r9, r9
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jns_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0x79); emit_.buffer().emit8(0x00);  // jns .positive (short jump)

        // negative: negate and set flag
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xC7); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00);  // mov r9, 1

        // .positive:
        std::size_t positive_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(positive_label) - static_cast<std::int64_t>(jns_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jns_pos + 1] = static_cast<std::uint8_t>(off);
        }

        // zero check
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jnz_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0x75); emit_.buffer().emit8(0x00);  // jnz .nonzero

        // its zero, just write '0'
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x30);  // mov byte [rdi], '0'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        std::size_t jmp_done_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x00);  // jmp .done

        // .nonzero: div-by-10 loop
        std::size_t nonzero_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(nonzero_label) - static_cast<std::int64_t>(jnz_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jnz_pos + 1] = static_cast<std::uint8_t>(off);
        }

        emit_.buffer().emit8(0xB9); emit_.buffer().emit32(10);  // mov ecx, 10

        std::size_t loop_start = emit_.buffer().pos();
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov rax, r8
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xF1);  // div rcx
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov r8, rax (quotient)
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xC2); emit_.buffer().emit8(0x30);  // add dl, '0'
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x17);  // mov [rdi], dl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        emit_.buffer().emit8(0x75);  // jnz .loop
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().pos() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));

        // .done:
        std::size_t done_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(done_label) - static_cast<std::int64_t>(jmp_done_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jmp_done_pos + 1] = static_cast<std::uint8_t>(off);
        }

        // prepend '-' if negative
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC9);  // test r9, r9
        std::size_t jz_nosign_pos = emit_.buffer().pos();
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x00);  // jz .no_sign

        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x2D);  // mov byte [rdi], '-'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi

        // .no_sign:
        std::size_t nosign_label = emit_.buffer().pos();
        {
            auto off = static_cast<std::int64_t>(nosign_label) - static_cast<std::int64_t>(jz_nosign_pos) - 2;
            if (off < -128 || off > 127) throw std::logic_error("short jump offset out of range");
            emit_.buffer().data()[jz_nosign_pos + 1] = static_cast<std::uint8_t>(off);
        }

        // rdi points one before first char, inc to get start of string
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC7);  // inc rdi

        // return pointer to start of string in rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xF8);  // mov rax, rdi

        // clean up stack: pop buffer (discard), pop value (discard)
        emit_.pop(x64::Reg::RCX);   // discard buffer
        emit_.pop(x64::Reg::RCX);   // discard value

        return true;
    }

    [[nodiscard]] bool generate_dll_string_equals(const ast::CallExpr& call) {
        // string_equals(a, b) -> 1 if identical, 0 if different
        if (call.args.size() < 2) {
            error("string_equals requires two string arguments");
            return false;
        }

        // eval a, push, eval b
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        // rax = b
        emit_.pop(x64::Reg::RCX);  // rcx = a
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);  // rdx = b

        // byte-by-byte compare loop
        std::size_t loop_top = emit_.buffer().pos();

        // movzx r8, byte [rcx]
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01);
        // movzx r9, byte [rdx]
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x0A);

        // cmp r8, r9
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x39);
        emit_.buffer().emit8(0xC8);

        // jne not_equal
        std::size_t jne_patch = emit_.jcc_rel32(x64::Emitter::CC_NE);

        // test r8, r8 (both bytes same, check if null)
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85);
        emit_.buffer().emit8(0xC0);

        // je equal (both null = strings match)
        std::size_t je_patch = emit_.jcc_rel32(x64::Emitter::CC_E);

        // inc rcx, inc rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);

        // jmp loop_top
        std::int32_t loop_rel = static_cast<std::int32_t>(
            loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        // not_equal: return 0
        emit_.patch_jump(jne_patch);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t done_jmp = emit_.jmp_rel32_placeholder();

        // equal: return 1
        emit_.patch_jump(je_patch);
        emit_.mov_imm32(x64::Reg::RAX, 1);

        // done
        emit_.patch_jump(done_jmp);

        return true;
    }

    [[nodiscard]] bool generate_dll_string_substring(const ast::CallExpr& call) {
        // string_substring(str, start, len) -> heap-allocated substring
        if (call.args.size() < 3) {
            error("string_substring requires str, start, and len arguments");
            return false;
        }

        // eval args onto stack
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // str
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // start
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // len
        // stack: [str, start, len]

        // --- HeapAlloc(len + 1) ---
        emit_.mov(x64::Reg::R8, x64::Reg::RAX);  // r8 = len
        emit_.add_imm(x64::Reg::R8, 1);            // r8 = len + 1
        emit_.push(x64::Reg::R8);  // save alloc_size
        // stack: [str, start, len, alloc_size]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = heap handle

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // alloc_size
        // stack: [str, start, len]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        // rax = buffer

        emit_.push(x64::Reg::RAX);  // save buffer
        // stack: [str, start, len, buffer]
        // offsets: buffer=[rsp+0], len=[rsp+8], start=[rsp+16], str=[rsp+24]

        // --- memcpy: copy len bytes from str+start to buffer ---
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);             // dst = buffer
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 8);      // r8 = len
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 24);    // rcx = str
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 16);    // rax = start
        emit_.add(x64::Reg::RCX, x64::Reg::RAX);              // rcx = str + start
        emit_inline_memcpy();

        // null terminate: mov byte [rdx], 0
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);

        // return buffer
        emit_.pop(x64::Reg::RAX);          // buffer
        emit_.add_imm(x64::Reg::RSP, 24);  // pop len, start, str

        return true;
    }

    [[nodiscard]] bool generate_dll_print_char(const ast::CallExpr& call) {
        // print a single char by making a tiny null-terminated string on the stack
        if (call.args.empty()) {
            error("print_char requires a character argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = char value

        // sub rsp, 16 ? stack space for our 2-byte string (aligned)
        emit_.sub_imm(x64::Reg::RSP, 16);

        // mov byte [rsp], al  (88 04 24)
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);

        // mov byte [rsp+1], 0  (C6 44 24 01 00)
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24);
        emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00);

        // mov rcx, rsp  (48 89 E1)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xE1);

        // call print routine (needs 32 bytes shadow space, but we already have 16 from our alloc
        // so just sub another 16 to get 32 total shadow above the call)
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(print_offset_);
        emit_.add_imm(x64::Reg::RSP, 32);

        // clean up our 16-byte string space
        emit_.add_imm(x64::Reg::RSP, 16);

        emit_.mov_imm32(x64::Reg::RAX, 0);
        return true;
    }

    [[nodiscard]] bool generate_dll_virtual_protect(const ast::CallExpr& call) {
        // virtual_protect(addr, size, new_protect) -> old_protect
        // VirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect)
        if (call.args.size() < 3) {
            error("virtual_protect requires addr, size, and new_protect arguments");
            return false;
        }
        
        // Generate args in reverse order
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // new_protect
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // size
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // addr -> RCX
        emit_.pop(x64::Reg::RDX);  // size -> RDX
        emit_.pop(x64::Reg::R8);   // new_protect -> R8
        
        // Allocate space for old_protect on stack
        emit_.sub_imm(x64::Reg::RSP, 48);  // 32 shadow + 8 for old_protect + alignment
        
        // R9 = &old_protect (at rsp+32)
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x4C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);  // lea r9, [rsp+0x20]
        
        emit_iat_call_raw(pe::iat::VirtualProtect);
        
        // Return old_protect
        emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24);
        emit_.buffer().emit8(0x20);  // mov eax, [rsp+0x20]
        
        emit_.add_imm(x64::Reg::RSP, 48);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_virtual_alloc(const ast::CallExpr& call) {
        // virtual_alloc(addr, size, type, protect) -> ptr
        // VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect)
        if (call.args.size() < 4) {
            error("virtual_alloc requires addr, size, type, and protect arguments");
            return false;
        }
        
        // Generate args
        if (!generate_expr(*call.args[3])) return false;
        emit_.push(x64::Reg::RAX);  // protect
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // type
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // size
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // addr -> RCX
        emit_.pop(x64::Reg::RDX);  // size -> RDX
        emit_.pop(x64::Reg::R8);   // type -> R8
        emit_.pop(x64::Reg::R9);   // protect -> R9
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::VirtualAlloc);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_virtual_free(const ast::CallExpr& call) {
        // virtual_free(addr, size, type) -> bool
        // VirtualFree(lpAddress, dwSize, dwFreeType)
        if (call.args.size() < 3) {
            error("virtual_free requires addr, size, and type arguments");
            return false;
        }
        
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // type
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // size
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // addr -> RCX
        emit_.pop(x64::Reg::RDX);  // size -> RDX
        emit_.pop(x64::Reg::R8);   // type -> R8
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::VirtualFree);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }

    [[nodiscard]] bool generate_dll_get_module(const ast::CallExpr& call) {
        // get_module(name) -> handle
        // GetModuleHandleA(lpModuleName)
        if (call.args.empty()) {
            error("get_module requires module name argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // name -> RCX
        
        // 32 bytes shadow space, rsp is already 0 mod 16 from prologue
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::GetModuleHandleA);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }

    [[nodiscard]] bool generate_dll_scan(const ast::CallExpr& call) {
        // scan(base, size, pattern) -> address or 0
        // SSE2 OPTIMIZED: uses pcmpeqb to check 16 bytes at once
        if (call.args.size() < 3) {
            error("scan requires base, size, and pattern arguments");
            return false;
        }
        
        // Parse pattern at compile time
        std::vector<std::uint8_t> bytes;
        std::vector<bool> mask;  // true = check this byte, false = wildcard
        
        const ast::Expr* pat_expr = call.args[2].get();
        if (std::holds_alternative<ast::LiteralExpr>(pat_expr->kind)) {
            const auto& lit = std::get<ast::LiteralExpr>(pat_expr->kind);
            if (auto* str = std::get_if<std::string>(&lit.value)) {
                std::string_view pat = *str;
                std::size_t i = 0;
                while (i < pat.size()) {
                    while (i < pat.size() && pat[i] == ' ') i++;
                    if (i >= pat.size()) break;
                    
                    if (pat[i] == '?') {
                        bytes.push_back(0);
                        mask.push_back(false);
                        i++;
                        if (i < pat.size() && pat[i] == '?') i++;
                    } else {
                        auto hex = [](char c) -> int {
                            if (c >= '0' && c <= '9') return c - '0';
                            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                            return -1;
                        };
                        int hi = hex(pat[i]);
                        int lo = (i + 1 < pat.size()) ? hex(pat[i + 1]) : -1;
                        if (hi >= 0 && lo >= 0) {
                            bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
                            mask.push_back(true);
                            i += 2;
                        } else {
                            i++;
                        }
                    }
                }
            }
        }
        
        if (bytes.empty()) {
            error("scan pattern must be a string literal");
            return false;
        }
        
        // Find first non-wildcard byte (anchor)
        std::size_t anchor_idx = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (mask[i]) {
                anchor_idx = i;
                break;
            }
        }
        std::uint8_t anchor_byte = bytes[anchor_idx];
        
        // anchor_idx is used as a disp8 in movdqu encoding, must fit signed byte
        if (static_cast<std::int64_t>(anchor_idx) < -128 || static_cast<std::int64_t>(anchor_idx) > 127)
            throw std::logic_error("short jump offset out of range");
        
        // Save callee-saved registers (Windows x64 ABI)
        emit_.push(x64::Reg::RDI);
        emit_.push(x64::Reg::RSI);
        emit_.push(x64::Reg::RBX);
        emit_.push(x64::Reg::R12);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        // Generate: size -> RSI, base -> RDI
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RDI, x64::Reg::RAX);
        emit_.pop(x64::Reg::RSI);
        
        // R12 = end = base + size - pattern_len
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x37);  // lea r12, [rdi + rsi]
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0x83);
        emit_.buffer().emit8(0xEC);
        emit_.buffer().emit8(static_cast<std::uint8_t>(bytes.size() + 16));  // sub r12, pattern_len+16
        
        // RDX = absolute end for scalar fallback
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x14); emit_.buffer().emit8(0x37);  // lea rdx, [rdi + rsi]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83);
        emit_.buffer().emit8(0xEA);
        emit_.buffer().emit8(static_cast<std::uint8_t>(bytes.size()));  // sub rdx, pattern_len
        
        // Broadcast anchor byte to XMM0 (16 copies)
        // mov eax, anchor_byte
        emit_.buffer().emit8(0xB8);
        emit_.buffer().emit32(anchor_byte);
        // movd xmm0, eax
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x6E); emit_.buffer().emit8(0xC0);
        // punpcklbw xmm0, xmm0
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x60); emit_.buffer().emit8(0xC0);
        // punpcklwd xmm0, xmm0
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x61); emit_.buffer().emit8(0xC0);
        // pshufd xmm0, xmm0, 0
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x70); emit_.buffer().emit8(0xC0);
        emit_.buffer().emit8(0x00);
        
        // Main SSE2 loop
        std::size_t loop_start = emit_.buffer().pos();
        
        // cmp rdi, r12; jae scalar_fallback
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x39);
        emit_.buffer().emit8(0xE7);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x83);
        std::size_t scalar_jmp = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // movdqu xmm1, [rdi + anchor_idx]
        emit_.buffer().emit8(0xF3); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x6F);
        if (anchor_idx == 0) {
            emit_.buffer().emit8(0x0F);
        } else {
            emit_.buffer().emit8(0x4F);
            emit_.buffer().emit8(static_cast<std::uint8_t>(anchor_idx));
        }
        
        // pcmpeqb xmm1, xmm0
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0xC8);
        
        // pmovmskb eax, xmm1
        emit_.buffer().emit8(0x66); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xD7); emit_.buffer().emit8(0xC1);
        
        // test eax, eax; jz next_16
        emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x84);
        std::size_t next16_jmp = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // Found matches! Save mask in EBX
        emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC3);
        
        std::size_t bit_loop = emit_.buffer().pos();
        
        // bsf ecx, ebx
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xBC);
        emit_.buffer().emit8(0xCB);
        
        // lea rsi, [rdi + rcx] - candidate
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x34); emit_.buffer().emit8(0x0F);
        
        // Verify full pattern at RSI
        std::vector<std::size_t> fail_jumps;
        for (std::size_t j = 0; j < bytes.size(); ++j) {
            if (mask[j]) {
                // cmp byte [rsi+j], bytes[j]
                if (j == 0) {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x3E);
                } else if (j < 128) {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x7E);
                    emit_.buffer().emit8(static_cast<std::uint8_t>(j));
                } else {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xBE);
                    emit_.buffer().emit32(static_cast<std::uint32_t>(j));
                }
                emit_.buffer().emit8(bytes[j]);
                
                // jne try_next_bit
                emit_.buffer().emit8(0x0F);
                emit_.buffer().emit8(0x85);
                fail_jumps.push_back(emit_.buffer().pos());
                emit_.buffer().emit32(0);
            }
        }
        
        // All matched! Return RSI
        emit_.mov(x64::Reg::RAX, x64::Reg::RSI);
        emit_.buffer().emit8(0xE9);  // jmp done
        std::size_t done_jmp = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // try_next_bit:
        std::size_t try_next_bit = emit_.buffer().pos();
        for (std::size_t pos : fail_jumps) {
            std::int32_t offset = static_cast<std::int32_t>(try_next_bit - pos - 4);
            auto* data = emit_.buffer().data();
            data[pos + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[pos + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[pos + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[pos + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        // btr ebx, ecx (clear this bit)
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xB3);
        emit_.buffer().emit8(0xCB);
        
        // test ebx, ebx; jnz bit_loop
        emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xDB);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x85);
        std::int32_t bit_back = static_cast<std::int32_t>(bit_loop) - 
                               static_cast<std::int32_t>(emit_.buffer().pos() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(bit_back));
        
        // next_16:
        std::size_t next16_label = emit_.buffer().pos();
        {
            std::int32_t offset = static_cast<std::int32_t>(next16_label - next16_jmp - 4);
            auto* data = emit_.buffer().data();
            data[next16_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[next16_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[next16_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[next16_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        // add rdi, 16; jmp loop_start
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83);
        emit_.buffer().emit8(0xC7); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0xE9);
        std::int32_t loop_back = static_cast<std::int32_t>(loop_start) - 
                                static_cast<std::int32_t>(emit_.buffer().pos() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(loop_back));
        
        // scalar_fallback:
        std::size_t scalar_label = emit_.buffer().pos();
        {
            std::int32_t offset = static_cast<std::int32_t>(scalar_label - scalar_jmp - 4);
            auto* data = emit_.buffer().data();
            data[scalar_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[scalar_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[scalar_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[scalar_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        // Scalar loop for tail
        std::size_t scalar_loop = emit_.buffer().pos();
        
        // cmp rdi, rdx; jae not_found
        emit_.cmp(x64::Reg::RDI, x64::Reg::RDX);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x83);
        std::size_t notfound_jmp = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // Check anchor
        if (anchor_idx == 0) {
            emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x3F);
        } else {
            emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x7F);
            emit_.buffer().emit8(static_cast<std::uint8_t>(anchor_idx));
        }
        emit_.buffer().emit8(anchor_byte);
        
        // jne scalar_next
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x85);
        std::size_t scalar_next_jmp = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // Verify full pattern
        std::vector<std::size_t> scalar_fail;
        for (std::size_t j = 0; j < bytes.size(); ++j) {
            if (mask[j] && j != anchor_idx) {
                if (j == 0) {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x3F);
                } else {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x7F);
                    emit_.buffer().emit8(static_cast<std::uint8_t>(j));
                }
                emit_.buffer().emit8(bytes[j]);
                emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x85);
                scalar_fail.push_back(emit_.buffer().pos());
                emit_.buffer().emit32(0);
            }
        }
        
        // Match! Return RDI
        emit_.mov(x64::Reg::RAX, x64::Reg::RDI);
        emit_.buffer().emit8(0xE9);
        std::size_t done_jmp2 = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        
        // scalar_next:
        std::size_t scalar_next = emit_.buffer().pos();
        {
            std::int32_t offset = static_cast<std::int32_t>(scalar_next - scalar_next_jmp - 4);
            auto* data = emit_.buffer().data();
            data[scalar_next_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[scalar_next_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[scalar_next_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[scalar_next_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        for (std::size_t pos : scalar_fail) {
            std::int32_t offset = static_cast<std::int32_t>(scalar_next - pos - 4);
            auto* data = emit_.buffer().data();
            data[pos + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[pos + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[pos + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[pos + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        // inc rdi; jmp scalar_loop
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC7);
        emit_.buffer().emit8(0xE9);
        std::int32_t scalar_back = static_cast<std::int32_t>(scalar_loop) - 
                                  static_cast<std::int32_t>(emit_.buffer().pos() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(scalar_back));
        
        // not_found:
        std::size_t notfound_label = emit_.buffer().pos();
        {
            std::int32_t offset = static_cast<std::int32_t>(notfound_label - notfound_jmp - 4);
            auto* data = emit_.buffer().data();
            data[notfound_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[notfound_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[notfound_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[notfound_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xC0);  // xor rax, rax
        
        // done:
        std::size_t done_label = emit_.buffer().pos();
        {
            std::int32_t offset = static_cast<std::int32_t>(done_label - done_jmp - 4);
            auto* data = emit_.buffer().data();
            data[done_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[done_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[done_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[done_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        {
            std::int32_t offset = static_cast<std::int32_t>(done_label - done_jmp2 - 4);
            auto* data = emit_.buffer().data();
            data[done_jmp2 + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[done_jmp2 + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[done_jmp2 + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[done_jmp2 + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.pop(x64::Reg::R12);
        emit_.pop(x64::Reg::RBX);
        emit_.pop(x64::Reg::RSI);
        emit_.pop(x64::Reg::RDI);
        
        return true;
    }

    // generic single-arg builtin caller
    [[nodiscard]] bool generate_builtin_one_arg(const ast::CallExpr& call, void* fn_ptr) {
        if (call.args.empty()) {
            error("builtin requires an argument");
            return false;
        }

        // Generate argument -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);  // Shadow space
        
        if (fn_ptr) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        // RAX now has return value
        
        return true;
    }
    
    // Variadic print: print(a, b, c) or println(a, b, c)
    [[nodiscard]] bool generate_builtin_print_variadic(const ast::CallExpr& call, bool add_newline) {
        // For each argument, print it
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            const auto& arg = call.args[i];
            
            // Add space before (except first arg)
            if (i > 0) {
                // Print a space character
                if (dll_mode_ && rt_.print_str) {
                    // Skip space for now - just print args directly
                }
            }
            
            // Determine type and print accordingly
            // For string literals, call print_string
            // For other expressions, call print_int
            if (arg->is<ast::LiteralExpr>()) {
                const auto& lit = arg->as<ast::LiteralExpr>();
                if (std::holds_alternative<std::string>(lit.value)) {
                    // String literal - call print
                    if (!generate_expr(*arg)) return false;
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                    emit_.sub_imm(x64::Reg::RSP, 32);
                    if (rt_.print_str) {
                        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.print_str));
                        emit_.call(x64::Reg::RAX);
                    } else if (dll_mode_) {
                        // DLL mode: call print stub
                        emit_.buffer().emit8(0xE8);  // call rel32
                        std::int32_t rel = static_cast<std::int32_t>(0x100) - 
                            static_cast<std::int32_t>(0x240 + emit_.buffer().pos() + 4);
                        emit_.buffer().emit32(static_cast<std::uint32_t>(rel));
                    }
                    emit_.add_imm(x64::Reg::RSP, 32);
                    continue;
                }
            }
            
            // Integer/other types - use print_int
            if (!generate_expr(*arg)) return false;
            
            if (dll_mode_) {
                // DLL mode: replicate what generate_dll_builtin_print_hex does
                // but with value already in RAX
                emit_.sub_imm(x64::Reg::RSP, 64);
                emit_.mov(x64::Reg::R8, x64::Reg::RAX);  // Save value in R8
                
                // Create hex string in stack buffer
                // Point RDX to start of buffer (rsp+0x20)
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                emit_.buffer().emit8(0x54); emit_.buffer().emit8(0x24);
                emit_.buffer().emit8(0x20);  // lea rdx, [rsp+0x20]
                
                // Write "0x" prefix
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02);
                emit_.buffer().emit8(0x30);  // mov byte [rdx], '0'
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x42);
                emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x78);  // mov byte [rdx+1], 'x'
                
                // Add 2 to RDX
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x83);
                emit_.buffer().emit8(0xC2); emit_.buffer().emit8(0x02);  // add rdx, 2
                
                // Convert to hex: 8 nibbles
                emit_.buffer().emit8(0xB9); emit_.buffer().emit32(8);  // mov ecx, 8
                
                std::size_t loop_start = emit_.buffer().pos();
                
                // mov rax, r8; rol rax, 4 (rotate left to get high nibble)
                emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xC1);
                emit_.buffer().emit8(0xC0); emit_.buffer().emit8(0x04);  // rol r8, 4
                emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x89);
                emit_.buffer().emit8(0xC0);  // mov rax, r8
                emit_.buffer().emit8(0x83); emit_.buffer().emit8(0xE0);
                emit_.buffer().emit8(0x0F);  // and eax, 0xF
                
                // Convert nibble to hex char
                emit_.buffer().emit8(0x3C); emit_.buffer().emit8(0x0A);  // cmp al, 10
                emit_.buffer().emit8(0x7C); emit_.buffer().emit8(0x04);  // jl +4
                emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x37);  // add al, 55 ('A'-10)
                emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x02);  // jmp +2
                emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x30);  // add al, '0'
                
                // Store char and advance
                emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x02);  // mov [rdx], al
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF);
                emit_.buffer().emit8(0xC2);  // inc rdx
                
                // Loop
                emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC9);  // dec ecx
                emit_.buffer().emit8(0x75);  // jnz
                std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().pos() - 1);
                emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));
                
                // Null terminate
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02);
                emit_.buffer().emit8(0x00);  // mov byte [rdx], 0
                
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x24);
                emit_.buffer().emit8(0x20);  // lea rcx, [rsp+0x20]
                
                emit_startup_call(print_offset_);
                
                emit_.add_imm(x64::Reg::RSP, 64);
            } else if (rt_.print_int) {
                emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.print_int));
                emit_.call(x64::Reg::RAX);
                emit_.add_imm(x64::Reg::RSP, 32);
            }
        }
        
        // Add newline if println
        if (add_newline) {
            if (dll_mode_) {
                // Allocate newline string on stack
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x04);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x0A);  // mov byte [rsp], '\n'
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x01);
                emit_.buffer().emit8(0x00);  // mov byte [rsp+1], 0
                emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
                
                emit_startup_call(print_offset_);
                
                emit_.add_imm(x64::Reg::RSP, 32);
            } else if (rt_.print_str) {
                // jit mode doesnt have println yet
            }
        }
        
        return true;
    }

    // Two-argument builtin (like write_file, array_get, string_append)
    [[nodiscard]] bool generate_builtin_two_arg(const ast::CallExpr& call, void* fn_ptr) {
        if (call.args.size() < 2) {
            error("builtin requires two arguments");
            return false;
        }

        // Generate second arg -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate first arg -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        // Pop second arg into RDX
        emit_.pop(x64::Reg::RDX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        if (fn_ptr) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    // Three-argument builtin (like array_set)
    [[nodiscard]] bool generate_builtin_three_arg(const ast::CallExpr& call, void* fn_ptr) {
        if (call.args.size() < 3) {
            error("builtin requires three arguments");
            return false;
        }

        // Generate third arg -> save to stack
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate second arg -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate first arg -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        // Pop second arg into RDX
        emit_.pop(x64::Reg::RDX);
        
        // Pop third arg into R8
        emit_.pop(x64::Reg::R8);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        if (fn_ptr) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    // string_get_char(handle, index) - two args
    [[nodiscard]] bool generate_builtin_string_get_char(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("string_get_char requires handle and index arguments");
            return false;
        }

        // Generate second arg (index) -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate first arg (handle) -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        
        // Pop index into RDX
        emit_.pop(x64::Reg::RDX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        if (rt_.string_get_char) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.string_get_char));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        // RAX has the character
        
        return true;
    }

    // ========================================================================
    // memory ops - direct inline codegen, no function call overhead
    // ========================================================================

    [[nodiscard]] bool generate_builtin_mem_read_i64(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("mem_read requires an address argument");
            return false;
        }
        // Generate address -> RAX
        if (!generate_expr(*call.args[0])) return false;
        // Read 64-bit value at address: mov RAX, [RAX]
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_read_i32(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("mem_read_i32 requires an address argument");
            return false;
        }
        // Generate address -> RAX
        if (!generate_expr(*call.args[0])) return false;
        // Read 32-bit value at address and sign-extend: movsxd RAX, [RAX]
        // For now, use 32-bit load + sign extension
        // mov eax, [rax] - this zero-extends to 64-bit
        emit_.buffer().emit8(0x8B);  // mov r32, r/m32
        emit_.buffer().emit8(0x00);  // ModR/M: [RAX]
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_read_i16(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("mem_read_i16 requires an address argument");
            return false;
        }
        if (!generate_expr(*call.args[0])) return false;
        // movzx eax, word ptr [rax]
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB7);
        emit_.buffer().emit8(0x00);  // ModR/M: [RAX]
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_read_i8(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("mem_read_i8 requires an address argument");
            return false;
        }
        if (!generate_expr(*call.args[0])) return false;
        // movzx eax, byte ptr [rax]
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6);
        emit_.buffer().emit8(0x00);  // ModR/M: [RAX]
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_write_i64(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("mem_write requires address and value arguments");
            return false;
        }
        // Generate value -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate address -> RAX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // Address in RCX
        
        // Pop value into RAX
        emit_.pop(x64::Reg::RAX);
        
        // Write: mov [RCX], RAX
        emit_.mov_store(x64::Reg::RCX, 0, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_write_i32(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("mem_write_i32 requires address and value arguments");
            return false;
        }
        // Generate value -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        
        // Generate address -> RAX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // Address in RCX
        
        // Pop value into EAX
        emit_.pop(x64::Reg::RAX);
        
        // Write 32-bit: mov [RCX], EAX
        emit_.buffer().emit8(0x89);
        emit_.buffer().emit8(0x01);  // ModR/M: [RCX]
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_write_i16(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("mem_write_i16 requires address and value arguments");
            return false;
        }
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RAX);
        // Write 16-bit: mov [RCX], AX (with 66 prefix)
        emit_.buffer().emit8(0x66);
        emit_.buffer().emit8(0x89);
        emit_.buffer().emit8(0x01);
        return true;
    }

    [[nodiscard]] bool generate_builtin_mem_write_i8(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("mem_write_i8 requires address and value arguments");
            return false;
        }
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RAX);
        // Write 8-bit: mov [RCX], AL
        emit_.buffer().emit8(0x88);
        emit_.buffer().emit8(0x01);
        return true;
    }

    // ========================================================================
    // ffi - runtime function lookups via extern C declarations
    // ========================================================================

    // Helper to get FFI function pointer by name (uses extern C declarations)
    void* get_ffi_fn(const std::string& name) {
        static const std::unordered_map<std::string_view, void*> ffi_table = {
            {"get_module",              reinterpret_cast<void*>(&opus_get_module)},
            {"load_library",            reinterpret_cast<void*>(&opus_load_library)},
            {"get_proc",                reinterpret_cast<void*>(&opus_get_proc)},
            {"ffi_call0",               reinterpret_cast<void*>(&opus_ffi_call0)},
            {"ffi_call1",               reinterpret_cast<void*>(&opus_ffi_call1)},
            {"ffi_call2",               reinterpret_cast<void*>(&opus_ffi_call2)},
            {"ffi_call3",               reinterpret_cast<void*>(&opus_ffi_call3)},
            {"ffi_call4",               reinterpret_cast<void*>(&opus_ffi_call4)},
            {"msgbox",                  reinterpret_cast<void*>(&opus_msgbox)},
            {"get_last_error",          reinterpret_cast<void*>(&opus_get_last_error)},
            {"virtual_protect",         reinterpret_cast<void*>(&opus_virtual_protect)},
            {"get_current_process",     reinterpret_cast<void*>(&opus_get_current_process)},
            {"get_current_process_id",  reinterpret_cast<void*>(&opus_get_current_process_id)},
            {"alloc_console",           reinterpret_cast<void*>(&opus_alloc_console)},
            {"free_console",            reinterpret_cast<void*>(&opus_free_console)},
            {"set_console_title",       reinterpret_cast<void*>(&opus_set_console_title)},
        };
        auto it = ffi_table.find(name);
        return it != ffi_table.end() ? it->second : nullptr;
    }

    [[nodiscard]] bool generate_builtin_ffi_zero_arg(const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_one_arg(const ast::CallExpr& call, const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.empty()) {
            // No argument means null/0
            emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        } else {
            if (!generate_expr(*call.args[0])) return false;
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        }
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_two_arg(const ast::CallExpr& call, const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.size() < 2) {
            error(std::format("{} requires 2 arguments", fn_name));
            return false;
        }
        // Generate second arg -> save to stack
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        // Generate first arg -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        // Pop second arg into RDX
        emit_.pop(x64::Reg::RDX);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_three_arg(const ast::CallExpr& call, const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.size() < 3) {
            error(std::format("{} requires 3 arguments", fn_name));
            return false;
        }
        // Generate third arg -> save
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);
        // Generate second arg -> save
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        // Generate first arg -> RCX
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        // Pop second into RDX
        emit_.pop(x64::Reg::RDX);
        // Pop third into R8
        emit_.pop(x64::Reg::R8);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_four_arg(const ast::CallExpr& call, const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.size() < 4) {
            error(std::format("{} requires 4 arguments", fn_name));
            return false;
        }
        // Generate all args, push in reverse
        if (!generate_expr(*call.args[3])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.pop(x64::Reg::R8);
        emit_.pop(x64::Reg::R9);
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_five_arg(const ast::CallExpr& call, const std::string& fn_name) {
        void* fn_ptr = get_ffi_fn(fn_name);
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.size() < 5) {
            error(std::format("{} requires 5 arguments", fn_name));
            return false;
        }
        // For 5+ args, 5th goes on stack after shadow space
        if (!generate_expr(*call.args[4])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[3])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.pop(x64::Reg::R8);
        emit_.pop(x64::Reg::R9);
        // 5th arg stays on stack (part of shadow space usage)
        
        emit_.sub_imm(x64::Reg::RSP, 48);  // shadow space + 5th arg + alignment
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 48);  // Clean up including pushed 5th arg
        return true;
    }

    // ========================================================================
    // threading codegen - spawn, await, parallel for, atomics
    // ========================================================================

    // emit a thread entry stub for the given function
    // conforms to LPTHREAD_START_ROUTINE (stdcall, returns DWORD)
    // rcx = pointer to Thread_Context
    std::size_t emit_thread_entry_stub(const std::string& fn_name) {
        auto it = thread_stubs_.find(fn_name);
        if (it != thread_stubs_.end()) return it->second;

        std::size_t stub_offset = emit_.buffer().pos();
        thread_stubs_[fn_name] = stub_offset;

        // prologue
        emit_.push(x64::Reg::RBP);
        emit_.mov(x64::Reg::RBP, x64::Reg::RSP);
        emit_.push(x64::Reg::R12);
        emit_.sub_imm(x64::Reg::RSP, 0x28);

        // r12 = context ptr (callee-saved so it survives the call)
        emit_.mov(x64::Reg::R12, x64::Reg::RCX);

        // load function ptr and args from context
        emit_.mov_load(x64::Reg::RAX, x64::Reg::R12, CTX_FN_PTR);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::R12, CTX_ARG0);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::R12, CTX_ARG1);
        emit_.mov_load(x64::Reg::R8, x64::Reg::R12, CTX_ARG2);
        emit_.call(x64::Reg::RAX);

        // store result
        emit_.mov_store(x64::Reg::R12, CTX_RESULT, x64::Reg::RAX);

        // return 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_imm(x64::Reg::RSP, 0x28);
        emit_.pop(x64::Reg::R12);
        emit_.pop(x64::Reg::RBP);
        emit_.ret();

        return stub_offset;
    }

    // spawn expression codegen
    // allocates Thread_Context in current stack frame, calls CreateThread
    // returns handle in rax, stores context ptr at [rbp + spawn_ctx_offset_]
    [[nodiscard]] bool generate_spawn(const ast::SpawnExpr& spawn) {
        if (!spawn.callee->is<ast::IdentExpr>()) {
            error("spawn requires a function name");
            return false;
        }
        const std::string& fn_name = spawn.callee->as<ast::IdentExpr>().name;

        auto fn_it = functions_.find(fn_name);
        if (fn_it == functions_.end()) {
            error(std::format("unknown function '{}' in spawn expression", fn_name));
            return false;
        }

        // emit thread entry stub (reuses if already emitted)
        // jump over it so main doesnt execute stub code inline
        std::size_t pre_stub_pos = emit_.buffer().pos();
        std::size_t jmp_over_stub = emit_.jmp_rel32_placeholder();
        std::size_t stub_offset = emit_thread_entry_stub(fn_name);
        if (emit_.buffer().pos() == pre_stub_pos + 5) {
            // stub was cached, no new code emitted - the jump lands right here
        }
        emit_.patch_jump(jmp_over_stub);

        // allocate context in current scope (48 bytes = 6 slots)
        std::size_t sid = spawn_counter_++;
        auto& ctx_sym = current_scope_->define("__spawn_ctx_" + std::to_string(sid),
            Type::make_primitive(PrimitiveType::I64), true);
        std::int32_t ctx_base = ctx_sym.stack_offset;
        // reserve 5 more slots (total 48 bytes)
        for (int i = 0; i < 5; ++i) {
            current_scope_->next_offset -= 8;
        }

        // allocate a 2-slot spawn result: [handle, ctx_ptr]
        // await reads both from this struct instead of a LIFO side-stack
        auto& result_sym = current_scope_->define("__spawn_result_" + std::to_string(sid),
            Type::make_primitive(PrimitiveType::I64), true);
        std::int32_t result_base = result_sym.stack_offset;
        current_scope_->next_offset -= 8; // second slot for ctx_ptr

        // evaluate args first (before we fill the context, since eval can trash regs)
        std::vector<std::int32_t> arg_temps;
        for (std::size_t i = 0; i < spawn.args.size() && i < 3; ++i) {
            if (!generate_expr(*spawn.args[i])) return false;
            auto& tmp = current_scope_->define("__spawn_arg_" + std::to_string(i) + "_" + std::to_string(sid),
                Type::make_primitive(PrimitiveType::I64), true);
            emit_.mov_store(x64::Reg::RBP, tmp.stack_offset, x64::Reg::RAX);
            arg_temps.push_back(tmp.stack_offset);
        }

        // fill context: the 48-byte block goes from ctx_base-40 (low) to ctx_base (high)
        // context pointer = rbp + ctx_base - 40 (lowest address)
        // layout: [fn_ptr, arg0, arg1, arg2, result, padding]
        std::int32_t ctx_ptr_off = ctx_base - 40;

        // store function ptr at [rbp + ctx_ptr_off]
        std::size_t fn_addr_fixup = emit_lea_rip_disp32(x64::Reg::RAX);
        call_fixups_.push_back({fn_addr_fixup, fn_name});
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_off, x64::Reg::RAX);

        // fill args into context slots
        for (std::size_t i = 0; i < arg_temps.size(); ++i) {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, arg_temps[i]);
            emit_.mov_store(x64::Reg::RBP, ctx_ptr_off + static_cast<std::int32_t>((i + 1) * 8), x64::Reg::RAX);
        }

        // zero result slot
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_off + CTX_RESULT, x64::Reg::RAX);

        // compute context base address into a temp
        auto& ctx_ptr_sym = current_scope_->define("__spawn_ctxptr_" + std::to_string(sid),
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.lea(x64::Reg::RAX, x64::Reg::RBP, ctx_ptr_off);
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_sym.stack_offset, x64::Reg::RAX);

        // set up CreateThread args
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        // r8 = stub address
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea r8, [rip+disp32]
        std::size_t stub_fixup = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        std::int32_t stub_rel = static_cast<std::int32_t>(stub_offset)
            - static_cast<std::int32_t>(stub_fixup + 4);
        emit_.buffer().patch32(stub_fixup, static_cast<std::uint32_t>(stub_rel));
        // r9 = context ptr
        emit_.lea(x64::Reg::R9, x64::Reg::RBP, ctx_ptr_off);
        // 5th arg: dwCreationFlags = 0 (on stack)
        // 6th arg: lpThreadId = NULL (on stack)
        emit_.sub_imm(x64::Reg::RSP, 48); // shadow(32) + 2 args(16)
        emit_.mov_imm32(x64::Reg::RAX, 0);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX); // dwCreationFlags = 0
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX); // lpThreadId = NULL

        emit_iat_call_raw(pe::iat::CreateThread);
        emit_.add_imm(x64::Reg::RSP, 48);

        // rax = thread HANDLE
        // pack [handle, ctx_ptr] into the spawn result struct
        // await will read both from this pointer, no LIFO ordering needed
        emit_.mov_store(x64::Reg::RBP, result_base, x64::Reg::RAX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ctx_ptr_sym.stack_offset);
        emit_.mov_store(x64::Reg::RBP, result_base - 8, x64::Reg::RCX);

        // return pointer to the spawn result struct
        emit_.lea(x64::Reg::RAX, x64::Reg::RBP, result_base);

        return true;
    }

    // await expression codegen
    // waits for thread handle, reads result from context, cleans up
    [[nodiscard]] bool generate_await(const ast::AwaitExpr& await_expr) {
        // evaluate handle expression -> rax (pointer to spawn result: [handle, ctx_ptr])
        if (!generate_expr(*await_expr.handle)) return false;

        // rax points to spawn result struct
        // load handle from [rax+0], ctx_ptr from [rax-8]
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RAX, -8); // ctx_ptr
        emit_.push(x64::Reg::RCX); // save ctx_ptr
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);  // handle

        // save handle
        emit_.push(x64::Reg::RAX);

        // WaitForSingleObject(handle, INFINITE)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.mov_imm32(x64::Reg::RDX, -1); // INFINITE = 0xFFFFFFFF
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::WaitForSingleObject);
        emit_.add_imm(x64::Reg::RSP, 32);

        // CloseHandle(handle)
        emit_.pop(x64::Reg::RCX); // restore handle
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.add_imm(x64::Reg::RSP, 32);

        // load result from context
        emit_.pop(x64::Reg::RAX); // restore ctx_ptr
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, CTX_RESULT);

        return true;
    }

    // captured variable info for pfor body emission
    struct CapturedVar {
        std::string name;
        std::int32_t parent_offset;
    };

    // emits the inner loop function that each worker thread runs
    // registers body_fn in functions_, emits stub, patches jmp_over
    bool emit_pfor_body_function(
        const ast::ParallelForStmt& pfor,
        const std::vector<CapturedVar>& captures,
        PforLayout& layout,
        std::size_t jmp_over
    ) {
        functions_[layout.body_fn] = FunctionInfo{
            .name = layout.body_fn,
            .return_type = Type::make_primitive(PrimitiveType::I64)
        };
        auto& body_info = functions_[layout.body_fn];
        body_info.code_offset = emit_.buffer().pos();

        push_scope();
        emit_.prologue(1024);

        // spill params: rcx=start, rdx=end, r8=parent_rbp
        auto& iter_start = current_scope_->define("__iter_start",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, iter_start.stack_offset, x64::Reg::RCX);
        auto& iter_end = current_scope_->define("__iter_end",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, iter_end.stack_offset, x64::Reg::RDX);
        auto& parent_rbp_sym = current_scope_->define("__parent_rbp",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, parent_rbp_sym.stack_offset, x64::Reg::R8);

        // copy captured variables from parent frame into our local frame
        // r8 still has parent rbp at this point
        for (const auto& cap : captures) {
            auto& local = current_scope_->define(cap.name,
                Type::make_primitive(PrimitiveType::I64), true);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::R8, cap.parent_offset);
            emit_.mov_store(x64::Reg::RBP, local.stack_offset, x64::Reg::RAX);
        }

        // loop variable
        auto& loop_var = current_scope_->define(pfor.name,
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, iter_start.stack_offset);
        emit_.mov_store(x64::Reg::RBP, loop_var.stack_offset, x64::Reg::RAX);

        // inner loop: while loop_var < iter_end
        std::size_t loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, loop_var.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, iter_end.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        for (const auto& stmt : pfor.body) {
            if (!generate_stmt(*stmt)) { pop_scope(); return false; }
        }

        // increment and loop back
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, loop_var.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, loop_var.stack_offset, x64::Reg::RAX);

        std::int32_t loop_rel = static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);
        emit_.patch_jump(loop_exit);

        // return 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.epilogue();
        pop_scope();
        body_info.code_size = emit_.buffer().pos() - body_info.code_offset;

        layout.stub_offset = emit_thread_entry_stub(layout.body_fn);
        emit_.patch_jump(jmp_over);
        return true;
    }

    // emits the CreateThread loop that spawns one worker per core
    void emit_pfor_spawn_loop(const PforLayout& layout) {
        // get cpu count via GetSystemInfo (48 byte struct, dwNumberOfProcessors at offset 32)
        emit_.sub_imm(x64::Reg::RSP, 48);
        emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetSystemInfo);
        emit_.add_imm(x64::Reg::RSP, 32);
        // mov eax, [rsp+0x20] ? read dwNumberOfProcessors
        emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);
        emit_.add_imm(x64::Reg::RSP, 48);

        emit_.mov_store(x64::Reg::RBP, layout.ncores_off, x64::Reg::RAX);

        // cap at PFOR_MAX_THREADS
        emit_.cmp_imm(x64::Reg::RAX, PFOR_MAX_THREADS);
        std::size_t cap_skip = emit_.jcc_rel32(x64::Emitter::CC_LE);
        emit_.mov_imm32(x64::Reg::RAX, PFOR_MAX_THREADS);
        emit_.mov_store(x64::Reg::RBP, layout.ncores_off, x64::Reg::RAX);
        emit_.patch_jump(cap_skip);

        // range = end - start
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.end_off);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.start_off);
        emit_.sub(x64::Reg::RAX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RBP, layout.range_off, x64::Reg::RAX);

        // cap ncores to range so we dont spawn useless threads
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t range_ok = emit_.jcc_rel32(x64::Emitter::CC_GE);
        emit_.mov_store(x64::Reg::RBP, layout.ncores_off, x64::Reg::RAX);
        emit_.patch_jump(range_ok);

        // chunk_size = range / ncores (rax still has range from above)
        emit_.cqo();
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.idiv(x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RBP, layout.chunk_off, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, layout.rem_off, x64::Reg::RDX);

        // grow rsp to cover all our locals so function calls dont clobber them
        std::int32_t frame_needed = (-current_scope_->next_offset + 15) & ~15;
        emit_.sub_imm(x64::Reg::RSP, frame_needed);

        // i = 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, layout.idx_off, x64::Reg::RAX);

        std::size_t spawn_loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t spawn_loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // context address: ctx_arr_base + i * PFOR_CTX_SIZE
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RAX, PFOR_CTX_SIZE);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, layout.ctx_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);

        emit_.push(x64::Reg::RDX);

        // [rdx+CTX_FN_PTR] = body function ptr
        std::size_t body_fixup = emit_lea_rip_disp32(x64::Reg::RAX);
        call_fixups_.push_back({body_fixup, layout.body_fn});
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, CTX_FN_PTR, x64::Reg::RAX);

        // [rdx+CTX_ARG0] = chunk_start = start + i * chunk_size
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.chunk_off);
        emit_.imul(x64::Reg::RAX, x64::Reg::RCX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.start_off);
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, CTX_ARG0, x64::Reg::RAX);

        // [rdx+CTX_ARG1] = chunk_end
        // last thread absorbs remainder, others get chunk_start + chunk_size
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t not_last = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.end_off);
        std::size_t chunk_end_done = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(not_last);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RDX, CTX_ARG0);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.chunk_off);
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.patch_jump(chunk_end_done);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, CTX_ARG1, x64::Reg::RAX);

        // [rdx+CTX_ARG2] = parent rbp
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, CTX_ARG2, x64::Reg::RBP);

        // [rdx+CTX_RESULT] = result (zero)
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, CTX_RESULT, x64::Reg::RAX);

        // CreateThread(NULL, 0, stub, context, 0, NULL)
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);

        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX); // lpThreadAttributes = NULL
        emit_.mov(x64::Reg::R9, x64::Reg::RDX);    // lpParameter = context
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);  // dwStackSize = 0
        // lea r8, [rip+stub_offset]
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05);
        std::size_t stub_fix2 = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        std::int32_t srel = static_cast<std::int32_t>(layout.stub_offset)
            - static_cast<std::int32_t>(stub_fix2 + 4);
        emit_.buffer().patch32(stub_fix2, static_cast<std::uint32_t>(srel));

        emit_.sub_imm(x64::Reg::RSP, 48);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateThread);
        emit_.add_imm(x64::Reg::RSP, 48);

        // store handle: hdl_arr[i] = rax
        emit_.pop(x64::Reg::RDX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.idx_off);
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RCX, 8);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, layout.hdl_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RDX, 0, x64::Reg::RAX);

        // i++
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, layout.idx_off, x64::Reg::RAX);

        std::int32_t spawn_loop_rel = static_cast<std::int32_t>(
            spawn_loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(spawn_loop_rel);
        emit_.patch_jump(spawn_loop_exit);
    }

    // emits WaitForMultipleObjects + CloseHandle cleanup loop
    void emit_pfor_wait_and_close(const PforLayout& layout) {
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, layout.hdl_arr_base);
        emit_.mov_imm32(x64::Reg::R8, 1);  // bWaitAll = TRUE
        emit_.mov_imm32(x64::Reg::R9, -1); // INFINITE
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::WaitForMultipleObjects);
        emit_.add_imm(x64::Reg::RSP, 32);

        // CloseHandle loop
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, layout.idx_off, x64::Reg::RAX);

        std::size_t close_loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.ncores_off);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t close_loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RAX, 8);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, layout.hdl_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RDX, 0);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.add_imm(x64::Reg::RSP, 32);

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, layout.idx_off, x64::Reg::RAX);

        std::int32_t close_rel = static_cast<std::int32_t>(
            close_loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(close_rel);
        emit_.patch_jump(close_loop_exit);
    }

    // parallel for codegen ? extracts body into internal function, spawns N threads, waits for all
    [[nodiscard]] bool generate_parallel_for(const ast::ParallelForStmt& pfor) {
        push_scope();

        // evaluate start and end into temp vars
        if (!generate_expr(*pfor.start)) { pop_scope(); return false; }
        auto& start_sym = current_scope_->define("__pfor_start",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, start_sym.stack_offset, x64::Reg::RAX);

        if (!generate_expr(*pfor.end)) { pop_scope(); return false; }
        auto& end_sym = current_scope_->define("__pfor_end",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, end_sym.stack_offset, x64::Reg::RAX);

        // skip everything if range is empty
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, start_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, end_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t skip_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // collect captured variables from parent scopes
        // threads cant access the callers stack directly, so we snapshot vars into the body frame
        std::vector<CapturedVar> captures;
        for (Scope* s = current_scope_; s != nullptr; s = s->parent) {
            for (const auto& [name, sym] : s->symbols) {
                if (name.starts_with("__pfor_")) continue;
                if (name == pfor.name) continue;
                captures.push_back({name, sym.stack_offset});
            }
        }

        PforLayout layout{};
        layout.body_fn = "__pfor_body_" + std::to_string(emit_.buffer().pos());
        layout.start_off = start_sym.stack_offset;
        layout.end_off = end_sym.stack_offset;

        // jump over the body function code
        std::size_t jmp_over = emit_.jmp_rel32_placeholder();

        // phase 1: emit the worker thread body function + entry stub
        if (!emit_pfor_body_function(pfor, captures, layout, jmp_over)) {
            pop_scope();
            return false;
        }

        // phase 2 setup: define all the scope vars the spawn loop needs
        auto& ncores_sym = current_scope_->define("__pfor_ncores",
            Type::make_primitive(PrimitiveType::I64), true);
        layout.ncores_off = ncores_sym.stack_offset;

        auto& range_sym = current_scope_->define("__pfor_range",
            Type::make_primitive(PrimitiveType::I64), true);
        layout.range_off = range_sym.stack_offset;

        auto& chunk_sym = current_scope_->define("__pfor_chunk",
            Type::make_primitive(PrimitiveType::I64), true);
        layout.chunk_off = chunk_sym.stack_offset;

        auto& rem_sym = current_scope_->define("__pfor_rem",
            Type::make_primitive(PrimitiveType::I64), true);
        layout.rem_off = rem_sym.stack_offset;

        // reserve stack space for context and handle arrays
        current_scope_->define("__pfor_ctxarr",
            Type::make_primitive(PrimitiveType::I64), true);
        for (int i = 0; i < PFOR_CTX_EXTRA_SLOTS; ++i) current_scope_->next_offset -= 8;
        layout.ctx_arr_base = current_scope_->next_offset + 8;

        current_scope_->define("__pfor_hdlarr",
            Type::make_primitive(PrimitiveType::I64), true);
        for (int i = 0; i < PFOR_HDL_EXTRA_SLOTS; ++i) current_scope_->next_offset -= 8;
        layout.hdl_arr_base = current_scope_->next_offset + 8;

        auto& idx_sym = current_scope_->define("__pfor_i",
            Type::make_primitive(PrimitiveType::I64), true);
        layout.idx_off = idx_sym.stack_offset;

        // phase 2: spawn worker threads
        emit_pfor_spawn_loop(layout);

        // phase 3: wait for all threads and close handles
        emit_pfor_wait_and_close(layout);

        emit_.patch_jump(skip_patch);

        pop_scope();
        return true;
    }
    // atomic operations codegen
    [[nodiscard]] bool generate_atomic_op(const ast::AtomicOpExpr& atomic) {
        // evaluate pointer arg -> save to stack
        if (!generate_expr(*atomic.ptr)) return false;
        emit_.push(x64::Reg::RAX); // save ptr

        switch (atomic.op) {
        case ast::AtomicOpExpr::Op::Add: {
            // atomic_add(ptr, val) -> old value
            if (atomic.args.empty()) { error("atomic_add needs a value arg"); return false; }
            if (!generate_expr(*atomic.args[0])) return false;
            emit_.pop(x64::Reg::RCX); // ptr
            // lock xadd [rcx], rax
            emit_.lock_xadd(x64::Reg::RCX, 0, x64::Reg::RAX);
            break;
        }
        case ast::AtomicOpExpr::Op::CAS: {
            // atomic_cas(ptr, expected, desired) -> old value (rax)
            if (atomic.args.size() < 2) { error("atomic_cas needs expected and desired"); return false; }
            // eval expected
            if (!generate_expr(*atomic.args[0])) return false;
            emit_.push(x64::Reg::RAX);
            // eval desired
            if (!generate_expr(*atomic.args[1])) return false;
            emit_.mov(x64::Reg::RDX, x64::Reg::RAX); // desired in rdx
            emit_.pop(x64::Reg::RAX); // expected in rax
            emit_.pop(x64::Reg::RCX); // ptr
            // lock cmpxchg [rcx], rdx
            emit_.lock_cmpxchg(x64::Reg::RCX, 0, x64::Reg::RDX);
            // rax = old value (either expected if swap succeeded, or actual if failed)
            break;
        }
        case ast::AtomicOpExpr::Op::Load: {
            // atomic_load(ptr) -> value
            emit_.pop(x64::Reg::RCX);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RCX, 0);
            break;
        }
        case ast::AtomicOpExpr::Op::Store: {
            // atomic_store(ptr, val) -> 0
            if (atomic.args.empty()) { error("atomic_store needs a value arg"); return false; }
            if (!generate_expr(*atomic.args[0])) return false;
            emit_.pop(x64::Reg::RCX); // ptr
            // xchg [rcx], rax (implicitly locked)
            emit_.xchg_mem(x64::Reg::RCX, 0, x64::Reg::RAX);
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
            break;
        }
        }
        return true;
    }
};

} // namespace opus

