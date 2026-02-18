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

    // Set runtime function pointers for builtins
    void set_runtime_pointers(
        void* print_int, void* print_str, void* read_file,
        void* string_length, void* string_get_char, void* print_string,
        void* make_string, void* write_file, void* malloc_fn,
        void* free_fn, void* array_new, void* array_get,
        void* array_set, void* array_len, void* array_free,
        void* string_append, void* int_to_string, void* print_char,
        // Self-hosting helpers
        void* string_equals = nullptr, void* string_substring = nullptr,
        void* is_alpha = nullptr, void* is_digit = nullptr, void* is_alnum = nullptr,
        void* is_whitespace = nullptr, void* string_starts_with = nullptr,
        void* exit_fn = nullptr, void* write_bytes = nullptr,
        void* buffer_new = nullptr, void* buffer_push = nullptr,
        void* buffer_len = nullptr, void* parse_int = nullptr) {
        runtime_print_int_ = print_int;
        runtime_print_str_ = print_str;
        runtime_read_file_ = read_file;
        runtime_string_length_ = string_length;
        runtime_string_get_char_ = string_get_char;
        runtime_print_string_ = print_string;
        runtime_make_string_ = make_string;
        runtime_write_file_ = write_file;
        runtime_malloc_ = malloc_fn;
        runtime_free_ = free_fn;
        runtime_array_new_ = array_new;
        runtime_array_get_ = array_get;
        runtime_array_set_ = array_set;
        runtime_array_len_ = array_len;
        runtime_array_free_ = array_free;
        runtime_string_append_ = string_append;
        runtime_int_to_string_ = int_to_string;
        runtime_print_char_ = print_char;
        // Self-hosting helpers
        runtime_string_equals_ = string_equals;
        runtime_string_substring_ = string_substring;
        runtime_is_alpha_ = is_alpha;
        runtime_is_digit_ = is_digit;
        runtime_is_alnum_ = is_alnum;
        runtime_is_whitespace_ = is_whitespace;
        runtime_string_starts_with_ = string_starts_with;
        runtime_exit_ = exit_fn;
        runtime_write_bytes_ = write_bytes;
        runtime_buffer_new_ = buffer_new;
        runtime_buffer_push_ = buffer_push;
        runtime_buffer_len_ = buffer_len;
        runtime_parse_int_ = parse_int;
    }

    // Generate code for a module
    bool generate(const ast::Module& mod) {
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
    
    // Return fixups as (patch_site, iat_offset) pairs
    std::vector<std::pair<std::size_t, std::size_t>> get_iat_fixups() const {
        std::vector<std::pair<std::size_t, std::size_t>> result;
        for (const auto& f : iat_fixups_) {
            result.emplace_back(f.patch_site, f.iat_offset);
        }
        return result;
    }
    
    // Return line map as (instruction_offset, source_line) pairs
    std::vector<std::pair<std::uint32_t, std::uint32_t>> get_line_map() const {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> result;
        for (const auto& e : line_map_) {
            result.emplace_back(e.offset, e.line);
        }
        return result;
    }

private:
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
        std::size_t patch_site = emit_.buffer().size();
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
        std::size_t current = emit_.buffer().size();
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
    
    // Runtime function pointers (for builtins like print_int)
    void* runtime_print_int_ = nullptr;
    void* runtime_print_str_ = nullptr;
    void* runtime_read_file_ = nullptr;
    void* runtime_string_length_ = nullptr;
    void* runtime_string_get_char_ = nullptr;
    void* runtime_print_string_ = nullptr;
    void* runtime_make_string_ = nullptr;
    void* runtime_write_file_ = nullptr;
    void* runtime_malloc_ = nullptr;
    void* runtime_free_ = nullptr;
    void* runtime_array_new_ = nullptr;
    void* runtime_array_get_ = nullptr;
    void* runtime_array_set_ = nullptr;
    void* runtime_array_len_ = nullptr;
    void* runtime_array_free_ = nullptr;
    void* runtime_string_append_ = nullptr;
    void* runtime_int_to_string_ = nullptr;
    void* runtime_print_char_ = nullptr;
    // Self-hosting helpers
    void* runtime_string_equals_ = nullptr;
    void* runtime_string_substring_ = nullptr;
    void* runtime_is_alpha_ = nullptr;
    void* runtime_is_digit_ = nullptr;
    void* runtime_is_alnum_ = nullptr;
    void* runtime_is_whitespace_ = nullptr;
    void* runtime_string_starts_with_ = nullptr;
    void* runtime_exit_ = nullptr;
    void* runtime_write_bytes_ = nullptr;
    void* runtime_buffer_new_ = nullptr;
    void* runtime_buffer_push_ = nullptr;
    void* runtime_buffer_len_ = nullptr;
    void* runtime_parse_int_ = nullptr;

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
    
    // rip-relative slot in code buffer that holds the global data base pointer
    // __opus_init writes HeapAlloc result here, all global accesses load from here
    std::size_t global_base_slot_ = 0;
    bool has_global_slot_ = false;
    
    // spawn/await context tracking (LIFO stack of context ptr offsets)
    std::vector<std::int32_t> spawn_context_stack_;

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
    // declarations
    // ========================================================================

    bool generate_decl(const ast::Decl& decl) {
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
    bool generate_import(const ast::ImportDecl& imp) {
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
            emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx
            emit_.mov_imm32(x64::Reg::R8, static_cast<std::int32_t>(next_global_offset_));
            
            emit_iat_call_raw(pe::iat::HeapAlloc);
            
            emit_.add_imm(x64::Reg::RSP, 32);
            
            // store base pointer to rip-relative slot so other functions can find it
            emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
            emit_.buffer().emit8(0x0D); // lea rcx, [rip+disp32]
            std::size_t lea_site = emit_.buffer().size();
            emit_.buffer().emit32(0);
            std::int32_t slot_rel = static_cast<std::int32_t>(global_base_slot_)
                - static_cast<std::int32_t>(lea_site + 4);
            emit_.buffer().patch32(lea_site, static_cast<std::uint32_t>(slot_rel));
            emit_.mov_store(x64::Reg::RCX, 0, x64::Reg::RAX);
            
            // run initializers
            for (auto& [name, gv] : globals_) {
                if (gv.init) {
                    // evaluate initializer -> RAX
                    generate_expr(*gv.init);
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX); // save value
                    
                    // reload base from slot
                    emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                    emit_.buffer().emit8(0x05); // lea rax, [rip+disp32]
                    std::size_t init_lea = emit_.buffer().size();
                    emit_.buffer().emit32(0);
                    std::int32_t init_rel = static_cast<std::int32_t>(global_base_slot_)
                        - static_cast<std::int32_t>(init_lea + 4);
                    emit_.buffer().patch32(init_lea, static_cast<std::uint32_t>(init_rel));
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

    bool generate_struct_decl(const ast::StructDecl& s) {
        StructInfo info;
        info.name = s.name;
        for (const auto& [name, type] : s.fields) {
            info.fields.emplace_back(name, type.clone());
        }
        info.calculate_offsets();
        structs_[s.name] = std::move(info);
        return true;
    }
    
    bool generate_class_decl(const ast::ClassDecl& c) {
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
        
        return true;
    }

    bool generate_enum_decl(const ast::EnumDecl& e) {
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

    bool generate_fn(const ast::FnDecl& fn) {
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
            std::size_t init_fixup = emit_.buffer().size();
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

    bool generate_stmt(const ast::Stmt& stmt) {
        // Record line mapping for debug
        if (stmt.span.start.line > 0) {
            line_map_.push_back({
                static_cast<std::uint32_t>(emit_.buffer().pos()),
                stmt.span.start.line
            });
        }
        
        if (stmt.is<ast::LetStmt>()) {
            return generate_let(stmt.as<ast::LetStmt>());
        }
        if (stmt.is<ast::ReturnStmt>()) {
            return generate_return(stmt.as<ast::ReturnStmt>());
        }
        if (stmt.is<ast::ExprStmt>()) {
            return generate_expr_stmt(stmt.as<ast::ExprStmt>());
        }
        if (stmt.is<ast::IfStmt>()) {
            return generate_if(stmt.as<ast::IfStmt>());
        }
        if (stmt.is<ast::WhileStmt>()) {
            return generate_while(stmt.as<ast::WhileStmt>());
        }
        if (stmt.is<ast::LoopStmt>()) {
            return generate_loop(stmt.as<ast::LoopStmt>());
        }
        if (stmt.is<ast::ForStmt>()) {
            return generate_for(stmt.as<ast::ForStmt>());
        }
        if (stmt.is<ast::ParallelForStmt>()) {
            return generate_parallel_for(stmt.as<ast::ParallelForStmt>());
        }
        if (stmt.is<ast::BreakStmt>()) {
            return generate_break();
        }
        if (stmt.is<ast::ContinueStmt>()) {
            return generate_continue();
        }
        if (stmt.is<ast::BlockStmt>()) {
            const auto& block = stmt.as<ast::BlockStmt>();
            push_scope();
            for (const auto& s : block.stmts) {
                if (!generate_stmt(*s)) {
                    pop_scope();
                    return false;
                }
            }
            pop_scope();
            return true;
        }
        
        error("unknown statement type");
        return false;
    }

    bool generate_let(const ast::LetStmt& let) {
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

    bool generate_return(const ast::ReturnStmt& ret) {
        if (ret.value) {
            if (!generate_expr(*ret.value.value())) return false;
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        emit_.epilogue();
        return true;
    }

    bool generate_expr_stmt(const ast::ExprStmt& stmt) {
        return generate_expr(*stmt.expr);
    }

    bool generate_if(const ast::IfStmt& if_stmt) {
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

        if (if_stmt.else_block) {
            std::size_t jmp_patch = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(jz_patch);

            push_scope();
            for (const auto& stmt : *if_stmt.else_block) {
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

    bool generate_while(const ast::WhileStmt& while_stmt) {
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

    bool generate_loop(const ast::LoopStmt& loop_stmt) {
        break_patches_.push_back({});
        continue_patches_.push_back({});
        std::size_t loop_start = emit_.buffer().pos();

        for (const auto& stmt : loop_stmt.body) {
            if (!generate_stmt(*stmt)) return false;
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

        return true;
    }

    bool generate_for(const ast::ForStmt& for_stmt) {
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
        auto& sym = current_scope_->define(for_stmt.var_name, 
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

    bool generate_break() {
        if (break_patches_.empty()) {
            error("break outside of loop");
            return false;
        }
        std::size_t patch = emit_.jmp_rel32_placeholder();
        break_patches_.back().push_back(patch);
        return true;
    }

    bool generate_continue() {
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

    bool generate_expr(const ast::Expr& expr) {
        if (expr.is<ast::LiteralExpr>()) {
            return generate_literal(expr.as<ast::LiteralExpr>());
        }
        if (expr.is<ast::IdentExpr>()) {
            return generate_ident(expr.as<ast::IdentExpr>());
        }
        if (expr.is<ast::BinaryExpr>()) {
            return generate_binary(expr.as<ast::BinaryExpr>());
        }
        if (expr.is<ast::UnaryExpr>()) {
            return generate_unary(expr.as<ast::UnaryExpr>());
        }
        if (expr.is<ast::CallExpr>()) {
            return generate_call(expr.as<ast::CallExpr>());
        }
        if (expr.is<ast::IndexExpr>()) {
            return generate_index(expr.as<ast::IndexExpr>());
        }
        if (expr.is<ast::FieldExpr>()) {
            return generate_field(expr.as<ast::FieldExpr>());
        }
        if (expr.is<ast::CastExpr>()) {
            // For now, casts just evaluate the expression (no type checking)
            return generate_expr(*expr.as<ast::CastExpr>().expr);
        }
        if (expr.is<ast::StructExpr>()) {
            return generate_struct_literal(expr.as<ast::StructExpr>());
        }
        if (expr.is<ast::SpawnExpr>()) {
            return generate_spawn(expr.as<ast::SpawnExpr>());
        }
        if (expr.is<ast::AwaitExpr>()) {
            return generate_await(expr.as<ast::AwaitExpr>());
        }
        if (expr.is<ast::AtomicOpExpr>()) {
            return generate_atomic_op(expr.as<ast::AtomicOpExpr>());
        }
        
        error("unknown expression type");
        return false;
    }

    // resolve the struct type name that a field expression evaluates to
    // e.g. for `outer.inner` where outer is Outer and inner is of type Inner, returns "Inner"
    std::optional<std::string> resolve_field_type(const ast::FieldExpr& field) {
        std::string base_struct;
        
        if (field.base->is<ast::IdentExpr>()) {
            const auto& name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(name);
            if (!sym) return std::nullopt;
            if (std::holds_alternative<StructType>(sym->type.kind))
                base_struct = std::get<StructType>(sym->type.kind).name;
            else if (std::holds_alternative<std::string>(sym->type.kind))
                base_struct = std::get<std::string>(sym->type.kind);
            else return std::nullopt;
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
                if (std::holds_alternative<StructType>(ftype.kind))
                    return std::get<StructType>(ftype.kind).name;
                if (std::holds_alternative<std::string>(ftype.kind))
                    return std::get<std::string>(ftype.kind);
                return std::nullopt; // primitive field, no struct type
            }
        }
        return std::nullopt;
    }

    bool generate_field(const ast::FieldExpr& field) {
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
            if (std::holds_alternative<StructType>(sym->type.kind))
                struct_name = std::get<StructType>(sym->type.kind).name;
            else if (std::holds_alternative<std::string>(sym->type.kind))
                struct_name = std::get<std::string>(sym->type.kind);
            else {
                error(std::format("variable '{}' is not a struct type", var_name));
                return false;
            }
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

    bool generate_struct_literal(const ast::StructExpr& lit) {
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
            emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx (flags = 0)
            emit_.mov_imm32(x64::Reg::R8, static_cast<std::int32_t>(size));  // size -> R8
            
            emit_iat_call_raw(pe::iat::HeapAlloc);
            
            emit_.add_imm(x64::Reg::RSP, 32);
        } else if (runtime_malloc_) {
            emit_.mov_imm32(x64::Reg::RCX, static_cast<std::int32_t>(size));
            emit_.sub_imm(x64::Reg::RSP, 32);
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(runtime_malloc_));
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
                return false;
            }
            
            std::size_t offset = *offset_opt;
            
            // Generate value expression -> RAX
            if (!generate_expr(*value_expr)) return false;
            
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

    bool generate_index(const ast::IndexExpr& idx) {
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

    bool generate_literal(const ast::LiteralExpr& lit) {
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
            // Store double as raw bits
            std::uint64_t bits;
            std::memcpy(&bits, d, sizeof(double));
            emit_.mov_imm64(x64::Reg::RAX, bits);
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
                std::size_t string_start = emit_.buffer().size();
                
                // Emit string bytes
                for (char c : str) {
                    emit_.buffer().emit8(static_cast<std::uint8_t>(c));
                }
                if (!is_hex) {
                    emit_.buffer().emit8(0);  // Null terminator for regular strings only
                }
                
                // Now emit: lea rax, [rip - string_len_with_lea]
                std::size_t current_pos = emit_.buffer().size();
                std::int32_t offset = static_cast<std::int32_t>(string_start) - static_cast<std::int32_t>(current_pos + 7);
                
                emit_.buffer().emit8(0x48);  // REX.W
                emit_.buffer().emit8(0x8D);  // LEA
                emit_.buffer().emit8(0x05);  // RAX, [RIP+disp32]
                emit_.buffer().emit8(static_cast<std::uint8_t>(offset & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 8) & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 16) & 0xFF));
                emit_.buffer().emit8(static_cast<std::uint8_t>((offset >> 24) & 0xFF));
            }
            else if (runtime_make_string_) {
                // Normal mode: use runtime string table
                using MakeStringFn = std::int64_t(*)(const char*);
                auto make_string = reinterpret_cast<MakeStringFn>(runtime_make_string_);
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

    bool generate_ident(const ast::IdentExpr& ident) {
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
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                emit_.buffer().emit8(0x05); // lea rax, [rip+disp32]
                std::size_t lea_site = emit_.buffer().size();
                emit_.buffer().emit32(0);
                std::int32_t rel = static_cast<std::int32_t>(global_base_slot_)
                    - static_cast<std::int32_t>(lea_site + 4);
                emit_.buffer().patch32(lea_site, static_cast<std::uint32_t>(rel));
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

    bool generate_binary(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;

        // Handle assignment and compound assignment specially
        if (bin.op == Op::Assign) {
            return generate_assignment(bin);
        }
        if (bin.op == Op::AddAssign || bin.op == Op::SubAssign || 
            bin.op == Op::MulAssign || bin.op == Op::DivAssign) {
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

    bool generate_assignment(const ast::BinaryExpr& bin) {
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
                if (std::holds_alternative<StructType>(sym->type.kind))
                    struct_name = std::get<StructType>(sym->type.kind).name;
                else if (std::holds_alternative<std::string>(sym->type.kind))
                    struct_name = std::get<std::string>(sym->type.kind);
                else {
                    error(std::format("variable '{}' is not a struct type", var_name));
                    return false;
                }
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
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                emit_.buffer().emit8(0x05); // lea rax, [rip+disp32]
                std::size_t lea_site = emit_.buffer().size();
                emit_.buffer().emit32(0);
                std::int32_t rel = static_cast<std::int32_t>(global_base_slot_)
                    - static_cast<std::int32_t>(lea_site + 4);
                emit_.buffer().patch32(lea_site, static_cast<std::uint32_t>(rel));
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

    bool generate_compound_assign(const ast::BinaryExpr& bin) {
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
            if (std::holds_alternative<StructType>(sym->type.kind)) {
                struct_name = std::get<StructType>(sym->type.kind).name;
            } else if (std::holds_alternative<std::string>(sym->type.kind)) {
                struct_name = std::get<std::string>(sym->type.kind);
            } else {
                error(std::format("variable '{}' is not a struct type", var_name));
                return false;
            }
            
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
            default:
                error("unsupported compound assignment operator");
                return false;
        }
        
        // Store result
        emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
        return true;
    }

    bool generate_unary(const ast::UnaryExpr& un) {
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

    bool generate_method_call(const ast::CallExpr& call) {
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
        if (std::holds_alternative<StructType>(sym->type.kind)) {
            class_name = std::get<StructType>(sym->type.kind).name;
        } else if (std::holds_alternative<std::string>(sym->type.kind)) {
            class_name = std::get<std::string>(sym->type.kind);
        } else {
            error(std::format("variable '{}' is not a class type", var_name));
            return false;
        }
        
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
        std::size_t fixup_site = emit_.buffer().size();
        emit_.buffer().emit32(0);
        call_fixups_.push_back({fixup_site, mangled_name});
        
        // clean up frame + pushed args
        emit_.add_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));
        
        return true;
    }

    bool generate_call(const ast::CallExpr& call) {
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
        
        // Normalize namespace syntax: Mem.read -> mem_read
        // This allows PascalCase namespaces like Mem.read, Console.println
        std::string fn_name;
        fn_name.reserve(raw_fn_name.size());
        for (char c : raw_fn_name) {
            if (c == '.') {
                fn_name += '_';
            } else if (c >= 'A' && c <= 'Z') {
                fn_name += static_cast<char>(c + 32);  // lowercase
            } else {
                fn_name += c;
            }
        }
        
        // DLL-mode builtins - these emit relative calls to embedded runtime
        if (dll_mode_) {
            if (fn_name == "dll_print" || fn_name == "print") {
                return generate_dll_builtin_print(call);
            }
            if (fn_name == "dll_set_title" || fn_name == "set_title") {
                return generate_dll_builtin_set_title(call);
            }
            if (fn_name == "alloc_console") {
                return generate_dll_builtin_alloc_console(call);
            }
            if (fn_name == "print_int" || fn_name == "print_dec") {
                return generate_dll_builtin_print_dec(call);
            }
            if (fn_name == "print_hex") {
                return generate_dll_builtin_print_int(call);
            }
            // Memory operations - inline, no runtime needed
            if (fn_name == "mem_read" || fn_name == "mem_read_i64") {
                return generate_dll_mem_read(call, 8);
            }
            if (fn_name == "mem_read_i32") {
                return generate_dll_mem_read(call, 4);
            }
            if (fn_name == "mem_read_i16") {
                return generate_dll_mem_read(call, 2);
            }
            if (fn_name == "mem_read_i8") {
                return generate_dll_mem_read(call, 1);
            }
            if (fn_name == "mem_write" || fn_name == "mem_write_i64") {
                return generate_dll_mem_write(call, 8);
            }
            if (fn_name == "mem_write_i32") {
                return generate_dll_mem_write(call, 4);
            }
            if (fn_name == "mem_write_i16") {
                return generate_dll_mem_write(call, 2);
            }
            if (fn_name == "mem_write_i8") {
                return generate_dll_mem_write(call, 1);
            }
            // Debug: crash() triggers access violation to test crash handler
            if (fn_name == "crash") {
                return generate_dll_crash(call);
            }
            // Install crash handler (opt-in)
            if (fn_name == "install_crash_handler") {
                return generate_dll_install_crash_handler(call);
            }
            // test builtins for crash handler verification
            if (fn_name == "trigger_illegal") {
                emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x0B);  // ud2
                return true;
            }
            if (fn_name == "trigger_stack_overflow") {
                // RaiseException(0xC00000FD, 0, 0, NULL) - directly fires stack overflow
                // injected DLL threads dont have guard pages so we cant trigger it naturally
                // cant use mov_imm32 here because it sign-extends with REX.W
                emit_.buffer().emit8(0xB9);                     // mov ecx, imm32 (zero-extends to rcx)
                emit_.buffer().emit32(0xC00000FD);
                emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);     // dwExceptionFlags = 0
                emit_.xor_(x64::Reg::R8, x64::Reg::R8);       // nNumberOfArguments = 0
                emit_.xor_(x64::Reg::R9, x64::Reg::R9);       // lpArguments = NULL
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_iat_call_raw(pe::iat::RaiseException);
                emit_.add_imm(x64::Reg::RSP, 32);
                return true;
            }
            // breakpoint - emit INT3, VEH catches it and opens REPL
            if (fn_name == "breakpoint") {
                emit_.buffer().emit8(0xCC);
                return true;
            }
            // conditional breakpoint - only fires when arg is nonzero
            if (fn_name == "breakpoint_if") {
                if (call.args.empty()) {
                    error("breakpoint_if requires 1 argument");
                    return false;
                }
                if (!generate_expr(*call.args[0])) return false;
                emit_.test(x64::Reg::RAX, x64::Reg::RAX);
                // jz +1 (skip the int3)
                emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x01);
                emit_.buffer().emit8(0xCC);
                return true;
            }
            // Memory block operations
            if (fn_name == "memcpy") {
                return generate_dll_memcpy(call);
            }
            if (fn_name == "memset") {
                return generate_dll_memset(call);
            }
            if (fn_name == "memcmp") {
                return generate_dll_memcmp(call);
            }
            // Timing
            if (fn_name == "sleep" || fn_name == "Sleep") {
                return generate_dll_sleep(call);
            }
            if (fn_name == "get_tick_count" || fn_name == "GetTickCount") {
                return generate_dll_get_tick_count(call);
            }
            // Math - basic float operations
            if (fn_name == "sqrt") {
                return generate_dll_sqrt(call);
            }
            if (fn_name == "sin") {
                return generate_dll_sin(call);
            }
            if (fn_name == "cos") {
                return generate_dll_cos(call);
            }
            if (fn_name == "tan") {
                return generate_dll_tan(call);
            }
            if (fn_name == "atan2") {
                return generate_dll_atan2(call);
            }
            if (fn_name == "floor") {
                return generate_dll_floor(call);
            }
            if (fn_name == "ceil") {
                return generate_dll_ceil(call);
            }
            if (fn_name == "abs" || fn_name == "fabs") {
                return generate_dll_abs(call);
            }
            if (fn_name == "pow") {
                return generate_dll_pow(call);
            }
            // Memory allocation
            if (fn_name == "malloc") {
                return generate_dll_malloc(call);
            }
            if (fn_name == "free") {
                return generate_dll_free(call);
            }
            // array builtins
            if (fn_name == "array_new" || fn_name == "new_array") {
                return generate_dll_array_new(call);
            }
            if (fn_name == "array_get") {
                return generate_dll_array_get(call);
            }
            if (fn_name == "array_set") {
                return generate_dll_array_set(call);
            }
            if (fn_name == "array_len") {
                return generate_dll_array_len(call);
            }
            if (fn_name == "array_free") {
                return generate_dll_array_free(call);
            }
            // String builtins
            if (fn_name == "string_length" || fn_name == "strlen") {
                return generate_dll_string_length(call);
            }
            if (fn_name == "string_append" || fn_name == "concat") {
                return generate_dll_string_append(call);
            }
            if (fn_name == "int_to_string" || fn_name == "itoa") {
                return generate_dll_int_to_string(call);
            }
            if (fn_name == "string_equals" || fn_name == "streq") {
                return generate_dll_string_equals(call);
            }
            if (fn_name == "string_substring" || fn_name == "substr") {
                return generate_dll_string_substring(call);
            }
            if (fn_name == "print_char" || fn_name == "putc") {
                return generate_dll_print_char(call);
            }
            // Memory protection
            if (fn_name == "virtual_protect" || fn_name == "VirtualProtect") {
                return generate_dll_virtual_protect(call);
            }
            if (fn_name == "virtual_alloc" || fn_name == "VirtualAlloc") {
                return generate_dll_virtual_alloc(call);
            }
            if (fn_name == "virtual_free" || fn_name == "VirtualFree") {
                return generate_dll_virtual_free(call);
            }
            // Pattern scanner
            if (fn_name == "scan") {
                return generate_dll_scan(call);
            }
            // Module functions
            if (fn_name == "get_module" || fn_name == "GetModuleHandle") {
                return generate_dll_get_module(call);
            }
        }
        
        // Check for builtins first
        if (fn_name == "print_int") {
            return generate_builtin_print_int(call);
        }
        if (fn_name == "print") {
            // DLL mode uses embedded print, JIT uses runtime
            if (dll_mode_) {
                return generate_dll_builtin_print(call);
            }
            return generate_builtin_string_op(call, runtime_print_str_);
        }
        if (fn_name == "println") {
            // Print string then newline
            if (dll_mode_) {
                if (!generate_dll_builtin_print(call)) return false;
                // Add newline
                emit_.sub_imm(x64::Reg::RSP, 32);
                // Put "\n" on stack
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x04);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x0A);  // mov byte [rsp], '\n'
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x01);
                emit_.buffer().emit8(0x00);  // mov byte [rsp+1], 0
                emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
                
                emit_startup_call(print_offset_);
                
                emit_.add_imm(x64::Reg::RSP, 32);
            } else {
                if (!generate_builtin_string_op(call, runtime_print_str_)) return false;
            }
            return true;
        }
        if (fn_name == "read_file") {
            return generate_builtin_string_op(call, runtime_read_file_);
        }
        if (fn_name == "string_length" || fn_name == "strlen") {
            return generate_builtin_string_op(call, runtime_string_length_);
        }
        if (fn_name == "string_get_char" || fn_name == "char_at") {
            return generate_builtin_string_get_char(call);
        }
        if (fn_name == "print_string" || fn_name == "puts") {
            return generate_builtin_string_op(call, runtime_print_string_);
        }
        if (fn_name == "write_file") {
            return generate_builtin_two_arg(call, runtime_write_file_);
        }
        if (fn_name == "malloc") {
            return generate_builtin_string_op(call, runtime_malloc_);
        }
        if (fn_name == "free") {
            return generate_builtin_string_op(call, runtime_free_);
        }
        if (fn_name == "array_new" || fn_name == "new_array") {
            return generate_builtin_string_op(call, runtime_array_new_);
        }
        if (fn_name == "array_get") {
            return generate_builtin_two_arg(call, runtime_array_get_);
        }
        if (fn_name == "array_set") {
            return generate_builtin_three_arg(call, runtime_array_set_);
        }
        if (fn_name == "array_len") {
            return generate_builtin_string_op(call, runtime_array_len_);
        }
        if (fn_name == "array_free") {
            return generate_builtin_string_op(call, runtime_array_free_);
        }
        if (fn_name == "string_append" || fn_name == "concat") {
            return generate_builtin_two_arg(call, runtime_string_append_);
        }
        if (fn_name == "int_to_string" || fn_name == "itoa") {
            return generate_builtin_string_op(call, runtime_int_to_string_);
        }
        if (fn_name == "print_char" || fn_name == "putc") {
            return generate_builtin_string_op(call, runtime_print_char_);
        }
        // Self-hosting helpers
        if (fn_name == "string_equals" || fn_name == "streq") {
            return generate_builtin_two_arg(call, runtime_string_equals_);
        }
        if (fn_name == "string_substring" || fn_name == "substr") {
            return generate_builtin_three_arg(call, runtime_string_substring_);
        }
        if (fn_name == "is_alpha") {
            return generate_builtin_string_op(call, runtime_is_alpha_);
        }
        if (fn_name == "is_digit") {
            return generate_builtin_string_op(call, runtime_is_digit_);
        }
        if (fn_name == "is_alnum") {
            return generate_builtin_string_op(call, runtime_is_alnum_);
        }
        if (fn_name == "is_whitespace") {
            return generate_builtin_string_op(call, runtime_is_whitespace_);
        }
        if (fn_name == "string_starts_with" || fn_name == "starts_with") {
            return generate_builtin_two_arg(call, runtime_string_starts_with_);
        }
        if (fn_name == "exit") {
            return generate_builtin_string_op(call, runtime_exit_);
        }
        if (fn_name == "write_bytes") {
            return generate_builtin_three_arg(call, runtime_write_bytes_);
        }
        if (fn_name == "buffer_new") {
            return generate_builtin_string_op(call, runtime_buffer_new_);
        }
        if (fn_name == "buffer_push") {
            return generate_builtin_two_arg(call, runtime_buffer_push_);
        }
        if (fn_name == "buffer_len") {
            return generate_builtin_string_op(call, runtime_buffer_len_);
        }
        if (fn_name == "parse_int" || fn_name == "atoi") {
            return generate_builtin_string_op(call, runtime_parse_int_);
        }
        // Memory operations - Load function address dynamically from symbol
        // These use the extern "C" functions defined in opus.ixx
        if (fn_name == "mem_read" || fn_name == "mem_read_i64" || fn_name == "memory_read") {
            return generate_builtin_mem_read_i64(call);
        }
        if (fn_name == "mem_read_i32") {
            return generate_builtin_mem_read_i32(call);
        }
        if (fn_name == "mem_read_i16") {
            return generate_builtin_mem_read_i16(call);
        }
        if (fn_name == "mem_read_i8") {
            return generate_builtin_mem_read_i8(call);
        }
        if (fn_name == "mem_read_ptr" || fn_name == "read_ptr") {
            return generate_builtin_mem_read_i64(call);  // Same as i64
        }
        if (fn_name == "mem_write" || fn_name == "mem_write_i64" || fn_name == "memory_write") {
            return generate_builtin_mem_write_i64(call);
        }
        if (fn_name == "mem_write_i32") {
            return generate_builtin_mem_write_i32(call);
        }
        if (fn_name == "mem_write_i16") {
            return generate_builtin_mem_write_i16(call);
        }
        if (fn_name == "mem_write_i8") {
            return generate_builtin_mem_write_i8(call);
        }
        if (fn_name == "mem_write_ptr" || fn_name == "write_ptr") {
            return generate_builtin_mem_write_i64(call);  // Same as i64
        }
        // FFI - Windows API
        if (fn_name == "get_module" || fn_name == "GetModuleHandle") {
            return generate_builtin_ffi_one_arg(call, "get_module");
        }
        if (fn_name == "load_library" || fn_name == "LoadLibrary") {
            return generate_builtin_ffi_one_arg(call, "load_library");
        }
        if (fn_name == "get_proc" || fn_name == "GetProcAddress") {
            return generate_builtin_ffi_two_arg(call, "get_proc");
        }
        if (fn_name == "ffi_call" || fn_name == "ffi_call0") {
            return generate_builtin_ffi_one_arg(call, "ffi_call0");
        }
        if (fn_name == "ffi_call1") {
            return generate_builtin_ffi_two_arg(call, "ffi_call1");
        }
        if (fn_name == "ffi_call2") {
            return generate_builtin_ffi_three_arg(call, "ffi_call2");
        }
        if (fn_name == "ffi_call3") {
            return generate_builtin_ffi_four_arg(call, "ffi_call3");
        }
        if (fn_name == "ffi_call4") {
            return generate_builtin_ffi_five_arg(call, "ffi_call4");
        }
        if (fn_name == "msgbox" || fn_name == "MessageBox") {
            return generate_builtin_ffi_three_arg(call, "msgbox");
        }
        if (fn_name == "get_last_error" || fn_name == "GetLastError") {
            return generate_builtin_ffi_zero_arg("get_last_error");
        }
        if (fn_name == "virtual_protect" || fn_name == "VirtualProtect") {
            return generate_builtin_ffi_three_arg(call, "virtual_protect");
        }
        if (fn_name == "get_current_process" || fn_name == "GetCurrentProcess") {
            return generate_builtin_ffi_zero_arg("get_current_process");
        }
        if (fn_name == "get_current_process_id" || fn_name == "GetCurrentProcessId" || fn_name == "getpid") {
            return generate_builtin_ffi_zero_arg("get_current_process_id");
        }
        // Console functions
        if (fn_name == "alloc_console" || fn_name == "AllocConsole") {
            return generate_builtin_ffi_zero_arg("alloc_console");
        }
        if (fn_name == "free_console" || fn_name == "FreeConsole") {
            return generate_builtin_ffi_zero_arg("free_console");
        }
        if (fn_name == "set_console_title" || fn_name == "SetConsoleTitle") {
            return generate_builtin_ffi_one_arg(call, "set_console_title");
        }
        if (fn_name == "range") {
            // range() is handled specially in for loops, not as a regular call
            // If called directly, just return 0
            emit_.mov_imm32(x64::Reg::RAX, 0);
            return true;
        }
        
        // Look up function (to verify it exists)
        auto it = functions_.find(fn_name);
        if (it == functions_.end()) {
            error(std::format("undefined function: {}", fn_name));
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
        std::size_t fixup_site = emit_.buffer().size();
        emit_.buffer().emit32(0);  // Placeholder - will be patched
        
        // Record fixup for later patching
        call_fixups_.push_back(CallFixup{
            .call_site = fixup_site,
            .target_fn = fn_name
        });
        
        // Restore stack (alloc + pushed args)
        emit_.add_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));

        return true;
    }

    bool generate_builtin_print_int(const ast::CallExpr& call) {
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
        if (runtime_print_int_) {
            // Load function pointer into RAX
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(runtime_print_int_));
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
    
    bool generate_dll_builtin_print(const ast::CallExpr& call) {
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
    
    bool generate_dll_builtin_set_title(const ast::CallExpr& call) {
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
    
    bool generate_dll_builtin_alloc_console([[maybe_unused]] const ast::CallExpr& call) {
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(alloc_console_offset_);
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    bool generate_dll_builtin_print_int(const ast::CallExpr& call) {
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
        std::size_t loop_start = emit_.buffer().size();
        
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
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().size() - 1);
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
    
    bool generate_dll_builtin_print_dec(const ast::CallExpr& call) {
        // signed decimal print with negative handling
        if (call.args.empty()) {
            error("print_dec requires an argument");
            return false;
        }
        
        if (!generate_expr(*call.args[0])) return false;
        
        emit_.sub_imm(x64::Reg::RSP, 64);
        emit_.mov(x64::Reg::R8, x64::Reg::RAX);
        
        // r9 = is_negative flag
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xC9);  // xor r9, r9
        
        // check sign
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jns_pos = emit_.buffer().size();
        emit_.buffer().emit8(0x79); emit_.buffer().emit8(0x00);  // jns .positive
        
        // negative: negate r8, set r9=1
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xC7); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00);  // mov r9, 1
        
        // .positive:
        std::size_t positive_label = emit_.buffer().size();
        emit_.buffer().data()[jns_pos + 1] = static_cast<std::uint8_t>(positive_label - jns_pos - 2);
        
        // rdi = end of stack buffer, fill backwards
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x7C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x3A);  // lea rdi, [rsp+0x3A]
        
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x00);  // mov byte [rdi], 0
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x0A);  // mov byte [rdi], '\n'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        
        // zero check
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jnz_pos = emit_.buffer().size();
        emit_.buffer().emit8(0x75); emit_.buffer().emit8(0x00);  // jnz .nonzero
        
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x30);  // mov byte [rdi], '0'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        std::size_t jmp_to_print_pos = emit_.buffer().size();
        emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x00);  // jmp .print
        
        // .nonzero:
        std::size_t after_zero = emit_.buffer().size();
        emit_.buffer().data()[jnz_pos + 1] = static_cast<std::uint8_t>(after_zero - jnz_pos - 2);
        
        // div-by-10 loop on the now-positive value in r8
        emit_.buffer().emit8(0xB9); emit_.buffer().emit32(10);  // mov ecx, 10
        
        std::size_t loop_start = emit_.buffer().size();
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov rax, r8
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor rdx, rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xF1);  // div rcx
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov r8, rax (quotient)
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xC2); emit_.buffer().emit8(0x30);  // add dl, '0'
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x17);  // mov [rdi], dl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        emit_.buffer().emit8(0x75);  // jnz
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().size() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));
        
        // prepend '-' if negative
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC9);  // test r9, r9
        std::size_t jz_nosign = emit_.buffer().size();
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x00);  // jz .no_sign
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x2D);  // mov byte [rdi], '-'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        // .no_sign:
        std::size_t nosign_label = emit_.buffer().size();
        emit_.buffer().data()[jz_nosign + 1] = static_cast<std::uint8_t>(nosign_label - jz_nosign - 2);
        
        // .print:
        std::size_t print_label = emit_.buffer().size();
        emit_.buffer().data()[jmp_to_print_pos + 1] = static_cast<std::uint8_t>(print_label - jmp_to_print_pos - 2);
        
        // inc rdi (point to first char)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC7);  // inc rdi
        
        // call dll_print_impl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xF9);  // mov rcx, rdi
        
        emit_startup_call(print_offset_);
        
        emit_.add_imm(x64::Reg::RSP, 64);
        
        return true;
    }
    
    bool generate_dll_crash([[maybe_unused]] const ast::CallExpr& call) {
        // write to null pointer - triggers access violation for crash handler testing
        emit_.buffer().emit8(0x48);  // REX.W
        emit_.buffer().emit8(0xC7);  // MOV r/m64, imm32
        emit_.buffer().emit8(0x04);  // SIB follows
        emit_.buffer().emit8(0x25);  // [disp32]
        emit_.buffer().emit32(0);    // address = 0
        emit_.buffer().emit32(0);    // value = 0
        return true;
    }
    
    bool generate_dll_install_crash_handler([[maybe_unused]] const ast::CallExpr& call) {
        // register our VEH as first handler so it catches before anything else
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_.buffer().emit8(0xB9);
        emit_.buffer().emit32(1);  // ecx = 1 (first handler)
        
        // lea rdx to the crash handler in .text via rip-relative
        std::size_t current_offset = emit_.buffer().size();
        std::int32_t crash_handler_rel = static_cast<std::int32_t>(crash_handler_offset_)
            - static_cast<std::int32_t>(user_code_offset_ + current_offset + 7);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x15);
        emit_.buffer().emit32(static_cast<std::uint32_t>(crash_handler_rel));
        
        emit_iat_call_raw(pe::iat::AddVectoredExceptionHandler);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        return true;
    }
    
    bool generate_dll_mem_read(const ast::CallExpr& call, int size) {
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
    
    bool generate_dll_mem_write(const ast::CallExpr& call, int size) {
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

    bool generate_dll_memcpy(const ast::CallExpr& call) {
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
    
    bool generate_dll_memset(const ast::CallExpr& call) {
        // memset(ptr, value, len) - fill memory
        if (call.args.size() < 3) {
            error("memset requires ptr, value, and len arguments");
            return false;
        }
        
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
        
        return true;
    }
    
    bool generate_dll_memcmp(const ast::CallExpr& call) {
        // memcmp(a, b, len) -> int - compare memory, returns 0 if equal
        if (call.args.size() < 3) {
            error("memcmp requires a, b, and len arguments");
            return false;
        }
        
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
        
        return true;
    }
    
    bool generate_dll_sleep(const ast::CallExpr& call) {
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
    
    bool generate_dll_get_tick_count([[maybe_unused]] const ast::CallExpr& call) {
        // get_tick_count() -> int - call Windows GetTickCount64
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::GetTickCount64);
        
        emit_.add_imm(x64::Reg::RSP, 32);
        // Result in RAX
        
        return true;
    }
    
    // Math functions using x87 FPU
    bool generate_dll_sqrt(const ast::CallExpr& call) {
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
    
    bool generate_dll_sin(const ast::CallExpr& call) {
        if (call.args.empty()) { error("sin requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // Use x87: fld, fsin, fstp
        emit_.sub_imm(x64::Reg::RSP, 16);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0x04);
        emit_.buffer().emit8(0x24);  // mov [rsp], rax
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
    
    bool generate_dll_cos(const ast::CallExpr& call) {
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
    
    bool generate_dll_tan(const ast::CallExpr& call) {
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
    
    bool generate_dll_atan2(const ast::CallExpr& call) {
        if (call.args.size() < 2) { error("atan2 requires y and x arguments"); return false; }
        
        // atan2(y, x)
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // x
        if (!generate_expr(*call.args[0])) return false;
        // RAX = y, [rsp] = x
        
        emit_.sub_imm(x64::Reg::RSP, 16);
        // Push y as double
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp] - y
        
        // Get x
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x10);  // mov rax, [rsp+16]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0);  // cvtsi2sd xmm0, rax  
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd [rsp], xmm0
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // fld qword [rsp] - x
        
        // fpatan: arctan(st(1)/st(0))
        emit_.buffer().emit8(0xD9); emit_.buffer().emit8(0xF3);  // fpatan
        
        emit_.buffer().emit8(0xDD); emit_.buffer().emit8(0x1C); emit_.buffer().emit8(0x24);  // fstp qword [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10);
        emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24);  // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0);  // cvttsd2si rax, xmm0
        emit_.add_imm(x64::Reg::RSP, 24);  // 16 + 8 for pushed x
        
        return true;
    }
    
    bool generate_dll_floor(const ast::CallExpr& call) {
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
    
    bool generate_dll_ceil(const ast::CallExpr& call) {
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
    
    bool generate_dll_abs(const ast::CallExpr& call) {
        if (call.args.empty()) { error("abs requires an argument"); return false; }
        if (!generate_expr(*call.args[0])) return false;
        
        // Integer abs: mov rdx, rax; neg rax; cmovs rax, rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC2);  // mov rdx, rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x48);
        emit_.buffer().emit8(0xC2);  // cmovs rax, rdx
        
        return true;
    }
    
    bool generate_dll_pow(const ast::CallExpr& call) {
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
        std::size_t loop_start = emit_.buffer().size();
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xD2);  // test rdx, rdx
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x09);  // jz +9 (exit loop)
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0xAF);
        emit_.buffer().emit8(0xC1);  // imul rax, rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCA);  // dec rdx
        emit_.buffer().emit8(0xEB);  // jmp
        std::int8_t offset = static_cast<std::int8_t>(loop_start - emit_.buffer().size() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(offset));
        
        return true;
    }

    bool generate_dll_malloc(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx
        emit_.pop(x64::Reg::R8);  // restore size
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }
    
    bool generate_dll_free(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx
        emit_.pop(x64::Reg::R8);  // ptr from stack
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_imm(x64::Reg::RSP, 32);
        
        return true;
    }

    bool generate_dll_array_new(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx
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

    bool generate_dll_array_get(const ast::CallExpr& call) {
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

    bool generate_dll_array_set(const ast::CallExpr& call) {
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

    bool generate_dll_array_len(const ast::CallExpr& call) {
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

    bool generate_dll_array_free(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx (flags = 0)
        emit_.pop(x64::Reg::R8);  // alloc base from stack

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_imm(x64::Reg::RSP, 32);

        return true;
    }

    bool generate_dll_string_length(const ast::CallExpr& call) {
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
        std::size_t loop_top = emit_.buffer().pos();

        // cmp byte [rcx], 0
        emit_.buffer().emit8(0x80);
        emit_.buffer().emit8(0x39);
        emit_.buffer().emit8(0x00);

        // je done
        std::size_t done_patch = emit_.jcc_rel32(x64::Emitter::CC_E);

        // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        // inc rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC0);

        // jmp loop_top
        std::int32_t loop_rel = static_cast<std::int32_t>(
            loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        // done: rax = length
        emit_.patch_jump(done_patch);

        return true;
    }

    bool generate_dll_string_append(const ast::CallExpr& call) {
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
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        std::size_t len_a_loop = emit_.buffer().pos();
        // cmp byte [rcx], 0
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x39); emit_.buffer().emit8(0x00);
        std::size_t len_a_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // inc rcx, inc rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC0);
        emit_.jmp_rel32(static_cast<std::int32_t>(len_a_loop - emit_.buffer().pos() - 5));
        emit_.patch_jump(len_a_done);
        // rax = len_a
        emit_.push(x64::Reg::RAX);  // save len_a
        // stack: [str_a, str_b, len_a]

        // --- inline strlen(b) ---
        // load str_b from [rsp+8]
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 8);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        std::size_t len_b_loop = emit_.buffer().pos();
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x39); emit_.buffer().emit8(0x00);
        std::size_t len_b_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC0);
        emit_.jmp_rel32(static_cast<std::int32_t>(len_b_loop - emit_.buffer().pos() - 5));
        emit_.patch_jump(len_b_done);
        // rax = len_b
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx (flags=0)
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
        // rcx = src (str_a), rdx = dst (buffer), r8 = count (len_a)
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 32);  // str_a
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);             // buffer
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 16);    // len_a

        std::size_t cpy_a_loop = emit_.buffer().pos();
        // test r8, r8 / je done
        emit_.test(x64::Reg::R8, x64::Reg::R8);
        std::size_t cpy_a_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // movzx rax, byte [rcx]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01);
        // mov byte [rdx], al
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x02);
        // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        // inc rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);
        // dec r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC8);
        emit_.jmp_rel32(static_cast<std::int32_t>(cpy_a_loop - emit_.buffer().pos() - 5));
        emit_.patch_jump(cpy_a_done);
        // rdx now points to buffer + len_a

        // --- memcpy b into buffer + len_a ---
        // rcx = src (str_b), rdx = already at right spot, r8 = count (len_b)
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 24);  // str_b
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 8);     // len_b

        std::size_t cpy_b_loop = emit_.buffer().pos();
        emit_.test(x64::Reg::R8, x64::Reg::R8);
        std::size_t cpy_b_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // movzx rax, byte [rcx]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01);
        // mov byte [rdx], al
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x02);
        // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        // inc rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);
        // dec r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC8);
        emit_.jmp_rel32(static_cast<std::int32_t>(cpy_b_loop - emit_.buffer().pos() - 5));
        emit_.patch_jump(cpy_b_done);

        // null terminate: mov byte [rdx], 0
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);

        // return buffer ptr
        emit_.pop(x64::Reg::RAX);   // buffer
        emit_.add_imm(x64::Reg::RSP, 32);  // pop len_b, len_a, str_b, str_a
        // stack: clean

        return true;
    }

    bool generate_dll_int_to_string(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx (flags=0)
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
        std::size_t jns_pos = emit_.buffer().size();
        emit_.buffer().emit8(0x79); emit_.buffer().emit8(0x00);  // jns .positive (short jump)

        // negative: negate and set flag
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8);  // neg r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xC7); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x01); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00);  // mov r9, 1

        // .positive:
        std::size_t positive_label = emit_.buffer().size();
        emit_.buffer().data()[jns_pos + 1] = static_cast<std::uint8_t>(positive_label - jns_pos - 2);

        // zero check
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t jnz_pos = emit_.buffer().size();
        emit_.buffer().emit8(0x75); emit_.buffer().emit8(0x00);  // jnz .nonzero

        // its zero, just write '0'
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x30);  // mov byte [rdi], '0'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        std::size_t jmp_done_pos = emit_.buffer().size();
        emit_.buffer().emit8(0xEB); emit_.buffer().emit8(0x00);  // jmp .done

        // .nonzero: div-by-10 loop
        std::size_t nonzero_label = emit_.buffer().size();
        emit_.buffer().data()[jnz_pos + 1] = static_cast<std::uint8_t>(nonzero_label - jnz_pos - 2);

        emit_.buffer().emit8(0xB9); emit_.buffer().emit32(10);  // mov ecx, 10

        std::size_t loop_start = emit_.buffer().size();
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov rax, r8
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor rdx, rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xF1);  // div rcx
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0);  // mov r8, rax (quotient)
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xC2); emit_.buffer().emit8(0x30);  // add dl, '0'
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x17);  // mov [rdi], dl
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC0);  // test r8, r8
        emit_.buffer().emit8(0x75);  // jnz .loop
        std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().size() - 1);
        emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));

        // .done:
        std::size_t done_label = emit_.buffer().size();
        emit_.buffer().data()[jmp_done_pos + 1] = static_cast<std::uint8_t>(done_label - jmp_done_pos - 2);

        // prepend '-' if negative
        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85); emit_.buffer().emit8(0xC9);  // test r9, r9
        std::size_t jz_nosign_pos = emit_.buffer().size();
        emit_.buffer().emit8(0x74); emit_.buffer().emit8(0x00);  // jz .no_sign

        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x07); emit_.buffer().emit8(0x2D);  // mov byte [rdi], '-'
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCF);  // dec rdi

        // .no_sign:
        std::size_t nosign_label = emit_.buffer().size();
        emit_.buffer().data()[jz_nosign_pos + 1] = static_cast<std::uint8_t>(nosign_label - jz_nosign_pos - 2);

        // rdi points one before first char, inc to get start of string
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC7);  // inc rdi

        // return pointer to start of string in rax
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xF8);  // mov rax, rdi

        // clean up stack: pop buffer (discard), pop value (discard)
        emit_.pop(x64::Reg::RCX);   // discard buffer
        emit_.pop(x64::Reg::RCX);   // discard value

        return true;
    }

    bool generate_dll_string_equals(const ast::CallExpr& call) {
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

    bool generate_dll_string_substring(const ast::CallExpr& call) {
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
        emit_.buffer().emit8(0x31); emit_.buffer().emit8(0xD2);  // xor edx, edx (flags=0)
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

        std::size_t cpy_loop = emit_.buffer().pos();
        emit_.test(x64::Reg::R8, x64::Reg::R8);
        std::size_t cpy_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        // movzx rax, byte [rcx]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01);
        // mov byte [rdx], al
        emit_.buffer().emit8(0x88); emit_.buffer().emit8(0x02);
        // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        // inc rdx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);
        // dec r8
        emit_.buffer().emit8(0x49); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC8);
        emit_.jmp_rel32(static_cast<std::int32_t>(cpy_loop - emit_.buffer().pos() - 5));
        emit_.patch_jump(cpy_done);

        // null terminate: mov byte [rdx], 0
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);

        // return buffer
        emit_.pop(x64::Reg::RAX);          // buffer
        emit_.add_imm(x64::Reg::RSP, 24);  // pop len, start, str

        return true;
    }

    bool generate_dll_print_char(const ast::CallExpr& call) {
        // print a single char by making a tiny null-terminated string on the stack
        if (call.args.empty()) {
            error("print_char requires a character argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        // rax = char value

        // sub rsp, 16 — stack space for our 2-byte string (aligned)
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

    bool generate_dll_virtual_protect(const ast::CallExpr& call) {
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
    
    bool generate_dll_virtual_alloc(const ast::CallExpr& call) {
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
    
    bool generate_dll_virtual_free(const ast::CallExpr& call) {
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

    bool generate_dll_get_module(const ast::CallExpr& call) {
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

    bool generate_dll_scan(const ast::CallExpr& call) {
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
        std::size_t loop_start = emit_.buffer().size();
        
        // cmp rdi, r12; jae scalar_fallback
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x39);
        emit_.buffer().emit8(0xE7);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x83);
        std::size_t scalar_jmp = emit_.buffer().size();
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
        std::size_t next16_jmp = emit_.buffer().size();
        emit_.buffer().emit32(0);
        
        // Found matches! Save mask in EBX
        emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC3);
        
        std::size_t bit_loop = emit_.buffer().size();
        
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
                fail_jumps.push_back(emit_.buffer().size());
                emit_.buffer().emit32(0);
            }
        }
        
        // All matched! Return RSI
        emit_.mov(x64::Reg::RAX, x64::Reg::RSI);
        emit_.buffer().emit8(0xE9);  // jmp done
        std::size_t done_jmp = emit_.buffer().size();
        emit_.buffer().emit32(0);
        
        // try_next_bit:
        std::size_t try_next_bit = emit_.buffer().size();
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
                               static_cast<std::int32_t>(emit_.buffer().size() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(bit_back));
        
        // next_16:
        std::size_t next16_label = emit_.buffer().size();
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
                                static_cast<std::int32_t>(emit_.buffer().size() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(loop_back));
        
        // scalar_fallback:
        std::size_t scalar_label = emit_.buffer().size();
        {
            std::int32_t offset = static_cast<std::int32_t>(scalar_label - scalar_jmp - 4);
            auto* data = emit_.buffer().data();
            data[scalar_jmp + 0] = static_cast<std::uint8_t>(offset & 0xFF);
            data[scalar_jmp + 1] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
            data[scalar_jmp + 2] = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
            data[scalar_jmp + 3] = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        }
        
        // Scalar loop for tail
        std::size_t scalar_loop = emit_.buffer().size();
        
        // cmp rdi, rdx; jae not_found
        emit_.cmp(x64::Reg::RDI, x64::Reg::RDX);
        emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x83);
        std::size_t notfound_jmp = emit_.buffer().size();
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
        std::size_t scalar_next_jmp = emit_.buffer().size();
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
                scalar_fail.push_back(emit_.buffer().size());
                emit_.buffer().emit32(0);
            }
        }
        
        // Match! Return RDI
        emit_.mov(x64::Reg::RAX, x64::Reg::RDI);
        emit_.buffer().emit8(0xE9);
        std::size_t done_jmp2 = emit_.buffer().size();
        emit_.buffer().emit32(0);
        
        // scalar_next:
        std::size_t scalar_next = emit_.buffer().size();
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
                                  static_cast<std::int32_t>(emit_.buffer().size() + 4);
        emit_.buffer().emit32(static_cast<std::uint32_t>(scalar_back));
        
        // not_found:
        std::size_t notfound_label = emit_.buffer().size();
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
        std::size_t done_label = emit_.buffer().size();
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

    // Generic single-arg builtin (like string_length, print_string)
    bool generate_builtin_string_op(const ast::CallExpr& call, void* fn_ptr) {
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
    bool generate_builtin_print_variadic(const ast::CallExpr& call, bool add_newline) {
        // For each argument, print it
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            const auto& arg = call.args[i];
            
            // Add space before (except first arg)
            if (i > 0) {
                // Print a space character
                if (dll_mode_ && runtime_print_str_) {
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
                    if (runtime_print_str_) {
                        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(runtime_print_str_));
                        emit_.call(x64::Reg::RAX);
                    } else if (dll_mode_) {
                        // DLL mode: call print stub
                        emit_.buffer().emit8(0xE8);  // call rel32
                        std::int32_t rel = static_cast<std::int32_t>(0x100) - 
                            static_cast<std::int32_t>(0x240 + emit_.buffer().size() + 4);
                        emit_.buffer().emit32(static_cast<std::uint32_t>(rel));
                    }
                    emit_.add_imm(x64::Reg::RSP, 32);
                    continue;
                }
            }
            
            // Integer/other types - use print_int
            if (!generate_expr(*arg)) return false;
            
            if (dll_mode_) {
                // DLL mode: call the print_int function
                // We need to replicate what generate_dll_builtin_print_int does
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
                
                std::size_t loop_start = emit_.buffer().size();
                
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
                std::int8_t loop_offset = static_cast<std::int8_t>(loop_start - emit_.buffer().size() - 1);
                emit_.buffer().emit8(static_cast<std::uint8_t>(loop_offset));
                
                // Null terminate
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02);
                emit_.buffer().emit8(0x00);  // mov byte [rdx], 0
                
                emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
                emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x24);
                emit_.buffer().emit8(0x20);  // lea rcx, [rsp+0x20]
                
                emit_startup_call(print_offset_);
                
                emit_.add_imm(x64::Reg::RSP, 64);
            } else if (runtime_print_int_) {
                emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(runtime_print_int_));
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
            } else if (runtime_print_str_) {
                // jit mode doesnt have println yet
            }
        }
        
        return true;
    }

    // Two-argument builtin (like write_file, array_get, string_append)
    bool generate_builtin_two_arg(const ast::CallExpr& call, void* fn_ptr) {
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
    bool generate_builtin_three_arg(const ast::CallExpr& call, void* fn_ptr) {
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
    bool generate_builtin_string_get_char(const ast::CallExpr& call) {
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
        
        if (runtime_string_get_char_) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(runtime_string_get_char_));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_imm(x64::Reg::RSP, 32);
        // RAX has the character
        
        return true;
    }

    // ========================================================================
    // memory ops - direct inline codegen, no function call overhead
    // ========================================================================

    bool generate_builtin_mem_read_i64(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_read_i32(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_read_i16(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_read_i8(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_write_i64(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_write_i32(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_write_i16(const ast::CallExpr& call) {
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

    bool generate_builtin_mem_write_i8(const ast::CallExpr& call) {
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
        if (name == "get_module") return reinterpret_cast<void*>(&opus_get_module);
        if (name == "load_library") return reinterpret_cast<void*>(&opus_load_library);
        if (name == "get_proc") return reinterpret_cast<void*>(&opus_get_proc);
        if (name == "ffi_call0") return reinterpret_cast<void*>(&opus_ffi_call0);
        if (name == "ffi_call1") return reinterpret_cast<void*>(&opus_ffi_call1);
        if (name == "ffi_call2") return reinterpret_cast<void*>(&opus_ffi_call2);
        if (name == "ffi_call3") return reinterpret_cast<void*>(&opus_ffi_call3);
        if (name == "ffi_call4") return reinterpret_cast<void*>(&opus_ffi_call4);
        if (name == "msgbox") return reinterpret_cast<void*>(&opus_msgbox);
        if (name == "get_last_error") return reinterpret_cast<void*>(&opus_get_last_error);
        if (name == "virtual_protect") return reinterpret_cast<void*>(&opus_virtual_protect);
        if (name == "get_current_process") return reinterpret_cast<void*>(&opus_get_current_process);
        if (name == "get_current_process_id") return reinterpret_cast<void*>(&opus_get_current_process_id);
        // Console functions
        if (name == "alloc_console") return reinterpret_cast<void*>(&opus_alloc_console);
        if (name == "free_console") return reinterpret_cast<void*>(&opus_free_console);
        if (name == "set_console_title") return reinterpret_cast<void*>(&opus_set_console_title);
        return nullptr;
    }

    bool generate_builtin_ffi_zero_arg(const std::string& fn_name) {
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

    bool generate_builtin_ffi_one_arg(const ast::CallExpr& call, const std::string& fn_name) {
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

    bool generate_builtin_ffi_two_arg(const ast::CallExpr& call, const std::string& fn_name) {
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

    bool generate_builtin_ffi_three_arg(const ast::CallExpr& call, const std::string& fn_name) {
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

    bool generate_builtin_ffi_four_arg(const ast::CallExpr& call, const std::string& fn_name) {
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

    bool generate_builtin_ffi_five_arg(const ast::CallExpr& call, const std::string& fn_name) {
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
        
        emit_.sub_imm(x64::Reg::RSP, 40);  // 32 shadow + 8 for 5th arg alignment
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
        emit_.mov_load(x64::Reg::RAX, x64::Reg::R12, 0x00);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::R12, 0x08);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::R12, 0x10);
        emit_.mov_load(x64::Reg::R8, x64::Reg::R12, 0x18);
        emit_.call(x64::Reg::RAX);

        // store result at context+0x20
        emit_.mov_store(x64::Reg::R12, 0x20, x64::Reg::RAX);

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
    bool generate_spawn(const ast::SpawnExpr& spawn) {
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
            // stub was cached, no new code emitted - undo the jump
            // actually we cant undo, but the jump target is right here so its a nop jump
        }
        emit_.patch_jump(jmp_over_stub);

        // allocate context in current scope (48 bytes = 6 slots)
        // we grab 6 stack slots from the current scope
        auto& ctx_sym = current_scope_->define("__spawn_ctx_" + std::to_string(emit_.buffer().pos()),
            Type::make_primitive(PrimitiveType::I64), true);
        std::int32_t ctx_base = ctx_sym.stack_offset;
        // reserve 5 more slots (total 48 bytes)
        for (int i = 0; i < 5; ++i) {
            current_scope_->next_offset -= 8;
        }

        // evaluate args first (before we fill the context, since eval can trash regs)
        std::vector<std::int32_t> arg_temps;
        for (std::size_t i = 0; i < spawn.args.size() && i < 3; ++i) {
            if (!generate_expr(*spawn.args[i])) return false;
            // stash in a temp slot
            auto& tmp = current_scope_->define("__spawn_arg_" + std::to_string(i) + "_" + std::to_string(emit_.buffer().pos()),
                Type::make_primitive(PrimitiveType::I64), true);
            emit_.mov_store(x64::Reg::RBP, tmp.stack_offset, x64::Reg::RAX);
            arg_temps.push_back(tmp.stack_offset);
        }

        // fill context: the 48-byte block goes from ctx_base-40 (low) to ctx_base (high)
        // context pointer = rbp + ctx_base - 40 (lowest address)
        // [ctx+0x00] = function_ptr, [ctx+0x08] = arg0, [ctx+0x10] = arg1,
        // [ctx+0x18] = arg2, [ctx+0x20] = result_slot, [ctx+0x28] = padding
        std::int32_t ctx_ptr_off = ctx_base - 40;

        // store function ptr at [rbp + ctx_ptr_off]
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea rax, [rip+disp32]
        std::size_t fn_addr_fixup = emit_.buffer().size();
        emit_.buffer().emit32(0);
        call_fixups_.push_back({fn_addr_fixup, fn_name});
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_off, x64::Reg::RAX);

        // fill args into context slots
        for (std::size_t i = 0; i < arg_temps.size(); ++i) {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, arg_temps[i]);
            emit_.mov_store(x64::Reg::RBP, ctx_ptr_off + static_cast<std::int32_t>((i + 1) * 8), x64::Reg::RAX);
        }

        // zero result slot
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_off + 0x20, x64::Reg::RAX);

        // save context base address for await
        auto& ctx_ptr_sym = current_scope_->define("__spawn_ctxptr_" + std::to_string(emit_.buffer().pos()),
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.lea(x64::Reg::RAX, x64::Reg::RBP, ctx_ptr_off);
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_sym.stack_offset, x64::Reg::RAX);

        // set up CreateThread args
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        // r8 = stub address
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea r8, [rip+disp32]
        std::size_t stub_fixup = emit_.buffer().size();
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
        // also save context ptr offset so await can find it
        // we stash the ctx_ptr_sym offset in a map keyed by... hmm
        // actually the user does: let h = spawn foo(x)
        // then: let result = await h
        // await gets h (the handle) from the variable
        // but it also needs the context ptr
        // we store ctx_ptr right after the handle in the callers frame
        // the let statement will store rax (handle) at some offset
        // we need ctx_ptr at offset-8
        // BUT we dont control where let puts it...

        // simplest v1: store ctx_ptr in rax's upper bits? no thats insane
        // ok: we push ctx_ptr onto a side-stack (vector in codegen)
        // await pops from it. this works if spawns and awaits are balanced
        // and in the same order (LIFO). good enough for v1.
        spawn_context_stack_.push_back(ctx_ptr_sym.stack_offset);

        return true;
    }

    // await expression codegen
    // waits for thread handle, reads result from context, cleans up
    bool generate_await(const ast::AwaitExpr& await_expr) {
        // evaluate handle expression -> rax
        if (!generate_expr(*await_expr.handle)) return false;

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
        emit_.push(x64::Reg::RCX); // save again for after close
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.add_imm(x64::Reg::RSP, 32);
        emit_.pop(x64::Reg::RCX); // clean up saved handle

        // load result from context
        if (!spawn_context_stack_.empty()) {
            std::int32_t ctx_offset = spawn_context_stack_.back();
            spawn_context_stack_.pop_back();
            // ctx_offset points to the __spawn_ctxptr variable
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, ctx_offset);
            // rax = context base ptr, result is at +0x20
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0x20);
        } else {
            // no context available, return 0
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }

        return true;
    }

    // parallel for codegen
    // parallel for codegen
    // extracts body into internal function, spawns N threads, waits for all
    bool generate_parallel_for(const ast::ParallelForStmt& pfor) {
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

        // check empty range: if start >= end, skip
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, start_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, end_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t skip_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // collect captured variables from parent scopes
        // the body runs in a separate thread so it cant access the callers stack
        // we pass the callers rbp through context+0x18 and copy vars into the body frame
        struct CapturedVar {
            std::string name;
            std::int32_t parent_offset;
        };
        std::vector<CapturedVar> captures;
        for (Scope* s = current_scope_; s != nullptr; s = s->parent) {
            for (const auto& [name, sym] : s->symbols) {
                // skip internal vars and the loop variable
                if (name.starts_with("__pfor_")) continue;
                if (name == pfor.var_name) continue;
                captures.push_back({name, sym.stack_offset});
            }
        }

        // emit the loop body as an internal function
        // takes (start_idx in rcx, end_idx in rdx, parent_rbp in r8)
        std::string body_fn = "__pfor_body_" + std::to_string(emit_.buffer().pos());

        // jump over the body function
        std::size_t jmp_over = emit_.jmp_rel32_placeholder();

        // emit body function
        functions_[body_fn] = FunctionInfo{
            .name = body_fn,
            .return_type = Type::make_primitive(PrimitiveType::I64)
        };
        auto& body_info = functions_[body_fn];
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
        auto& loop_var = current_scope_->define(pfor.var_name,
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, iter_start.stack_offset);
        emit_.mov_store(x64::Reg::RBP, loop_var.stack_offset, x64::Reg::RAX);

        // inner loop: while loop_var < iter_end
        std::size_t loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, loop_var.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, iter_end.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // generate body statements
        for (const auto& stmt : pfor.body) {
            if (!generate_stmt(*stmt)) { pop_scope(); return false; }
        }

        // increment loop var
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, loop_var.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, loop_var.stack_offset, x64::Reg::RAX);

        // jump back
        std::int32_t loop_rel = static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        emit_.patch_jump(loop_exit);

        // return 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.epilogue();
        pop_scope();
        body_info.code_size = emit_.buffer().pos() - body_info.code_offset;

        // emit thread entry stub right after body (before we patch jmp_over)
        std::size_t stub_offset = emit_thread_entry_stub(body_fn);

        // patch the jump-over (skips both body func and stub)
        emit_.patch_jump(jmp_over);

        // back in the caller scope - now do the parallel dispatch
        // get cpu count via GetSystemInfo
        // SYSTEM_INFO is 48 bytes, dwNumberOfProcessors at offset 32
        emit_.sub_imm(x64::Reg::RSP, 48); // SYSTEM_INFO on stack
        emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
        emit_.sub_imm(x64::Reg::RSP, 32); // shadow
        emit_iat_call_raw(pe::iat::GetSystemInfo);
        emit_.add_imm(x64::Reg::RSP, 32);
        // read dwNumberOfProcessors (offset 32 in SYSTEM_INFO, its a DWORD)
        emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20); // mov eax, [rsp+0x20]
        emit_.add_imm(x64::Reg::RSP, 48); // pop SYSTEM_INFO

        // rax = num_cores, save it
        auto& ncores_sym = current_scope_->define("__pfor_ncores",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, ncores_sym.stack_offset, x64::Reg::RAX);

        // cap at 64 cores max (sanity)
        emit_.cmp_imm(x64::Reg::RAX, 64);
        std::size_t cap_skip = emit_.jcc_rel32(x64::Emitter::CC_LE);
        emit_.mov_imm32(x64::Reg::RAX, 64);
        emit_.mov_store(x64::Reg::RBP, ncores_sym.stack_offset, x64::Reg::RAX);
        emit_.patch_jump(cap_skip);

        // compute range = end - start
        auto& range_sym = current_scope_->define("__pfor_range",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, end_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, start_sym.stack_offset);
        emit_.sub(x64::Reg::RAX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RBP, range_sym.stack_offset, x64::Reg::RAX);

        // cap ncores to range so we dont spawn useless threads
        // if range < ncores, set ncores = range
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t range_ok = emit_.jcc_rel32(x64::Emitter::CC_GE);
        emit_.mov_store(x64::Reg::RBP, ncores_sym.stack_offset, x64::Reg::RAX);
        emit_.patch_jump(range_ok);

        // chunk_size = range / ncores
        auto& chunk_sym = current_scope_->define("__pfor_chunk",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.cqo();
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.idiv(x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RBP, chunk_sym.stack_offset, x64::Reg::RAX);
        // rdx = remainder, save it
        auto& rem_sym = current_scope_->define("__pfor_rem",
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, rem_sym.stack_offset, x64::Reg::RDX);

        // allocate arrays for contexts and handles on stack
        // max 64 cores * 48 = 3072 for contexts, 64 * 8 = 512 for handles
        // stack grows down so we reserve slots then use the bottom as base for upward indexing
        auto& ctx_arr_sym = current_scope_->define("__pfor_ctxarr",
            Type::make_primitive(PrimitiveType::I64), true);
        for (int i = 0; i < 447; ++i) current_scope_->next_offset -= 8;
        std::int32_t ctx_arr_base = current_scope_->next_offset + 8; // bottom of reserved block

        auto& hdl_arr_sym = current_scope_->define("__pfor_hdlarr",
            Type::make_primitive(PrimitiveType::I64), true);
        for (int i = 0; i < 63; ++i) current_scope_->next_offset -= 8;
        std::int32_t hdl_arr_base = current_scope_->next_offset + 8; // bottom of reserved block

        // spawn loop: for each core, fill context and create thread
        auto& idx_sym = current_scope_->define("__pfor_i",
            Type::make_primitive(PrimitiveType::I64), true);

        // grow rsp to cover all our locals so function calls dont clobber them
        // next_offset is negative, tells us how deep the frame goes
        std::int32_t frame_needed = (-current_scope_->next_offset + 15) & ~15;
        emit_.sub_imm(x64::Reg::RSP, frame_needed);

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, idx_sym.stack_offset, x64::Reg::RAX);

        std::size_t spawn_loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t spawn_loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // compute context address: ctx_arr_base + i * 48
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RAX, 48);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, ctx_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        // rdx = context ptr for this thread

        emit_.push(x64::Reg::RDX); // save ctx ptr

        // [rdx+0x00] = body function ptr
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea rax, [rip+disp32]
        std::size_t body_fixup = emit_.buffer().size();
        emit_.buffer().emit32(0);
        call_fixups_.push_back({body_fixup, body_fn});
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, 0x00, x64::Reg::RAX);

        // [rdx+0x08] = chunk_start = start + i * chunk_size
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, chunk_sym.stack_offset);
        emit_.imul(x64::Reg::RAX, x64::Reg::RCX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, start_sym.stack_offset);
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, 0x08, x64::Reg::RAX);

        // [rdx+0x10] = chunk_end
        // last thread absorbs remainder, others get chunk_start + chunk_size
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t not_last = emit_.jcc_rel32(x64::Emitter::CC_NE);
        // last thread: end = pfor.end
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, end_sym.stack_offset);
        std::size_t chunk_end_done = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(not_last);
        // not last: end = chunk_start + chunk_size
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RDX, 0x08); // chunk_start
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, chunk_sym.stack_offset);
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.patch_jump(chunk_end_done);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, 0x10, x64::Reg::RAX);

        // [rdx+0x18] = parent rbp (so body can access captured vars)
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, 0x18, x64::Reg::RBP);

        // [rdx+0x20] = result (zero)
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.push(x64::Reg::RDX);
        emit_.mov_store(x64::Reg::RDX, 0x20, x64::Reg::RAX);

        // call CreateThread
        emit_.pop(x64::Reg::RDX); // context ptr -> goes in r9
        emit_.push(x64::Reg::RDX); // save for handle storage

        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX); // lpThreadAttributes = NULL
        emit_.mov(x64::Reg::R9, x64::Reg::RDX);    // lpParameter = context
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);  // dwStackSize = 0
        // r8 = stub address
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea r8, [rip+disp32]
        std::size_t stub_fix2 = emit_.buffer().size();
        emit_.buffer().emit32(0);
        std::int32_t srel = static_cast<std::int32_t>(stub_offset)
            - static_cast<std::int32_t>(stub_fix2 + 4);
        emit_.buffer().patch32(stub_fix2, static_cast<std::uint32_t>(srel));

        // 5th + 6th args on stack
        emit_.sub_imm(x64::Reg::RSP, 48);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateThread);
        emit_.add_imm(x64::Reg::RSP, 48);

        // store handle: hdl_arr[i] = rax
        emit_.pop(x64::Reg::RDX); // pop saved context ptr
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RCX, 8);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, hdl_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RDX, 0, x64::Reg::RAX);

        // i++
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, idx_sym.stack_offset, x64::Reg::RAX);

        std::int32_t spawn_loop_rel = static_cast<std::int32_t>(
            spawn_loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(spawn_loop_rel);
        emit_.patch_jump(spawn_loop_exit);

        // WaitForMultipleObjects(ncores, handle_array, TRUE, INFINITE)
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, hdl_arr_base);
        emit_.mov_imm32(x64::Reg::R8, 1);  // bWaitAll = TRUE
        emit_.mov_imm32(x64::Reg::R9, -1); // INFINITE
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::WaitForMultipleObjects);
        emit_.add_imm(x64::Reg::RSP, 32);

        // CloseHandle loop
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RBP, idx_sym.stack_offset, x64::Reg::RAX);

        std::size_t close_loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ncores_sym.stack_offset);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t close_loop_exit = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // load handle[i]
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RAX, 8);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, hdl_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RDX, 0);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.add_imm(x64::Reg::RSP, 32);

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, idx_sym.stack_offset);
        emit_.add_imm(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, idx_sym.stack_offset, x64::Reg::RAX);

        std::int32_t close_rel = static_cast<std::int32_t>(
            close_loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(close_rel);
        emit_.patch_jump(close_loop_exit);

        // patch the empty-range skip
        emit_.patch_jump(skip_patch);

        pop_scope();
        return true;
    }
    // atomic operations codegen
    bool generate_atomic_op(const ast::AtomicOpExpr& atomic) {
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

