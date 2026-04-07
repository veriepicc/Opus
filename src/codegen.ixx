// codegen - AST to x64

export module opus.codegen;

import opus.types;
import opus.ast;
import opus.errors;
import opus.x64;
import opus.pe;
import opus.lexer;
import opus.parser;
import opus.stdlib;
import std;

extern "C" {
    std::int64_t opus_get_module(std::int64_t name_handle);
    std::int64_t opus_load_library(std::int64_t name_handle);
    std::int64_t opus_get_proc(std::int64_t module, std::int64_t name_handle);
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

    // ffi
    RtI64I64    get_module = nullptr;
    RtI64I64    load_library = nullptr;
    RtI64I64I64 get_proc = nullptr;
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
    std::unordered_map<std::string, std::vector<const ast::Expr*>> inline_bindings;
    Scope* parent = nullptr;
    std::int32_t next_offset = -8;  // Start at RBP-8

    Symbol* lookup(const std::string& name) {
        if (auto it = symbols.find(name); it != symbols.end()) {
            return &it->second;
        }
        if (parent) return parent->lookup(name);
        return nullptr;
    }

    const ast::Expr* lookup_inline(const std::string& name) const {
        if (auto it = inline_bindings.find(name); it != inline_bindings.end() && !it->second.empty()) {
            return it->second.back();
        }
        if (parent) return parent->lookup_inline(name);
        return nullptr;
    }

    void push_inline(const std::string& name, const ast::Expr* expr) {
        inline_bindings[name].push_back(expr);
    }

    void pop_inline(const std::string& name) {
        auto it = inline_bindings.find(name);
        if (it == inline_bindings.end()) {
            return;
        }
        if (!it->second.empty()) {
            it->second.pop_back();
        }
        if (it->second.empty()) {
            inline_bindings.erase(it);
        }
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
    const ast::FnDecl* decl = nullptr;
    bool is_extern = false;
    bool single_use_inline_safe = false;
};

enum class OwnedLocalState {
    None,
    Live
};

class CodeGenerator {
public:
    CodeGenerator() = default;

    void set_output_kind(OutputKind kind) {
        output_kind_ = kind;
        print_offset_ = pe::PeImageGenerator::DLL_PRINT_OFFSET;
        set_title_offset_ = pe::PeImageGenerator::DLL_SET_TITLE_OFFSET;
        alloc_console_offset_ = pe::PeImageGenerator::DLL_ALLOC_CONSOLE_OFFSET;
        print_hex_offset_ = pe::PeImageGenerator::DLL_PRINT_HEX_OFFSET;
        crash_handler_offset_ = pe::PeImageGenerator::CRASH_HANDLER_OFFSET;
        if (output_is_exe(output_kind_)) {
            print_offset_ = pe::PeImageGenerator::EXE_PRINT_OFFSET;
            set_title_offset_ = pe::PeImageGenerator::EXE_SET_TITLE_OFFSET;
            alloc_console_offset_ = pe::PeImageGenerator::EXE_ALLOC_CONSOLE_OFFSET;
            print_hex_offset_ = pe::PeImageGenerator::EXE_PRINT_HEX_OFFSET;
            crash_handler_offset_ = pe::PeImageGenerator::EXE_CRASH_HANDLER_OFFSET;
        }
    }

    bool is_native_image_output() const { return output_is_native_image(output_kind_); }
    bool is_exe_output() const { return output_is_exe(output_kind_); }
    bool is_dll_output() const { return output_is_dll(output_kind_); }

    // set source file path for import resolution
    void set_source_path(const std::string& path) { source_path_ = path; }
    void set_project_root(const std::string& path) { project_root_ = path; }
    void set_import_search_paths(const std::vector<std::string>& paths) { import_search_paths_ = paths; }

    // set the user code offset for rip-relative calculations
    // defaults to debug layout for backward compat
    void set_user_code_offset(std::size_t offset) { user_code_offset_ = offset; }
    std::size_t user_code_offset() const { return user_code_offset_; }

    // enable auto-parallelization of for loops
    void set_auto_parallel(bool enabled) { auto_parallel_ = enabled; }
    bool is_auto_parallel() const { return auto_parallel_; }

    void set_runtime_pointers(const RuntimePointers& rt) {
        rt_ = rt;
    }

    // Generate code for a module
    [[nodiscard]] bool generate(const ast::Module& mod) {
        module_name_ = mod.name;

        // first pass: collect function signatures, structs, and globals
        for (const auto& decl : mod.decls) {
            if (decl->is<ast::TypeAliasDecl>()) {
                if (!register_type_alias_decl(decl->as<ast::TypeAliasDecl>())) return false;
            } else if (decl->is<ast::FnDecl>()) {
                const auto& fn = decl->as<ast::FnDecl>();
                if (!claim_symbol_name(function_owners_, "function", fn.name, current_symbol_owner())) return false;
                auto resolved_ret = canonicalize_type(fn.return_type);
                if (!resolved_ret) return false;
                std::vector<Type> params;
                params.reserve(fn.params.size());
                for (const auto& param : fn.params) {
                    auto resolved = canonicalize_type(param.type);
                    if (!resolved) return false;
                    params.push_back(std::move(*resolved));
                }
                functions_[fn.name] = FunctionInfo{
                    .name = fn.name,
                    .code_offset = 0,
                    .code_size = 0,
                    .return_type = std::move(*resolved_ret),
                    .param_types = std::move(params),
                    .decl = &fn,
                    .is_extern = fn.attrs.is_extern()
                };
            } else if (decl->is<ast::StructDecl>()) {
                if (!generate_struct_decl(decl->as<ast::StructDecl>())) return false;
            } else if (decl->is<ast::StaticDecl>()) {
                if (!register_static_decl(decl->as<ast::StaticDecl>())) return false;
            }
        }

        // imports can contribute globals too, so collect their declarations before
        // deciding whether this module needs a writable globals slot
        for (const auto& decl : mod.decls) {
            if (decl->is<ast::ImportDecl>()) {
                if (!collect_import_metadata(decl->as<ast::ImportDecl>())) return false;
            }
        }

        // reserve a writable globals slot only when native-image globals exist
        if (is_native_image_output() && !globals_.empty()) {
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
            if (it->second.is_extern) {
                error(std::format(
                    "extern function '{}' is declaration-only right now; use load_library/get_proc + a typed fn alias for runtime calls",
                    fixup.target_fn));
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
    static constexpr std::int32_t CTX_ARG3    = 0x20;
    static constexpr std::int32_t CTX_RESULT  = 0x28;
    static constexpr std::int32_t CTX_SIZE    = 0x30;

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

    struct SelfUpdateInfo {
        Symbol* sym = nullptr;
        ast::BinaryExpr::Op op = ast::BinaryExpr::Op::Add;
        const ast::Expr* rhs = nullptr;
        std::int64_t rhs_imm = 0;
        bool has_rhs_imm = false;
    };

    struct FieldSelfUpdateInfo {
        Symbol* base_sym = nullptr;
        std::int32_t field_offset = 0;
        ast::BinaryExpr::Op op = ast::BinaryExpr::Op::Add;
        const ast::Expr* rhs = nullptr;
        std::int64_t rhs_imm = 0;
        bool has_rhs_imm = false;
    };

    struct WhileRegisterPlan {
        std::int64_t limit = 0;
        std::vector<std::pair<Symbol*, x64::Reg>> bindings;
        struct InductionPlan {
            Symbol* accum_sym = nullptr;
            x64::Reg term_reg = x64::Reg::RDX;
            std::int64_t step = 0;
            std::int32_t disp = 0;
        };
        std::optional<InductionPlan> induction;
    };

    struct WhileMultiStatePlan {
        struct Update {
            Symbol* sym = nullptr;
            ast::BinaryExpr::Op op = ast::BinaryExpr::Op::Add;
            bool has_rhs_imm = false;
            std::int64_t rhs_imm = 0;
            Symbol* rhs_sym = nullptr;
        };

        std::int64_t limit = 0;
        Symbol* counter_sym = nullptr;
        std::vector<std::pair<Symbol*, x64::Reg>> bindings;
        std::vector<Update> updates;
    };

    struct WhileBoundPlan {
        std::int64_t limit = 0;
        std::vector<std::pair<Symbol*, x64::Reg>> bindings;
    };

    struct WhileSimdKernelPlan {
        enum class Op : std::uint8_t { Add, Sub, Mul };
        struct Step {
            Op op = Op::Add;
            Symbol* dst = nullptr;
            Symbol* lhs = nullptr;
            Symbol* rhs = nullptr;
        };

        std::int64_t limit = 0;
        Symbol* counter_sym = nullptr;
        std::vector<std::pair<Symbol*, std::uint8_t>> zmm_bindings;
        std::vector<Symbol*> init_loads;
        std::vector<Symbol*> final_stores;
        std::vector<Step> steps;
    };

    struct WhileMultiStateReductionPlan {
        std::int64_t limit = 0;
        Symbol* counter_sym = nullptr;
        Symbol* accum_sym = nullptr;
        std::vector<Symbol*> contributor_syms;
        std::int64_t term_step = 0;
    };

    struct WhileAlternatingBranchReductionPlan {
        std::int64_t limit = 0;
        Symbol* counter_sym = nullptr;
        Symbol* accum_sym = nullptr;
        Symbol* even_sym = nullptr;
        Symbol* odd_sym = nullptr;
        std::int64_t even_step = 0;
        std::int64_t odd_step = 0;
    };

    struct FunctionLeafRegisterPlan {
        std::unordered_map<std::string, x64::Reg> param_regs;
        std::unordered_map<std::string, x64::Reg> local_regs;
    };

    struct FunctionSavedLocalPlan {
        std::vector<std::pair<std::string, x64::Reg>> locals;
        std::vector<x64::Reg> save_regs;
        std::size_t prefix_count = 0;
    };

    struct LeaRhsPattern {
        x64::Reg index_reg = x64::Reg::RAX;
        std::uint8_t scale = 1;
        std::int32_t disp = 0;
    };

    x64::Emitter emit_;
    std::string module_name_;
    std::string source_path_;
    std::string project_root_;
    std::vector<std::string> import_search_paths_;
    mutable std::optional<std::string> last_import_resolution_error_;
    enum class ImportState { InProgress, Completed };
    std::unordered_map<std::string, ImportState> import_states_;
    std::vector<std::string> import_stack_;
    struct ImportedModuleExports {
        std::vector<std::string> functions;
        std::vector<std::string> types;
        std::vector<std::string> enums;
        std::vector<std::string> globals;
    };
    std::unordered_map<std::string, ImportedModuleExports> imported_module_exports_;
    std::unordered_set<std::string> imported_module_metadata_;
    std::unordered_map<std::string, std::shared_ptr<ast::Module>> imported_module_asts_;
    std::unordered_map<std::string, std::string> function_aliases_;
    std::unordered_map<std::string, std::string> type_aliases_;
    std::unordered_map<std::string, Type> named_type_aliases_;
    std::unordered_map<std::string, std::string> enum_aliases_;
    std::unordered_map<std::string, std::string> global_aliases_;
    std::unordered_map<std::string, std::string> function_owners_;
    std::unordered_map<std::string, std::string> type_owners_;
    std::unordered_map<std::string, std::string> enum_owners_;
    std::unordered_map<std::string, std::string> global_owners_;
    std::unordered_map<std::string, std::string> import_namespace_owners_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::vector<std::string> errors_;
    
    Scope* current_scope_ = nullptr;
    std::vector<std::unique_ptr<Scope>> scopes_;
    std::unordered_map<const Symbol*, x64::Reg> register_bindings_;
    std::unordered_map<std::string, x64::Reg> pending_named_register_bindings_;
    std::vector<std::unordered_set<std::string>> bound_stack_elision_names_;
    std::vector<x64::Reg> current_function_saved_regs_;
    Type current_function_return_type_;
    bool has_current_function_return_type_ = false;
    std::unordered_map<const Symbol*, OwnedLocalState> owned_locals_;
    struct InductionBinding {
        x64::Reg term_reg = x64::Reg::RDX;
        std::int64_t step = 0;
    };
    std::unordered_map<const Symbol*, InductionBinding> accumulator_inductions_;
    std::unordered_map<const Symbol*, InductionBinding> counter_inductions_;
    
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
        emit_.add_smart(x64::Reg::RSP, 32);
    }

    [[nodiscard]] bool emit_runtime_alloc_bytes(std::int32_t size) {
        if (size <= 0) {
            error("internal error: attempted to allocate non-positive byte count");
            return false;
        }

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.mov_imm32(x64::Reg::R8, size);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool emit_runtime_free_reg(x64::Reg ptr_reg) {
        emit_.push(ptr_reg);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_smart(x64::Reg::RSP, 40);
        return true;
    }

    [[nodiscard]] bool emit_thread_spawn_call() {
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        emit_.sub_imm(x64::Reg::RSP, 48); // shadow + 2 stack args
        emit_.mov_imm32(x64::Reg::RAX, 0);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateThread);
        emit_.add_smart(x64::Reg::RSP, 48);
        return true;
    }

    [[nodiscard]] bool emit_thread_wait_call() {
        emit_.mov_imm32(x64::Reg::RDX, -1);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::WaitForSingleObject);
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool emit_close_handle_call() {
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
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

    OutputKind output_kind_ = OutputKind::Raw;
    std::size_t user_code_offset_ = pe::PeImageGenerator::STARTUP_CODE_SIZE;  // default to debug layout

    // runtime routine offsets in .text - set from the current native-image output kind
    std::size_t print_offset_ = pe::PeImageGenerator::DLL_PRINT_OFFSET;
    std::size_t set_title_offset_ = pe::PeImageGenerator::DLL_SET_TITLE_OFFSET;
    std::size_t alloc_console_offset_ = pe::PeImageGenerator::DLL_ALLOC_CONSOLE_OFFSET;
    std::size_t print_hex_offset_ = pe::PeImageGenerator::DLL_PRINT_HEX_OFFSET;
    std::size_t crash_handler_offset_ = pe::PeImageGenerator::CRASH_HANDLER_OFFSET;

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
    std::unordered_map<std::string_view, BuiltinHandler> shared_builtins_;
    bool builtin_tables_initialized_ = false;

    void init_builtin_tables() {
        if (builtin_tables_initialized_) return;
        builtin_tables_initialized_ = true;

        // ---- native-image builtins (checked first for exe/dll output) ----
        dll_builtins_ = {
            {"dll_print",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"print",           [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"print_string",    [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"puts",            [this](const ast::CallExpr& c) { return generate_dll_builtin_print(c); }},
            {"dll_set_title",   [this](const ast::CallExpr& c) { return generate_dll_builtin_set_title(c); }},
            {"set_title",       [this](const ast::CallExpr& c) { return generate_dll_builtin_set_title(c); }},
            {"alloc_console",   [this](const ast::CallExpr& c) { return generate_dll_builtin_alloc_console(c); }},
            {"print_int",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_dec(c); }},
            {"print_dec",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_dec(c); }},
            {"print_hex",       [this](const ast::CallExpr& c) { return generate_dll_builtin_print_hex(c); }},
            {"exit",            [this](const ast::CallExpr& c) { return generate_dll_exit(c); }},
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
                emit_.add_smart(x64::Reg::RSP, 32);
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
            {"make_string",     [this](const ast::CallExpr& c) { return generate_dll_make_string(c); }},
            {"int_to_string",   [this](const ast::CallExpr& c) { return generate_dll_int_to_string(c); }},
            {"itoa",            [this](const ast::CallExpr& c) { return generate_dll_int_to_string(c); }},
            {"string_equals",   [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"streq",           [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"string_starts_with", [this](const ast::CallExpr& c) { return generate_dll_string_starts_with(c); }},
            {"starts_with",     [this](const ast::CallExpr& c) { return generate_dll_string_starts_with(c); }},
            {"string_substring",[this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"substr",          [this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"print_char",      [this](const ast::CallExpr& c) { return generate_dll_print_char(c); }},
            {"putc",            [this](const ast::CallExpr& c) { return generate_dll_print_char(c); }},
            {"is_alpha",        [this](const ast::CallExpr& c) { return generate_dll_is_alpha(c); }},
            {"is_digit",        [this](const ast::CallExpr& c) { return generate_dll_is_digit(c); }},
            {"is_alnum",        [this](const ast::CallExpr& c) { return generate_dll_is_alnum(c); }},
            {"is_whitespace",   [this](const ast::CallExpr& c) { return generate_dll_is_whitespace(c); }},
            {"parse_int",       [this](const ast::CallExpr& c) { return generate_dll_parse_int(c); }},
            {"atoi",            [this](const ast::CallExpr& c) { return generate_dll_parse_int(c); }},
            {"buffer_new",      [this](const ast::CallExpr& c) { return generate_dll_buffer_new(c); }},
            {"buffer_push",     [this](const ast::CallExpr& c) { return generate_dll_buffer_push(c); }},
            {"buffer_len",      [this](const ast::CallExpr& c) { return generate_dll_buffer_len(c); }},
            {"write_bytes",     [this](const ast::CallExpr& c) { return generate_dll_write_bytes(c); }},
            // memory protection
            {"virtual_protect", [this](const ast::CallExpr& c) { return generate_dll_virtual_protect(c); }},
            {"virtual_alloc",   [this](const ast::CallExpr& c) { return generate_dll_virtual_alloc(c); }},
            {"virtual_free",    [this](const ast::CallExpr& c) { return generate_dll_virtual_free(c); }},
            // pattern scanner
            {"scan",            [this](const ast::CallExpr& c) { return generate_dll_scan(c); }},
            // module functions
            {"get_module",      [this](const ast::CallExpr& c) { return generate_dll_get_module(c); }},
        };

        // ---- shared builtins ----
        // note: "print" and "println" still have split handling baked into their handlers
        shared_builtins_ = {
            {"print_int",       [this](const ast::CallExpr& c) { return generate_builtin_print_int(c); }},
            {"print",           [this](const ast::CallExpr& c) {
                if (is_native_image_output()) return generate_dll_builtin_print(c);
                return generate_builtin_one_arg(c, as_void(rt_.print_str));
            }},
            {"println",         [this](const ast::CallExpr& c) {
                if (is_native_image_output()) {
                    if (!generate_dll_builtin_print(c)) return false;
                    emit_.sub_imm(x64::Reg::RSP, 32);
                    emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x04);
                    emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x0A);
                    emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44);
                    emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x01);
                    emit_.buffer().emit8(0x00);
                    emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
                    emit_startup_call(print_offset_);
                    emit_.add_smart(x64::Reg::RSP, 32);
                } else {
                    if (!generate_builtin_one_arg(c, as_void(rt_.print_str))) return false;
                }
                return true;
            }},
            {"read_file",       [this](const ast::CallExpr& c) {
                if (is_native_image_output()) return generate_dll_read_file(c);
                return generate_builtin_one_arg(c, as_void(rt_.read_file));
            }},
            {"string_length",   [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.string_length)); }},
            {"strlen",          [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.string_length)); }},
            {"string_get_char", [this](const ast::CallExpr& c) { return generate_dll_string_get_char(c); }},
            {"char_at",         [this](const ast::CallExpr& c) { return generate_dll_string_get_char(c); }},
            {"print_string",    [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_string)); }},
            {"puts",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_string)); }},
            {"write_file",      [this](const ast::CallExpr& c) {
                if (is_native_image_output()) return generate_dll_write_file(c);
                return generate_builtin_two_arg(c, as_void(rt_.write_file));
            }},
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
            {"make_string",     [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.make_string)); }},
            {"int_to_string",   [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.int_to_string)); }},
            {"itoa",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.int_to_string)); }},
            {"print_char",      [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_char)); }},
            {"putc",            [this](const ast::CallExpr& c) { return generate_builtin_one_arg(c, as_void(rt_.print_char)); }},
            {"string_equals",   [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"streq",           [this](const ast::CallExpr& c) { return generate_dll_string_equals(c); }},
            {"string_substring",[this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"substr",          [this](const ast::CallExpr& c) { return generate_dll_string_substring(c); }},
            {"is_alpha",        [this](const ast::CallExpr& c) { return generate_dll_is_alpha(c); }},
            {"is_digit",        [this](const ast::CallExpr& c) { return generate_dll_is_digit(c); }},
            {"is_alnum",        [this](const ast::CallExpr& c) { return generate_dll_is_alnum(c); }},
            {"is_whitespace",   [this](const ast::CallExpr& c) { return generate_dll_is_whitespace(c); }},
            {"string_starts_with", [this](const ast::CallExpr& c) { return generate_dll_string_starts_with(c); }},
            {"starts_with",     [this](const ast::CallExpr& c) { return generate_dll_string_starts_with(c); }},
            {"exit",            [this](const ast::CallExpr& c) { return generate_dll_exit(c); }},
            {"write_bytes",     [this](const ast::CallExpr& c) { return generate_dll_write_bytes(c); }},
            {"buffer_new",      [this](const ast::CallExpr& c) { return generate_dll_buffer_new(c); }},
            {"buffer_push",     [this](const ast::CallExpr& c) { return generate_dll_buffer_push(c); }},
            {"buffer_len",      [this](const ast::CallExpr& c) { return generate_dll_buffer_len(c); }},
            {"parse_int",       [this](const ast::CallExpr& c) { return generate_dll_parse_int(c); }},
            {"atoi",            [this](const ast::CallExpr& c) { return generate_dll_parse_int(c); }},
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
            // explicit SIMD builtins
            {"simd_has_avx2",      [this](const ast::CallExpr&) { return generate_builtin_simd_has_avx2(); }},
            {"simd_has_avx512f",   [this](const ast::CallExpr&) { return generate_builtin_simd_has_avx512f(); }},
            {"simd_has_avx512dq",  [this](const ast::CallExpr&) { return generate_builtin_simd_has_avx512dq(); }},
            {"simd_i32x8_add",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x8_add(c); }},
            {"simd_i32x8_sub",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x8_sub(c); }},
            {"simd_i32x8_mul",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x8_mul(c); }},
            {"simd_i64x4_add",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x4_add(c); }},
            {"simd_i64x4_sub",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x4_sub(c); }},
            {"simd_i32x8_splat",   [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x8_splat(c); }},
            {"simd_i64x4_splat",   [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x4_splat(c); }},
            {"simd_i32x16_add",    [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x16_add(c); }},
            {"simd_i32x16_sub",    [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x16_sub(c); }},
            {"simd_i32x16_mul",    [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x16_mul(c); }},
            {"simd_i64x8_add",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x8_add(c); }},
            {"simd_i64x8_sub",     [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x8_sub(c); }},
            {"simd_i32x16_splat",  [this](const ast::CallExpr& c) { return generate_builtin_simd_i32x16_splat(c); }},
            {"simd_i64x8_splat",   [this](const ast::CallExpr& c) { return generate_builtin_simd_i64x8_splat(c); }},
            // ffi - windows api
            {"get_module",      [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "get_module"); }},
            {"load_library",    [this](const ast::CallExpr& c) { return generate_builtin_ffi_one_arg(c, "load_library"); }},
            {"get_proc",        [this](const ast::CallExpr& c) { return generate_builtin_ffi_two_arg(c, "get_proc"); }},
            {"get_last_error",  [this](const ast::CallExpr&) { return generate_builtin_ffi_zero_arg("get_last_error"); }},
            {"virtual_protect", [this](const ast::CallExpr& c) { return generate_builtin_ffi_fixed_arg(c, "virtual_protect", 3); }},
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

    template <typename Map>
    static void append_unique_candidate_keys(const Map& map,
                                             std::vector<std::string>& out,
                                             std::unordered_set<std::string>& seen) {
        for (const auto& [name, _] : map) {
            if (seen.insert(name).second) {
                out.push_back(name);
            }
        }
    }

    [[nodiscard]] std::vector<std::string> collect_value_name_candidates() const {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;

        for (const Scope* scope = current_scope_; scope; scope = scope->parent) {
            append_unique_candidate_keys(scope->symbols, out, seen);
        }
        append_unique_candidate_keys(globals_, out, seen);
        append_unique_candidate_keys(functions_, out, seen);
        append_unique_candidate_keys(function_aliases_, out, seen);
        append_unique_candidate_keys(global_aliases_, out, seen);
        append_unique_candidate_keys(import_namespace_owners_, out, seen);

        return out;
    }

    [[nodiscard]] std::vector<std::string> collect_function_name_candidates() const {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;
        append_unique_candidate_keys(functions_, out, seen);
        append_unique_candidate_keys(function_aliases_, out, seen);
        return out;
    }

    [[nodiscard]] std::vector<std::string> collect_type_name_candidates() const {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;
        append_unique_candidate_keys(structs_, out, seen);
        append_unique_candidate_keys(type_aliases_, out, seen);
        append_unique_candidate_keys(named_type_aliases_, out, seen);
        append_unique_candidate_keys(enum_aliases_, out, seen);
        return out;
    }

    [[nodiscard]] std::vector<std::string> collect_import_member_candidates(std::string_view prefix) const {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;
        const std::string needle = std::string(prefix) + ".";

        auto append_suffixes = [&](const auto& map) {
            for (const auto& [name, _] : map) {
                if (name.rfind(needle, 0) != 0) {
                    continue;
                }
                std::string suffix = name.substr(needle.size());
                if (seen.insert(suffix).second) {
                    out.push_back(std::move(suffix));
                }
            }
        };

        append_suffixes(function_aliases_);
        append_suffixes(global_aliases_);
        append_suffixes(type_aliases_);
        append_suffixes(enum_aliases_);
        return out;
    }

    void error_undefined_name(std::string_view kind,
                              std::string_view name,
                              const std::vector<std::string>& candidates) {
        std::string msg = std::format("undefined {}: {}", kind, name);
        if (auto suggestion = find_closest_match(name, candidates)) {
            msg += std::format(" (did you mean '{}'?)", *suggestion);
        }
        error(msg);
    }

    void error_unknown_type(std::string_view kind, std::string_view name) {
        std::string msg = std::format("unknown {}: {}", kind, name);
        if (auto suggestion = find_closest_match(name, collect_type_name_candidates())) {
            msg += std::format(" (did you mean '{}'?)", *suggestion);
        }
        error(msg);
    }

    void error_unknown_import_member(std::string_view prefix, std::string_view member) {
        std::string msg = std::format("import namespace '{}' has no exported member '{}'", prefix, member);
        if (auto suggestion = find_closest_match(member, collect_import_member_candidates(prefix))) {
            msg += std::format(" (did you mean '{}'?)", *suggestion);
        }
        error(msg);
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

    [[nodiscard]] std::optional<x64::Reg> lookup_bound_reg(const Symbol* sym) const {
        if (!sym) {
            return std::nullopt;
        }
        if (auto it = register_bindings_.find(sym); it != register_bindings_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool should_elide_bound_stack_slot(const Symbol* sym) const {
        if (!sym || !lookup_bound_reg(sym)) {
            return false;
        }
        for (auto it = bound_stack_elision_names_.rbegin(); it != bound_stack_elision_names_.rend(); ++it) {
            if (it->contains(sym->name)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool reg_is_bound(x64::Reg reg) const {
        for (const auto& [_, bound] : register_bindings_) {
            if (bound == reg) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static constexpr std::array<x64::Reg, 6> scratch_reg_candidates() {
        return {
            x64::Reg::R11,
            x64::Reg::R10,
            x64::Reg::R9,
            x64::Reg::R8,
            x64::Reg::RDX,
            x64::Reg::RCX,
        };
    }

    [[nodiscard]] std::size_t available_scratch_reg_count(std::initializer_list<x64::Reg> avoid = {}) const {
        std::size_t count = 0;
        for (x64::Reg candidate : scratch_reg_candidates()) {
            if (reg_is_bound(candidate)) {
                continue;
            }
            if (std::find(avoid.begin(), avoid.end(), candidate) != avoid.end()) {
                continue;
            }
            count++;
        }
        return count;
    }

    [[nodiscard]] std::optional<x64::Reg> pick_scratch_reg(std::initializer_list<x64::Reg> avoid = {}) const {
        const auto candidates = scratch_reg_candidates();

        for (x64::Reg candidate : candidates) {
            if (reg_is_bound(candidate)) {
                continue;
            }
            if (std::find(avoid.begin(), avoid.end(), candidate) != avoid.end()) {
                continue;
            }
            return candidate;
        }

        return std::nullopt;
    }

    [[nodiscard]] std::size_t scope_frame_size(std::size_t scope_mark) const {
        std::int32_t min_stack_offset = 0;
        for (std::size_t i = scope_mark; i < scopes_.size(); ++i) {
            for (const auto& [_, sym] : scopes_[i]->symbols) {
                if (lookup_bound_reg(&sym)) {
                    continue;
                }
                min_stack_offset = std::min(min_stack_offset, sym.stack_offset);
            }
        }
        if (min_stack_offset >= 0) {
            return 0;
        }
        return static_cast<std::size_t>(-min_stack_offset);
    }

    void patch_scope_frame(std::size_t patch_site, std::size_t scope_mark, std::size_t save_reg_count = 0) {
        std::size_t alloc = x64::Emitter::aligned_stack_allocation(scope_frame_size(scope_mark), save_reg_count);
        emit_.buffer().patch32(patch_site, static_cast<std::uint32_t>(alloc));
    }

    void emit_current_epilogue() {
        emit_.epilogue(current_function_saved_regs_);
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

    [[nodiscard]] bool emit_load_global_base_ptr(x64::Reg dst) {
        if (!has_global_slot_) return false;
        emit_lea_rip_slot(dst);
        emit_.mov_load(dst, dst, 0);
        return true;
    }

    [[nodiscard]] bool emit_store_global_base_ptr(x64::Reg value_reg) {
        if (!has_global_slot_) return false;
        emit_lea_rip_slot(x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RCX, 0, value_reg);
        return true;
    }

    // extract struct/class name from a Type, handling both StructType and string variants
    std::optional<std::string> get_struct_name(const Type& type) {
        if (std::holds_alternative<StructType>(type.kind))
            return resolve_type_name(std::get<StructType>(type.kind).name);
        if (std::holds_alternative<PointerType>(type.kind)) {
            const auto& ptr = std::get<PointerType>(type.kind);
            return get_struct_name(*ptr.pointee);
        }
        if (std::holds_alternative<std::string>(type.kind))
            return resolve_type_name(std::get<std::string>(type.kind));
        return std::nullopt;
    }

    [[nodiscard]] bool allows_interior_mutation(const Symbol& sym) {
        return sym.is_mut || get_struct_name(sym.type).has_value() || is_ptr_like_type(sym.type);
    }

    [[nodiscard]] std::string describe_rebinding_target(std::string_view kind,
                                                        std::string_view name) const {
        return std::format("cannot rebind immutable {}: {}", kind, name);
    }

    [[nodiscard]] std::string describe_field_mutation_target(std::string_view name) const {
        return std::format("cannot mutate field through immutable non-reference value: {}", name);
    }

    std::optional<std::string> get_qualified_name(const ast::Expr& expr) const {
        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            return ident->name;
        }
        if (auto* field = std::get_if<ast::FieldExpr>(&expr.kind)) {
            return get_qualified_name(*field);
        }
        return std::nullopt;
    }

    std::optional<std::string> get_qualified_name(const ast::FieldExpr& field) const {
        auto base = get_qualified_name(*field.base);
        if (!base) {
            return std::nullopt;
        }
        return *base + "." + field.field;
    }

    std::vector<std::string> get_import_prefixes(const ast::ImportDecl& imp) const {
        std::vector<std::string> prefixes;
        prefixes.push_back(imp.path);
        if (imp.alias) {
            prefixes.push_back(*imp.alias);
        }
        return prefixes;
    }

    ImportedModuleExports collect_imported_module_exports(const ast::Module& mod) const {
        ImportedModuleExports exports;
        for (const auto& decl : mod.decls) {
            if (decl->is<ast::FnDecl>()) {
                exports.functions.push_back(decl->as<ast::FnDecl>().name);
            } else if (decl->is<ast::StructDecl>()) {
                exports.types.push_back(decl->as<ast::StructDecl>().name);
            } else if (decl->is<ast::ClassDecl>()) {
                exports.types.push_back(decl->as<ast::ClassDecl>().name);
            } else if (decl->is<ast::TypeAliasDecl>()) {
                exports.types.push_back(decl->as<ast::TypeAliasDecl>().name);
            } else if (decl->is<ast::EnumDecl>()) {
                exports.enums.push_back(decl->as<ast::EnumDecl>().name);
            } else if (decl->is<ast::StaticDecl>()) {
                exports.globals.push_back(decl->as<ast::StaticDecl>().name);
            }
        }
        return exports;
    }

    bool register_import_aliases(const ast::ImportDecl& imp, const ImportedModuleExports& exports, std::string_view module_owner) {
        for (const auto& prefix : get_import_prefixes(imp)) {
            if (!claim_symbol_name(import_namespace_owners_, "import namespace", prefix, module_owner, true)) {
                return false;
            }
            for (const auto& name : exports.functions) {
                function_aliases_.try_emplace(prefix + "." + name, name);
            }
            for (const auto& name : exports.types) {
                type_aliases_.try_emplace(prefix + "." + name, name);
            }
            for (const auto& name : exports.enums) {
                enum_aliases_.try_emplace(prefix + "." + name, name);
            }
            for (const auto& name : exports.globals) {
                global_aliases_.try_emplace(prefix + "." + name, name);
            }
        }
        return true;
    }

    std::string resolve_function_name(std::string_view name) const {
        if (auto it = function_aliases_.find(std::string(name)); it != function_aliases_.end()) {
            return it->second;
        }
        return std::string(name);
    }

    std::string resolve_type_name(std::string_view name) const {
        if (auto it = type_aliases_.find(std::string(name)); it != type_aliases_.end()) {
            return it->second;
        }
        return std::string(name);
    }

    std::optional<Type> canonicalize_type_impl(const Type& type, std::unordered_set<std::string>& seen) {
        return std::visit(overloaded{
            [&](const PrimitiveType&) -> std::optional<Type> {
                return type.clone();
            },
            [&](const ArrayType& arr) -> std::optional<Type> {
                auto elem = canonicalize_type_impl(*arr.element, seen);
                if (!elem) return std::nullopt;
                Type out;
                out.kind = ArrayType{
                    .element = std::make_unique<Type>(std::move(*elem)),
                    .size = arr.size
                };
                return out;
            },
            [&](const FunctionType& fn) -> std::optional<Type> {
                FunctionType out_fn;
                for (const auto& param : fn.params) {
                    auto resolved = canonicalize_type_impl(*param, seen);
                    if (!resolved) return std::nullopt;
                    out_fn.params.push_back(std::make_unique<Type>(std::move(*resolved)));
                }
                auto ret = canonicalize_type_impl(*fn.ret, seen);
                if (!ret) return std::nullopt;
                out_fn.ret = std::make_unique<Type>(std::move(*ret));
                out_fn.is_variadic = fn.is_variadic;
                Type out;
                out.kind = std::move(out_fn);
                return out;
            },
            [&](const StructType& st) -> std::optional<Type> {
                StructType out_st;
                out_st.name = resolve_type_name(st.name);
                for (const auto& [field_name, field_type] : st.fields) {
                    auto resolved = canonicalize_type_impl(*field_type, seen);
                    if (!resolved) return std::nullopt;
                    out_st.fields.emplace_back(field_name, std::make_unique<Type>(std::move(*resolved)));
                }
                Type out;
                out.kind = std::move(out_st);
                return out;
            },
            [&](const PointerType& ptr) -> std::optional<Type> {
                auto pointee = canonicalize_type_impl(*ptr.pointee, seen);
                if (!pointee) return std::nullopt;
                Type out;
                out.kind = PointerType{
                    .pointee = std::make_unique<Type>(std::move(*pointee)),
                    .is_mut = ptr.is_mut
                };
                return out;
            },
            [&](const std::string& name) -> std::optional<Type> {
                std::string resolved_name = resolve_type_name(name);
                if (auto it = named_type_aliases_.find(resolved_name); it != named_type_aliases_.end()) {
                    if (seen.contains(resolved_name)) {
                        error(std::format("cyclic type alias detected for '{}'", resolved_name));
                        return std::nullopt;
                    }
                    seen.insert(resolved_name);
                    auto resolved = canonicalize_type_impl(it->second, seen);
                    seen.erase(resolved_name);
                    return resolved;
                }
                Type out;
                out.kind = resolved_name;
                return out;
            }
        }, type.kind);
    }

    std::optional<Type> canonicalize_type(const Type& type) {
        std::unordered_set<std::string> seen;
        return canonicalize_type_impl(type, seen);
    }

    [[nodiscard]] static bool types_equal(const Type& lhs, const Type& rhs) {
        if (lhs.kind.index() != rhs.kind.index()) {
            return false;
        }

        return std::visit(overloaded{
            [&](const PrimitiveType& a, const PrimitiveType& b) {
                return a == b;
            },
            [&](const ArrayType& a, const ArrayType& b) {
                return a.size == b.size && types_equal(*a.element, *b.element);
            },
            [&](const FunctionType& a, const FunctionType& b) {
                if (a.is_variadic != b.is_variadic || a.params.size() != b.params.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < a.params.size(); ++i) {
                    if (!types_equal(*a.params[i], *b.params[i])) {
                        return false;
                    }
                }
                return types_equal(*a.ret, *b.ret);
            },
            [&](const StructType& a, const StructType& b) {
                return a.name == b.name;
            },
            [&](const PointerType& a, const PointerType& b) {
                return a.is_mut == b.is_mut && types_equal(*a.pointee, *b.pointee);
            },
            [&](const std::string& a, const std::string& b) {
                return a == b;
            },
            [&](const auto&, const auto&) {
                return false;
            }
        }, lhs.kind, rhs.kind);
    }

    [[nodiscard]] std::optional<Type> infer_stmt_suite_value_type(const ast::StmtSuite& stmts) {
        std::size_t scope_mark = scopes_.size();
        push_scope();

        auto cleanup_scope = [&]() {
            while (scopes_.size() > scope_mark) {
                pop_scope();
            }
        };

        for (std::size_t i = 0; i < stmts.size(); ++i) {
            const auto& stmt = *stmts[i];
            bool is_tail = (i + 1 == stmts.size());
            if (stmt.is<ast::LetStmt>()) {
                const auto& let = stmt.as<ast::LetStmt>();
                Type let_type;
                if (!infer_let_type(let, let_type)) {
                    cleanup_scope();
                    return std::nullopt;
                }
                current_scope_->define(let.name, std::move(let_type), let.is_mut);
                continue;
            }

            if (!is_tail) {
                continue;
            }

            if (stmt.is<ast::ExprStmt>()) {
                auto inferred = infer_expr_type(*stmt.as<ast::ExprStmt>().expr);
                cleanup_scope();
                return inferred;
            }
            if (stmt.is<ast::ReturnStmt>() && stmt.as<ast::ReturnStmt>().value) {
                auto inferred = infer_expr_type(*stmt.as<ast::ReturnStmt>().value);
                cleanup_scope();
                return inferred;
            }
        }

        cleanup_scope();
        return Type::make_primitive(PrimitiveType::Void);
    }

    [[nodiscard]] std::optional<Type> infer_block_result_type(const ast::Block& block) {
        std::size_t scope_mark = scopes_.size();
        push_scope();

        auto cleanup_scope = [&]() {
            while (scopes_.size() > scope_mark) {
                pop_scope();
            }
        };

        for (const auto& stmt_ptr : block.stmts) {
            const auto& stmt = *stmt_ptr;
            if (!stmt.is<ast::LetStmt>()) {
                continue;
            }

            const auto& let = stmt.as<ast::LetStmt>();
            Type let_type;
            if (!infer_let_type(let, let_type)) {
                cleanup_scope();
                return std::nullopt;
            }
            current_scope_->define(let.name, std::move(let_type), let.is_mut);
        }

        if (!block.result) {
            cleanup_scope();
            return Type::make_primitive(PrimitiveType::Void);
        }

        auto inferred = infer_expr_type(*block.result);
        cleanup_scope();
        return inferred;
    }

    std::string resolve_enum_name(std::string_view name) const {
        if (auto it = enum_aliases_.find(std::string(name)); it != enum_aliases_.end()) {
            return it->second;
        }
        return std::string(name);
    }

    std::optional<std::string> resolve_global_alias(std::string_view name) const {
        if (auto it = global_aliases_.find(std::string(name)); it != global_aliases_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<std::string> resolve_function_alias_name(std::string_view name) const {
        std::string resolved = resolve_function_name(name);
        if (functions_.contains(resolved)) {
            return resolved;
        }
        return std::nullopt;
    }

    std::optional<Type> make_function_value_type(std::string_view resolved_name) const {
        auto it = functions_.find(std::string(resolved_name));
        if (it == functions_.end()) {
            return std::nullopt;
        }
        FunctionType fn_type;
        fn_type.params.reserve(it->second.param_types.size());
        for (const auto& param : it->second.param_types) {
            fn_type.params.push_back(std::make_unique<Type>(param.clone()));
        }
        fn_type.ret = std::make_unique<Type>(it->second.return_type.clone());
        Type out;
        out.kind = std::move(fn_type);
        return out;
    }

    std::string current_symbol_owner() const {
        if (!source_path_.empty()) {
            return source_path_;
        }
        return module_name_;
    }

    bool claim_symbol_name(std::unordered_map<std::string, std::string>& owners,
                           std::string_view kind,
                           std::string_view name,
                           std::string_view owner,
                           bool allow_same_owner_existing = false) {
        auto it = owners.find(std::string(name));
        if (it == owners.end()) {
            owners.emplace(std::string(name), std::string(owner));
            return true;
        }
        if (it->second == owner) {
            if (allow_same_owner_existing) {
                return true;
            }
            error(std::format("duplicate {} '{}' in module '{}'", kind, name, owner));
            return false;
        }
        error(std::format("{} '{}' from '{}' conflicts with {} from '{}'",
            kind, name, owner, kind, it->second));
        return false;
    }

    std::string display_module_path(std::string_view canonical) const {
        std::filesystem::path path(canonical);
        if (!project_root_.empty()) {
            std::error_code ec;
            auto rel = std::filesystem::relative(path, project_root_, ec);
            if (!ec && !rel.empty()) {
                return rel.generic_string();
            }
        }
        return path.generic_string();
    }

    std::string format_import_cycle(std::string_view canonical) const {
        std::string cycle;
        auto start = std::find(import_stack_.begin(), import_stack_.end(), std::string(canonical));
        if (start == import_stack_.end()) {
            return display_module_path(canonical);
        }
        for (auto it = start; it != import_stack_.end(); ++it) {
            if (!cycle.empty()) cycle += " -> ";
            cycle += display_module_path(*it);
        }
        if (!cycle.empty()) cycle += " -> ";
        cycle += display_module_path(canonical);
        return cycle;
    }

    std::string describe_import_request(std::string_view module_path) const {
        auto rel = import_rel_path(module_path);
        if (!source_path_.empty()) {
            return std::format("{} (from {})", rel, display_module_path(source_path_));
        }
        return rel;
    }

    std::vector<std::filesystem::path> import_search_roots() const {
        std::vector<std::filesystem::path> search_roots;
        if (!source_path_.empty()) {
            search_roots.push_back(std::filesystem::path(source_path_).parent_path());
        }
        if (!project_root_.empty()) {
            search_roots.push_back(project_root_);
        }
        for (const auto& search_path : import_search_paths_) {
            std::filesystem::path root = search_path;
            if (root.is_relative() && !project_root_.empty()) {
                root = std::filesystem::path(project_root_) / root;
            }
            search_roots.push_back(root);
        }
        if (search_roots.empty()) {
            search_roots.push_back(std::filesystem::current_path());
        }
        return search_roots;
    }

    std::vector<std::filesystem::path> resolve_import_candidates(std::string_view module_path) const {
        std::string rel_path(module_path);
        for (auto& c : rel_path) {
            if (c == '.') c = '/';
        }
        rel_path += ".op";

        std::vector<std::filesystem::path> matches;
        for (const auto& root : import_search_roots()) {
            auto candidate = std::filesystem::weakly_canonical(root / rel_path);
            if (!std::filesystem::exists(candidate)) {
                continue;
            }
            if (std::find(matches.begin(), matches.end(), candidate) == matches.end()) {
                matches.push_back(std::move(candidate));
            }
        }
        return matches;
    }

    std::string import_rel_path(std::string_view module_path) const {
        std::string rel_path(module_path);
        for (auto& c : rel_path) {
            if (c == '.') c = '/';
        }
        rel_path += ".op";
        return rel_path;
    }

    std::string embedded_import_canonical(std::string_view module_path) const {
        return std::format("<embedded-stdlib>/{}", import_rel_path(module_path));
    }

    std::optional<std::pair<std::string, std::string>> load_import_source(std::string_view module_path) const {
        last_import_resolution_error_.reset();
        auto matches = resolve_import_candidates(module_path);
        if (matches.size() > 1) {
            auto request = describe_import_request(module_path);
            std::string details;
            for (const auto& match : matches) {
                if (!details.empty()) details += "\n";
                details += std::format("  - {}", display_module_path(match.string()));
            }
            last_import_resolution_error_ = std::format(
                "ambiguous module import: {}\nmatched multiple files:\n{}",
                request,
                details
            );
            return std::nullopt;
        }

        if (matches.size() == 1) {
            const auto& full_path = matches.front();
            if (std::ifstream file(full_path); file) {
                std::stringstream buf;
                buf << file.rdbuf();
                return std::pair{full_path.string(), buf.str()};
            }
        }

        if (auto it = embedded_stdlib_sources().find(std::string(module_path)); it != embedded_stdlib_sources().end()) {
            return std::pair{embedded_import_canonical(module_path), it->second};
        }

        last_import_resolution_error_ = std::format("cannot find module: {}", describe_import_request(module_path));
        return std::nullopt;
    }

    std::string import_resolution_error(std::string_view module_path) const {
        if (last_import_resolution_error_.has_value()) {
            return *last_import_resolution_error_;
        }
        return std::format("cannot find module: {}", describe_import_request(module_path));
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
        if (decl.is<ast::TypeAliasDecl>()) {
            return register_type_alias_decl(decl.as<ast::TypeAliasDecl>());
        }
        if (decl.is<ast::ImportDecl>()) {
            return generate_import(decl.as<ast::ImportDecl>());
        }
        return true;
    }

    [[nodiscard]] bool generate_stmt_list(const std::vector<ast::StmtPtr>& stmts) {
        return generate_stmt_list_from(stmts, 0);
    }

    [[nodiscard]] bool generate_stmt_suite_value(const ast::StmtSuite& stmts) {
        if (stmts.empty()) {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
            return true;
        }

        for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
            if (!generate_stmt(*stmts[i])) {
                return false;
            }
        }

        const auto& tail = *stmts.back();
        if (tail.is<ast::ExprStmt>()) {
            return generate_expr(*tail.as<ast::ExprStmt>().expr);
        }
        if (!generate_stmt(tail)) {
            return false;
        }
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool collect_import_metadata(const ast::ImportDecl& imp) {
        auto import_source = load_import_source(imp.path);
        if (!import_source) {
            error(import_resolution_error(imp.path));
            return false;
        }
        auto [canonical, source] = *import_source;

        if (imported_module_metadata_.contains(canonical)) {
            return true;
        }

        if (std::ranges::find(import_stack_, canonical) != import_stack_.end()) {
            error(std::format("import cycle detected: {}", format_import_cycle(canonical)));
            return false;
        }

        import_stack_.push_back(canonical);

        auto abandon_import = [&]() {
            if (!import_stack_.empty() && import_stack_.back() == canonical) {
                import_stack_.pop_back();
            }
        };

        std::shared_ptr<ast::Module> mod_ptr;
        if (auto ast_it = imported_module_asts_.find(canonical); ast_it != imported_module_asts_.end()) {
            mod_ptr = ast_it->second;
        } else {
            Lexer lexer(source, canonical);
            auto tokens = lexer.tokenize_all();
            for (const auto& tok : tokens) {
                if (tok.kind == TokenKind::Error) {
                    abandon_import();
                    error(std::format("{}:{}:{}: lexer error in imported module: {}",
                        tok.loc.file, tok.loc.line, tok.loc.column, tok.text));
                    return false;
                }
            }

            Parser parser(std::move(tokens), SyntaxMode::CStyle);
            auto mod_result = parser.parse_module(canonical);
            if (!mod_result) {
                abandon_import();
                for (const auto& err : mod_result.error()) {
                    error(std::format("error in imported module {}: {}", imp.path, err.to_string()));
                }
                return false;
            }

            mod_ptr = std::make_shared<ast::Module>(std::move(*mod_result));
            imported_module_asts_[canonical] = mod_ptr;
        }

        auto saved_path = source_path_;
        source_path_ = canonical;

        auto fail_import = [&]() {
            source_path_ = saved_path;
            abandon_import();
            return false;
        };

        for (const auto& decl : mod_ptr->decls) {
            if (decl->is<ast::ImportDecl>()) {
                if (!collect_import_metadata(decl->as<ast::ImportDecl>())) {
                    return fail_import();
                }
            }
        }

        for (const auto& decl : mod_ptr->decls) {
            if (decl->is<ast::FnDecl>()) {
                const auto& fn = decl->as<ast::FnDecl>();
                if (!claim_symbol_name(function_owners_, "function", fn.name, current_symbol_owner())) {
                    return fail_import();
                }
                functions_[fn.name] = FunctionInfo{
                    .name = fn.name,
                    .code_offset = 0,
                    .code_size = 0,
                    .return_type = fn.return_type.clone(),
                    .param_types = [&fn] {
                        std::vector<Type> params;
                        params.reserve(fn.params.size());
                        for (const auto& param : fn.params) params.push_back(param.type.clone());
                        return params;
                    }(),
                    .decl = &fn
                };
            } else if (decl->is<ast::StructDecl>()) {
                if (!generate_struct_decl(decl->as<ast::StructDecl>())) {
                    return fail_import();
                }
            } else if (decl->is<ast::StaticDecl>()) {
                if (!register_static_decl(decl->as<ast::StaticDecl>())) {
                    return fail_import();
                }
            }
        }

        imported_module_exports_[canonical] = collect_imported_module_exports(*mod_ptr);
        imported_module_metadata_.insert(canonical);
        import_stack_.pop_back();
        source_path_ = saved_path;
        return true;
    }
    
    // resolve and compile an imported module
    [[nodiscard]] bool generate_import(const ast::ImportDecl& imp) {
        auto import_source = load_import_source(imp.path);
        if (!import_source) {
            error(import_resolution_error(imp.path));
            return false;
        }
        auto [canonical, source] = *import_source;

        if (auto state_it = import_states_.find(canonical); state_it != import_states_.end()) {
            if (state_it->second == ImportState::InProgress) {
                error(std::format("import cycle detected: {}", format_import_cycle(canonical)));
                return false;
            }
            if (auto it = imported_module_exports_.find(canonical); it != imported_module_exports_.end()) {
                return register_import_aliases(imp, it->second, canonical);
            }
            error(std::format("import bookkeeping error for module: {}", display_module_path(canonical)));
            return false;
        }
        import_states_[canonical] = ImportState::InProgress;
        import_stack_.push_back(canonical);

        auto abandon_import = [&]() {
            import_states_.erase(canonical);
            if (!import_stack_.empty() && import_stack_.back() == canonical) {
                import_stack_.pop_back();
            }
        };

        std::shared_ptr<ast::Module> mod_ptr;
        if (auto ast_it = imported_module_asts_.find(canonical); ast_it != imported_module_asts_.end()) {
            mod_ptr = ast_it->second;
        } else {
            Lexer lexer(source, canonical);
            auto tokens = lexer.tokenize_all();
            for (const auto& tok : tokens) {
                if (tok.kind == TokenKind::Error) {
                    abandon_import();
                    error(std::format("{}:{}:{}: lexer error in imported module: {}",
                        tok.loc.file, tok.loc.line, tok.loc.column, tok.text));
                    return false;
                }
            }

            Parser parser(std::move(tokens), SyntaxMode::CStyle);
            auto mod_result = parser.parse_module(canonical);
            if (!mod_result) {
                abandon_import();
                for (const auto& err : mod_result.error()) {
                    error(std::format("error in imported module {}: {}", imp.path, err.to_string()));
                }
                return false;
            }

            mod_ptr = std::make_shared<ast::Module>(std::move(*mod_result));
            imported_module_asts_[canonical] = mod_ptr;
        }

        bool metadata_already_collected = imported_module_metadata_.contains(canonical);
        ImportedModuleExports exports;
        if (auto it = imported_module_exports_.find(canonical); it != imported_module_exports_.end()) {
            exports = it->second;
        } else {
            exports = collect_imported_module_exports(*mod_ptr);
        }

        // save current source path, set to imported file
        auto saved_path = source_path_;
        source_path_ = canonical;

        auto fail_import = [&]() {
            source_path_ = saved_path;
            abandon_import();
            return false;
        };

        if (!metadata_already_collected) {
            // first pass: collect function signatures, structs, and globals from imported module
            for (const auto& decl : mod_ptr->decls) {
                if (decl->is<ast::FnDecl>()) {
                    const auto& fn = decl->as<ast::FnDecl>();
                    if (!claim_symbol_name(function_owners_, "function", fn.name, current_symbol_owner())) {
                        return fail_import();
                    }
                    functions_[fn.name] = FunctionInfo{
                        .name = fn.name,
                        .code_offset = 0,
                        .code_size = 0,
                        .return_type = fn.return_type.clone(),
                        .param_types = [&fn] {
                            std::vector<Type> params;
                            params.reserve(fn.params.size());
                            for (const auto& param : fn.params) params.push_back(param.type.clone());
                            return params;
                        }(),
                        .decl = &fn
                    };
                } else if (decl->is<ast::StructDecl>()) {
                    if (!generate_struct_decl(decl->as<ast::StructDecl>())) {
                        return fail_import();
                    }
                } else if (decl->is<ast::StaticDecl>()) {
                    if (!register_static_decl(decl->as<ast::StaticDecl>())) {
                        return fail_import();
                    }
                }
            }
        }

        // second pass: compile declarations
        for (const auto& decl : mod_ptr->decls) {
            if (!generate_decl(*decl)) {
                return fail_import();
            }
        }

        if (!register_import_aliases(imp, exports, canonical)) {
            return fail_import();
        }

        imported_module_exports_[canonical] = exports;
        import_states_[canonical] = ImportState::Completed;
        import_stack_.pop_back();

        source_path_ = saved_path;
        return true;
    }

    // track global variables for later initialization
    bool register_static_decl(const ast::StaticDecl& sd) {
        if (!claim_symbol_name(global_owners_, "global", sd.name, current_symbol_owner(), true)) return false;
        auto resolved_type = canonicalize_type(sd.type);
        if (!resolved_type) return false;
        // imports collect globals during metadata pre-pass using a temporary parsed
        // module. When we later compile the real imported module, refresh the
        // initializer pointer so __opus_init never walks a dangling AST node.
        if (auto it = globals_.find(sd.name); it != globals_.end()) {
            it->second.type = resolved_type->clone();
            it->second.is_mut = sd.is_mut;
            it->second.init = sd.init.get();
            return true;
        }
        
        GlobalVar gv;
        gv.name = sd.name;
        gv.type = std::move(*resolved_type);
        gv.is_mut = sd.is_mut;
        gv.offset = next_global_offset_;
        next_global_offset_ += 8;
        
        if (sd.init) {
            gv.init = sd.init.get();
        }
        
        globals_[sd.name] = std::move(gv);
        global_order_.push_back(sd.name);
        return true;
    }

    [[nodiscard]] bool register_type_alias_decl(const ast::TypeAliasDecl& alias) {
        if (!claim_symbol_name(type_owners_, "type", alias.name, current_symbol_owner(), true)) return false;
        auto resolved = canonicalize_type(alias.target);
        if (!resolved) return false;
        named_type_aliases_[alias.name] = std::move(*resolved);
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
        
        std::size_t scope_mark = scopes_.size();
        push_scope();
        std::size_t frame_patch = emit_.prologue_patchable();
        
        if (next_global_offset_ > 0) {
            if (!emit_runtime_alloc_bytes(static_cast<std::int32_t>(next_global_offset_))) return;
            
            // store base pointer so other functions can find it
            if (!emit_store_global_base_ptr(x64::Reg::RAX)) return;
            
            // run initializers
            for (const auto& name : global_order_) {
                auto& gv = globals_[name];
                if (gv.init) {
                    // evaluate initializer -> RAX
                    if (!generate_expr(*gv.init)) return;
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX); // save value
                    
                    // reload base from the active globals slot
                    if (!emit_load_global_base_ptr(x64::Reg::RAX)) return;
                    
                    // store value at base + offset
                    emit_.mov_store(x64::Reg::RAX, static_cast<std::int32_t>(gv.offset), x64::Reg::RCX);
                }
            }
        }

        patch_scope_frame(frame_patch, scope_mark);
        
        emit_.epilogue();
        pop_scope();
        
        info.code_size = emit_.buffer().pos() - info.code_offset;
    }

    [[nodiscard]] bool generate_struct_decl(const ast::StructDecl& s) {
        if (!claim_symbol_name(type_owners_, "type", s.name, current_symbol_owner(), true)) return false;
        if (structs_.contains(s.name)) return true;
        StructInfo info;
        info.name = s.name;
        for (const auto& [name, type] : s.fields) {
            auto resolved = canonicalize_type(type);
            if (!resolved) return false;
            info.fields.emplace_back(name, std::move(*resolved));
        }
        info.calculate_offsets();
        structs_[s.name] = std::move(info);
        return true;
    }
    
    [[nodiscard]] bool generate_class_decl(const ast::ClassDecl& c) {
        if (!claim_symbol_name(type_owners_, "type", c.name, current_symbol_owner())) return false;
        // Register fields (same as struct)
        StructInfo info;
        info.name = c.name;
        for (const auto& [name, type] : c.fields) {
            auto resolved = canonicalize_type(type);
            if (!resolved) return false;
            info.fields.emplace_back(name, std::move(*resolved));
        }
        info.calculate_offsets();
        structs_[c.name] = std::move(info);
        
        // methods get mangled as ClassName_MethodName
        for (const auto& method : c.methods) {
            std::string mangled_name = c.name + "_" + method.name;
            if (!claim_symbol_name(function_owners_, "function", mangled_name, current_symbol_owner())) return false;
            
            // Register function info first
            auto resolved_ret = canonicalize_type(method.return_type);
            if (!resolved_ret) return false;
            std::vector<Type> method_params;
            method_params.reserve(method.params.size() + (method.has_receiver() ? 1u : 0u));
            if (method.has_receiver()) {
                method_params.push_back(Type::make_primitive(PrimitiveType::Ptr));
            }
            for (const auto& param : method.params) {
                auto resolved = canonicalize_type(param.type);
                if (!resolved) return false;
                method_params.push_back(std::move(*resolved));
            }
            functions_[mangled_name] = FunctionInfo{
                .name = mangled_name,
                .return_type = std::move(*resolved_ret),
                .param_types = std::move(method_params)
            };
            
            // Generate the method code
            auto& func_info = functions_[mangled_name];
            func_info.code_offset = emit_.buffer().pos();
            
            std::size_t scope_mark = scopes_.size();
            push_scope();
            
            std::size_t frame_patch = emit_.prologue_patchable();
            
            current_class_name_ = c.name;
            std::size_t arg_index_base = 0;
            if (method.has_receiver()) {
                // implicit self param in rcx (windows x64 convention)
                Type self_type;
                self_type.kind = c.name;
                Symbol& self_sym = current_scope_->define("self", std::move(self_type), true);
                self_sym.is_param = true;
                emit_.mov_store(x64::Reg::RBP, self_sym.stack_offset, x64::Reg::RCX);
                arg_index_base = 1;
            }
            
            const x64::Reg param_regs[] = {x64::Reg::RCX, x64::Reg::RDX, x64::Reg::R8, x64::Reg::R9};
            
            for (std::size_t i = 0; i < method.params.size(); ++i) {
                const auto& param = method.params[i];
                auto resolved = canonicalize_type(param.type);
                if (!resolved) return false;
                Symbol& sym = current_scope_->define(param.name, std::move(*resolved), param.is_mut);
                sym.is_param = true;
                const std::size_t arg_index = i + arg_index_base;
                if (arg_index < std::size(param_regs)) {
                    emit_.mov_store(x64::Reg::RBP, sym.stack_offset, param_regs[arg_index]);
                } else {
                    // stack params use the full call arg index including the receiver when present
                    std::int32_t src_offset = 16 + static_cast<std::int32_t>(arg_index) * 8;
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, src_offset);
                    emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
                }
            }
            
            // Generate body
            if (!generate_stmt_list(method.body)) {
                pop_scope();
                return false;
            }

            patch_scope_frame(frame_patch, scope_mark);
            
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
        if (!claim_symbol_name(enum_owners_, "enum", e.name, current_symbol_owner())) return false;
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

    [[nodiscard]] static bool expr_contains_call(const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) { return false; },
            [&](const ast::IdentExpr&) { return false; },
            [&](const ast::UnaryExpr& e) { return expr_contains_call(*e.operand); },
            [&](const ast::BinaryExpr& e) { return expr_contains_call(*e.lhs) || expr_contains_call(*e.rhs); },
            [&](const ast::IndexExpr& e) { return expr_contains_call(*e.base) || expr_contains_call(*e.index); },
            [&](const ast::FieldExpr& e) { return expr_contains_call(*e.base); },
            [&](const ast::CastExpr& e) { return expr_contains_call(*e.expr); },
            [&](const ast::ArrayExpr& e) {
                for (const auto& elem : e.elements) {
                    if (expr_contains_call(*elem)) return true;
                }
                return false;
            },
            [&](const ast::StructExpr& e) {
                for (const auto& [_, value] : e.fields) {
                    if (expr_contains_call(*value)) return true;
                }
                return false;
            },
            [&](const ast::CallExpr&) { return true; },
            [&](const auto&) { return true; },
        }, expr.kind);
    }

    [[nodiscard]] static bool stmt_is_leaf_registerizable(
        const ast::Stmt& stmt,
        bool top_level,
        std::vector<std::string>& mutable_locals
    ) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& s) {
                if (!s.init || expr_contains_call(*s.init)) {
                    return false;
                }
                if (s.is_mut) {
                    if (!top_level) {
                        return false;
                    }
                    mutable_locals.push_back(s.name);
                }
                return true;
            },
            [&](const ast::ExprStmt& s) {
                return !expr_contains_call(*s.expr);
            },
            [&](const ast::ReturnStmt& s) {
                return !s.value || !expr_contains_call(*s.value);
            },
            [&](const ast::IfStmt& s) {
                if (expr_contains_call(*s.condition)) {
                    return false;
                }
                for (const auto& inner : s.then_block) {
                    if (!stmt_is_leaf_registerizable(*inner, false, mutable_locals)) {
                        return false;
                    }
                }
                if (s.else_block) {
                    for (const auto& inner : *s.else_block) {
                        if (!stmt_is_leaf_registerizable(*inner, false, mutable_locals)) {
                            return false;
                        }
                    }
                }
                return true;
            },
            [&](const ast::BlockStmt& s) {
                for (const auto& inner : s.block.stmts) {
                    if (!stmt_is_leaf_registerizable(*inner, false, mutable_locals)) {
                        return false;
                    }
                }
                return true;
            },
            [&](const auto&) {
                return false;
            },
        }, stmt.kind);
    }

    [[nodiscard]] static bool try_make_function_leaf_register_plan(const ast::FnDecl& fn, FunctionLeafRegisterPlan& plan) {
        if (!fn.has_body() || fn.params.size() > 4) {
            return false;
        }

        const auto& body = fn.body_ref();
        std::vector<std::string> mutable_locals;
        for (const auto& stmt : body) {
            if (!stmt_is_leaf_registerizable(*stmt, true, mutable_locals)) {
                return false;
            }
        }

        plan.param_regs.clear();
        plan.local_regs.clear();
        std::unordered_set<x64::Reg> used_regs;
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            plan.param_regs[fn.params[i].name] = x64::ARG_REGS[i];
            used_regs.insert(x64::ARG_REGS[i]);
        }

        constexpr std::array<x64::Reg, 6> local_candidates = {
            x64::Reg::R10,
            x64::Reg::R11,
            x64::Reg::R9,
            x64::Reg::R8,
            x64::Reg::RDX,
            x64::Reg::RCX,
        };

        for (const auto& name : mutable_locals) {
            if (plan.local_regs.contains(name)) {
                continue;
            }
            bool assigned = false;
            for (x64::Reg reg : local_candidates) {
                if (used_regs.contains(reg)) {
                    continue;
                }
                plan.local_regs[name] = reg;
                used_regs.insert(reg);
                assigned = true;
                break;
            }
            if (!assigned) {
                return false;
            }
        }

        return !plan.local_regs.empty() || !plan.param_regs.empty();
    }

    [[nodiscard]] bool try_make_function_saved_local_plan(const ast::FnDecl&, FunctionSavedLocalPlan& plan) {
        plan.locals.clear();
        plan.save_regs.clear();
        plan.prefix_count = 0;
        return false;
    }

    [[nodiscard]] static bool expr_has_only_local_side_effects(const ast::Expr& expr, const std::unordered_set<std::string>& locals) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) { return true; },
            [&](const ast::IdentExpr&) { return true; },
            [&](const ast::UnaryExpr& e) { return expr_has_only_local_side_effects(*e.operand, locals); },
            [&](const ast::BinaryExpr& e) {
                using Op = ast::BinaryExpr::Op;
                switch (e.op) {
                    case Op::Assign:
                    case Op::AddAssign:
                    case Op::SubAssign:
                    case Op::MulAssign:
                    case Op::DivAssign:
                    case Op::ModAssign:
                        if (!e.lhs->is<ast::IdentExpr>()) {
                            return false;
                        }
                        return locals.contains(e.lhs->as<ast::IdentExpr>().name) &&
                               expr_has_only_local_side_effects(*e.rhs, locals);
                    default:
                        return expr_has_only_local_side_effects(*e.lhs, locals) &&
                               expr_has_only_local_side_effects(*e.rhs, locals);
                }
            },
            [&](const ast::IndexExpr& e) {
                return expr_has_only_local_side_effects(*e.base, locals) &&
                       expr_has_only_local_side_effects(*e.index, locals);
            },
            [&](const ast::FieldExpr& e) { return expr_has_only_local_side_effects(*e.base, locals); },
            [&](const ast::CastExpr& e) { return expr_has_only_local_side_effects(*e.expr, locals); },
            [&](const ast::ArrayExpr& e) {
                for (const auto& elem : e.elements) {
                    if (!expr_has_only_local_side_effects(*elem, locals)) return false;
                }
                return true;
            },
            [&](const ast::StructExpr& e) {
                for (const auto& [_, value] : e.fields) {
                    if (!expr_has_only_local_side_effects(*value, locals)) return false;
                }
                return true;
            },
            [&](const ast::CallExpr&) { return false; },
            [&](const auto&) { return false; },
        }, expr.kind);
    }

    [[nodiscard]] static bool stmt_is_single_use_inline_safe_fn(const ast::Stmt& stmt, std::unordered_set<std::string>& locals) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& s) {
                if (!s.init || !expr_has_only_local_side_effects(*s.init, locals)) {
                    return false;
                }
                locals.insert(s.name);
                return true;
            },
            [&](const ast::ExprStmt& s) {
                return expr_has_only_local_side_effects(*s.expr, locals);
            },
            [&](const ast::ReturnStmt& s) {
                return !s.value || expr_has_only_local_side_effects(*s.value, locals);
            },
            [&](const ast::IfStmt& s) {
                if (!expr_has_only_local_side_effects(*s.condition, locals)) {
                    return false;
                }
                auto then_locals = locals;
                for (const auto& inner : s.then_block) {
                    if (!stmt_is_single_use_inline_safe_fn(*inner, then_locals)) {
                        return false;
                    }
                }
                if (s.else_block) {
                    auto else_locals = locals;
                    for (const auto& inner : *s.else_block) {
                        if (!stmt_is_single_use_inline_safe_fn(*inner, else_locals)) {
                            return false;
                        }
                    }
                }
                return true;
            },
            [&](const ast::BlockStmt& s) {
                auto block_locals = locals;
                for (const auto& inner : s.block.stmts) {
                    if (!stmt_is_single_use_inline_safe_fn(*inner, block_locals)) {
                        return false;
                    }
                }
                return true;
            },
            [&](const auto&) { return false; },
        }, stmt.kind);
    }

    [[nodiscard]] static bool is_single_use_inline_safe_fn(const ast::FnDecl& fn) {
        if (!fn.has_body()) {
            return false;
        }

        const auto& body = fn.body_ref();
        std::unordered_set<std::string> locals;
        for (const auto& param : fn.params) {
            locals.insert(param.name);
        }
        for (const auto& stmt : body) {
            if (!stmt_is_single_use_inline_safe_fn(*stmt, locals)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool expr_is_single_use_inline_safe(const ast::Expr& expr) const {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) { return true; },
            [&](const ast::IdentExpr&) { return true; },
            [&](const ast::UnaryExpr& e) { return expr_is_single_use_inline_safe(*e.operand); },
            [&](const ast::BinaryExpr& e) {
                using Op = ast::BinaryExpr::Op;
                switch (e.op) {
                    case Op::Assign:
                    case Op::AddAssign:
                    case Op::SubAssign:
                    case Op::MulAssign:
                    case Op::DivAssign:
                    case Op::ModAssign:
                        return false;
                    default:
                        return expr_is_single_use_inline_safe(*e.lhs) && expr_is_single_use_inline_safe(*e.rhs);
                }
            },
            [&](const ast::IndexExpr& e) {
                return expr_is_single_use_inline_safe(*e.base) && expr_is_single_use_inline_safe(*e.index);
            },
            [&](const ast::FieldExpr& e) { return expr_is_single_use_inline_safe(*e.base); },
            [&](const ast::CastExpr& e) { return expr_is_single_use_inline_safe(*e.expr); },
            [&](const ast::ArrayExpr& e) {
                for (const auto& elem : e.elements) {
                    if (!expr_is_single_use_inline_safe(*elem)) return false;
                }
                return true;
            },
            [&](const ast::StructExpr& e) {
                for (const auto& [_, value] : e.fields) {
                    if (!expr_is_single_use_inline_safe(*value)) return false;
                }
                return true;
            },
            [&](const ast::CallExpr& e) {
                if (!e.callee->is<ast::IdentExpr>()) {
                    return false;
                }
                auto it = functions_.find(e.callee->as<ast::IdentExpr>().name);
                if (it == functions_.end() || !it->second.single_use_inline_safe) {
                    return false;
                }
                for (const auto& arg : e.args) {
                    if (!expr_is_single_use_inline_safe(*arg)) return false;
                }
                return true;
            },
            [&](const auto&) { return false; },
        }, expr.kind);
    }

    [[nodiscard]] bool generate_fn(const ast::FnDecl& fn) {
        if (fn.attrs.is_extern()) {
            // External functions don't need code generation
            return true;
        }
        if (!fn.has_body()) {
            error(std::format("function '{}' is missing a body", fn.name));
            return false;
        }

        const auto& body = fn.body_ref();

        // Record function start
        auto& info = functions_[fn.name];
        info.code_offset = emit_.buffer().pos();
        info.single_use_inline_safe = is_single_use_inline_safe_fn(fn);
        current_function_return_type_ = info.return_type.clone();
        has_current_function_return_type_ = true;
        owned_locals_.clear();

        FunctionLeafRegisterPlan leaf_plan;
        bool use_leaf_plan = try_make_function_leaf_register_plan(fn, leaf_plan);
        bool has_fp_reg_params = false;
        for (std::size_t i = 0; i < fn.params.size() && i < 4; ++i) {
            auto resolved = canonicalize_type(fn.params[i].type);
            if (!resolved) return false;
            if (resolved->is_float()) {
                has_fp_reg_params = true;
                break;
            }
        }
        if (has_fp_reg_params) {
            use_leaf_plan = false;
        }
        FunctionSavedLocalPlan saved_local_plan;
        bool use_saved_local_plan = !use_leaf_plan && try_make_function_saved_local_plan(fn, saved_local_plan);

        std::size_t scope_mark = scopes_.size();
        push_scope();

        register_bindings_.clear();
        pending_named_register_bindings_.clear();
        current_function_saved_regs_.clear();
        bool pushed_function_bound_elision = false;
        if (use_leaf_plan) {
            pending_named_register_bindings_ = leaf_plan.local_regs;
            if (!leaf_plan.local_regs.empty()) {
                bound_stack_elision_names_.push_back({});
                for (const auto& [name, _] : leaf_plan.local_regs) {
                    bound_stack_elision_names_.back().insert(name);
                }
                pushed_function_bound_elision = true;
            }
        } else if (use_saved_local_plan) {
            current_function_saved_regs_ = saved_local_plan.save_regs;
        }

        std::size_t frame_patch = emit_.prologue_patchable(current_function_saved_regs_);

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
            emit_.add_smart(x64::Reg::RSP, 32);
        }

        // windows x64: first 4 args in rcx, rdx, r8, r9
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            const auto& param = fn.params[i];
            auto resolved = canonicalize_type(param.type);
            if (!resolved) return false;
            auto& sym = current_scope_->define(param.name, std::move(*resolved), param.is_mut);
            sym.is_param = true;

            if (use_leaf_plan) {
                if (auto reg_it = leaf_plan.param_regs.find(param.name); reg_it != leaf_plan.param_regs.end()) {
                    register_bindings_[&sym] = reg_it->second;
                    if (!emit_coerce_reg_to_type(reg_it->second, sym.type)) {
                        return false;
                    }
                    continue;
                }
            }
            
            // spill param register to stack slot
            if (i < 4) {
                if (sym.type.is_float()) {
                    if (!emit_capture_float_param_to_rax(i, sym.type)) {
                        return false;
                    }
                } else {
                    x64::Reg param_reg = x64::ARG_REGS[i];
                    emit_.mov(x64::Reg::RAX, param_reg);
                    if (!emit_coerce_rax_to_type(sym.type)) {
                        return false;
                    }
                }
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
            } else {
                // stack params: caller puts them at [RSP+32+(i-4)*8] before call
                // after push rbp + mov rbp,rsp thats [RBP+16+i*8]
                std::int32_t src_offset = 16 + static_cast<std::int32_t>(i) * 8;
                if (sym.type.is_float()) {
                    if (!emit_capture_stack_float_param_to_rax(src_offset, sym.type)) {
                        return false;
                    }
                } else {
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, src_offset);
                    if (!emit_coerce_rax_to_type(sym.type)) {
                        return false;
                    }
                }
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
            }
        }

        bool body_ok = true;
        if (use_saved_local_plan) {
            for (std::size_t i = 0; i < saved_local_plan.prefix_count; ++i) {
                const auto& let = body[i]->as<ast::LetStmt>();
                if (i < saved_local_plan.locals.size()) {
                    pending_named_register_bindings_[saved_local_plan.locals[i].first] = saved_local_plan.locals[i].second;
                }
                Symbol* sym = define_let_symbol(let);
                if (!sym || !emit_let_init(let, *sym)) {
                    body_ok = false;
                    break;
                }
                update_owned_local_after_store(sym, let.init.get());
                if (i < saved_local_plan.locals.size()) {
                    pending_named_register_bindings_.erase(saved_local_plan.locals[i].first);
                }
            }
            pending_named_register_bindings_.clear();
            if (body_ok) {
                body_ok = generate_stmt_list_from(body, saved_local_plan.prefix_count);
            }
        } else {
            body_ok = generate_stmt_list(body);
        }

        // Generate body
        if (!body_ok) {
            pending_named_register_bindings_.clear();
            register_bindings_.clear();
            current_function_saved_regs_.clear();
            has_current_function_return_type_ = false;
            if (pushed_function_bound_elision) {
                bound_stack_elision_names_.pop_back();
            }
            pop_scope();
            return false;
        }

        patch_scope_frame(frame_patch, scope_mark, current_function_saved_regs_.size());

        pending_named_register_bindings_.clear();
        register_bindings_.clear();
        if (pushed_function_bound_elision) {
            bound_stack_elision_names_.pop_back();
        }

        // If no explicit return, add one
        if (info.return_type.is_void()) {
            emit_current_epilogue();
        } else {
            // Default return 0
            emit_.mov_imm32(x64::Reg::RAX, 0);
            emit_current_epilogue();
        }

        info.code_size = emit_.buffer().pos() - info.code_offset;
        current_function_saved_regs_.clear();
        has_current_function_return_type_ = false;

        pop_scope();
        return true;
    }

    [[nodiscard]] std::optional<std::string> extract_ident_name(const ast::Expr& expr) const {
        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            return ident->name;
        }
        if (auto* cast = std::get_if<ast::CastExpr>(&expr.kind)) {
            return extract_ident_name(*cast->expr);
        }
        if (auto* unary = std::get_if<ast::UnaryExpr>(&expr.kind)) {
            if (unary->op == ast::UnaryExpr::Op::AddrOf ||
                unary->op == ast::UnaryExpr::Op::AddrOfMut) {
                return extract_ident_name(*unary->operand);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool expr_is_side_effect_free(const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) { return true; },
            [&](const ast::IdentExpr&) { return true; },
            [&](const ast::BinaryExpr& e) {
                using Op = ast::BinaryExpr::Op;
                switch (e.op) {
                    case Op::Add:
                    case Op::Sub:
                    case Op::Mul:
                    case Op::Div:
                    case Op::Mod:
                    case Op::BitAnd:
                    case Op::BitOr:
                    case Op::BitXor:
                    case Op::Shl:
                    case Op::Shr:
                    case Op::Eq:
                    case Op::Ne:
                    case Op::Lt:
                    case Op::Gt:
                    case Op::Le:
                    case Op::Ge:
                        return expr_is_side_effect_free(*e.lhs) && expr_is_side_effect_free(*e.rhs);
                    default:
                        return false;
                }
            },
            [&](const ast::UnaryExpr& e) {
                using Op = ast::UnaryExpr::Op;
                switch (e.op) {
                    case Op::Neg:
                    case Op::Not:
                    case Op::BitNot:
                        return expr_is_side_effect_free(*e.operand);
                    default:
                        return false;
                }
            },
            [&](const ast::CallExpr&) { return false; },
            [&](const ast::IndexExpr& e) { return expr_is_side_effect_free(*e.base) && expr_is_side_effect_free(*e.index); },
            [&](const ast::FieldExpr& e) { return expr_is_side_effect_free(*e.base); },
            [&](const ast::CastExpr& e) { return expr_is_side_effect_free(*e.expr); },
            [&](const ast::ArrayExpr& e) {
                for (const auto& elem : e.elements) {
                    if (!expr_is_side_effect_free(*elem)) return false;
                }
                return true;
            },
            [&](const ast::StructExpr&) { return false; },
            [&](const ast::IfExpr& e) {
                if (!expr_is_side_effect_free(*e.condition)) return false;
                for (const auto& stmt : e.then_block) {
                    if (!expr_is_side_effect_free(*stmt)) return false;
                }
                if (e.else_block) {
                    for (const auto& stmt : *e.else_block) {
                        if (!expr_is_side_effect_free(*stmt)) return false;
                    }
                }
                return true;
            },
            [&](const ast::BlockExpr& e) {
                for (const auto& stmt : e.block.stmts) {
                    if (!expr_is_side_effect_free(*stmt)) return false;
                }
                return !e.block.result || expr_is_side_effect_free(*e.block.result);
            },
            [&](const ast::SpawnExpr&) { return false; },
            [&](const ast::AwaitExpr&) { return false; },
            [&](const ast::AtomicLoadExpr&) { return false; },
            [&](const ast::AtomicStoreExpr&) { return false; },
            [&](const ast::AtomicAddExpr&) { return false; },
            [&](const ast::AtomicCompareExchangeExpr&) { return false; },
        }, expr.kind);
    }

    [[nodiscard]] static bool expr_is_side_effect_free(const ast::Stmt& stmt) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& s) {
                return !s.init || expr_is_side_effect_free(*s.init);
            },
            [&](const ast::ExprStmt& s) { return expr_is_side_effect_free(*s.expr); },
            [&](const ast::ReturnStmt& s) { return !s.value || expr_is_side_effect_free(*s.value); },
            [&](const auto&) { return false; },
        }, stmt.kind);
    }

    [[nodiscard]] static bool can_generate_pure_expr(const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) { return true; },
            [&](const ast::IdentExpr&) { return true; },
            [&](const ast::FieldExpr& e) { return can_generate_pure_expr(*e.base); },
            [&](const ast::UnaryExpr& e) {
                using Op = ast::UnaryExpr::Op;
                switch (e.op) {
                    case Op::Neg:
                    case Op::Not:
                    case Op::BitNot:
                        return can_generate_pure_expr(*e.operand);
                    default:
                        return false;
                }
            },
            [&](const ast::BinaryExpr& e) {
                using Op = ast::BinaryExpr::Op;
                switch (e.op) {
                    case Op::Add:
                    case Op::Sub:
                    case Op::Mul:
                    case Op::Div:
                    case Op::Mod:
                    case Op::BitAnd:
                    case Op::BitOr:
                    case Op::BitXor:
                    case Op::Shl:
                    case Op::Shr:
                    case Op::Eq:
                    case Op::Ne:
                    case Op::Lt:
                    case Op::Gt:
                    case Op::Le:
                    case Op::Ge:
                        return can_generate_pure_expr(*e.lhs) && can_generate_pure_expr(*e.rhs);
                    default:
                        return false;
                }
            },
            [&](const auto&) { return false; },
        }, expr.kind);
    }

    [[nodiscard]] static std::size_t count_ident_uses_in_expr(const std::string& name, const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) -> std::size_t { return 0; },
            [&](const ast::IdentExpr& e) -> std::size_t { return e.name == name ? 1u : 0u; },
            [&](const ast::BinaryExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.lhs) + count_ident_uses_in_expr(name, *e.rhs);
            },
            [&](const ast::UnaryExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.operand);
            },
            [&](const ast::CallExpr& e) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *e.callee);
                for (const auto& arg : e.args) total += count_ident_uses_in_expr(name, *arg);
                return total;
            },
            [&](const ast::IndexExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.base) + count_ident_uses_in_expr(name, *e.index);
            },
            [&](const ast::FieldExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.base);
            },
            [&](const ast::CastExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.expr);
            },
            [&](const ast::ArrayExpr& e) -> std::size_t {
                std::size_t total = 0;
                for (const auto& elem : e.elements) total += count_ident_uses_in_expr(name, *elem);
                return total;
            },
            [&](const ast::StructExpr& e) -> std::size_t {
                std::size_t total = 0;
                for (const auto& [_, value] : e.fields) total += count_ident_uses_in_expr(name, *value);
                return total;
            },
            [&](const ast::IfExpr& e) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *e.condition);
                for (const auto& stmt : e.then_block) total += count_ident_uses_in_stmt(name, *stmt);
                if (e.else_block) {
                    for (const auto& stmt : *e.else_block) total += count_ident_uses_in_stmt(name, *stmt);
                }
                return total;
            },
            [&](const ast::BlockExpr& e) -> std::size_t {
                std::size_t total = 0;
                for (const auto& stmt : e.block.stmts) total += count_ident_uses_in_stmt(name, *stmt);
                if (e.block.result) total += count_ident_uses_in_expr(name, *e.block.result);
                return total;
            },
            [&](const ast::SpawnExpr& e) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *e.callee);
                for (const auto& arg : e.args) total += count_ident_uses_in_expr(name, *arg);
                return total;
            },
            [&](const ast::AwaitExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.handle);
            },
            [&](const ast::AtomicLoadExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.ptr);
            },
            [&](const ast::AtomicStoreExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.ptr) +
                       count_ident_uses_in_expr(name, *e.value);
            },
            [&](const ast::AtomicAddExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.ptr) +
                       count_ident_uses_in_expr(name, *e.value);
            },
            [&](const ast::AtomicCompareExchangeExpr& e) -> std::size_t {
                return count_ident_uses_in_expr(name, *e.ptr) +
                       count_ident_uses_in_expr(name, *e.expected) +
                       count_ident_uses_in_expr(name, *e.desired);
            },
        }, expr.kind);
    }

    [[nodiscard]] static std::size_t count_ident_uses_in_stmt(const std::string& name, const ast::Stmt& stmt) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& s) -> std::size_t {
                return s.init ? count_ident_uses_in_expr(name, *s.init) : 0u;
            },
            [&](const ast::ExprStmt& s) -> std::size_t {
                return count_ident_uses_in_expr(name, *s.expr);
            },
            [&](const ast::ReturnStmt& s) -> std::size_t {
                return s.value ? count_ident_uses_in_expr(name, *s.value) : 0u;
            },
            [&](const ast::IfStmt& s) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *s.condition);
                for (const auto& inner : s.then_block) total += count_ident_uses_in_stmt(name, *inner);
                if (s.else_block) {
                    for (const auto& inner : *s.else_block) total += count_ident_uses_in_stmt(name, *inner);
                }
                return total;
            },
            [&](const ast::WhileStmt& s) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *s.condition);
                for (const auto& inner : s.body) total += count_ident_uses_in_stmt(name, *inner);
                return total;
            },
            [&](const ast::ForStmt& s) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *s.iterable);
                for (const auto& inner : s.body) total += count_ident_uses_in_stmt(name, *inner);
                return total;
            },
            [&](const ast::LoopStmt& s) -> std::size_t {
                std::size_t total = 0;
                for (const auto& inner : s.body) total += count_ident_uses_in_stmt(name, *inner);
                return total;
            },
            [&](const ast::BlockStmt& s) -> std::size_t {
                std::size_t total = 0;
                for (const auto& inner : s.block.stmts) total += count_ident_uses_in_stmt(name, *inner);
                return total;
            },
            [&](const ast::ParallelForStmt& s) -> std::size_t {
                std::size_t total = count_ident_uses_in_expr(name, *s.start) + count_ident_uses_in_expr(name, *s.end);
                for (const auto& inner : s.body) total += count_ident_uses_in_stmt(name, *inner);
                return total;
            },
            [&](const auto&) -> std::size_t { return 0; },
        }, stmt.kind);
    }

    [[nodiscard]] static std::size_t count_ident_uses_in_range(const std::string& name, const std::vector<ast::StmtPtr>& stmts, std::size_t start) {
        std::size_t total = 0;
        for (std::size_t i = start; i < stmts.size(); ++i) {
            if (stmts[i]->is<ast::LetStmt>() && stmts[i]->as<ast::LetStmt>().name == name) {
                break;
            }
            total += count_ident_uses_in_stmt(name, *stmts[i]);
        }
        return total;
    }

    [[nodiscard]] static bool expr_uses_ident_in_call_arg_context(
        const std::string& name,
        const ast::Expr& expr,
        bool in_call_arg = false
    ) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) -> bool { return false; },
            [&](const ast::IdentExpr& e) -> bool { return in_call_arg && e.name == name; },
            [&](const ast::BinaryExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.lhs, in_call_arg) ||
                       expr_uses_ident_in_call_arg_context(name, *e.rhs, in_call_arg);
            },
            [&](const ast::UnaryExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.operand, in_call_arg);
            },
            [&](const ast::CallExpr& e) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *e.callee, in_call_arg)) {
                    return true;
                }
                for (const auto& arg : e.args) {
                    if (expr_uses_ident_in_call_arg_context(name, *arg, true)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::IndexExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.base, in_call_arg) ||
                       expr_uses_ident_in_call_arg_context(name, *e.index, in_call_arg);
            },
            [&](const ast::FieldExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.base, in_call_arg);
            },
            [&](const ast::CastExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.expr, in_call_arg);
            },
            [&](const ast::ArrayExpr& e) -> bool {
                for (const auto& elem : e.elements) {
                    if (expr_uses_ident_in_call_arg_context(name, *elem, in_call_arg)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::StructExpr& e) -> bool {
                for (const auto& [_, value] : e.fields) {
                    if (expr_uses_ident_in_call_arg_context(name, *value, in_call_arg)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::IfExpr& e) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *e.condition, in_call_arg)) {
                    return true;
                }
                for (const auto& stmt : e.then_block) {
                    if (stmt_uses_ident_in_call_arg_context(name, *stmt)) {
                        return true;
                    }
                }
                if (e.else_block) {
                    for (const auto& stmt : *e.else_block) {
                        if (stmt_uses_ident_in_call_arg_context(name, *stmt)) {
                            return true;
                        }
                    }
                }
                return false;
            },
            [&](const ast::BlockExpr& e) -> bool {
                for (const auto& stmt : e.block.stmts) {
                    if (stmt_uses_ident_in_call_arg_context(name, *stmt)) {
                        return true;
                    }
                }
                return e.block.result &&
                       expr_uses_ident_in_call_arg_context(name, *e.block.result, in_call_arg);
            },
            [&](const ast::SpawnExpr& e) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *e.callee, in_call_arg)) {
                    return true;
                }
                for (const auto& arg : e.args) {
                    if (expr_uses_ident_in_call_arg_context(name, *arg, true)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::AwaitExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.handle, in_call_arg);
            },
            [&](const ast::AtomicLoadExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.ptr, in_call_arg);
            },
            [&](const ast::AtomicStoreExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.ptr, in_call_arg) ||
                       expr_uses_ident_in_call_arg_context(name, *e.value, true);
            },
            [&](const ast::AtomicAddExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.ptr, in_call_arg) ||
                       expr_uses_ident_in_call_arg_context(name, *e.value, true);
            },
            [&](const ast::AtomicCompareExchangeExpr& e) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *e.ptr, in_call_arg) ||
                       expr_uses_ident_in_call_arg_context(name, *e.expected, true) ||
                       expr_uses_ident_in_call_arg_context(name, *e.desired, true);
            },
        }, expr.kind);
    }

    [[nodiscard]] static bool stmt_uses_ident_in_call_arg_context(const std::string& name, const ast::Stmt& stmt) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& s) -> bool {
                return s.init && expr_uses_ident_in_call_arg_context(name, *s.init);
            },
            [&](const ast::ExprStmt& s) -> bool {
                return expr_uses_ident_in_call_arg_context(name, *s.expr);
            },
            [&](const ast::ReturnStmt& s) -> bool {
                return s.value && expr_uses_ident_in_call_arg_context(name, *s.value);
            },
            [&](const ast::IfStmt& s) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *s.condition)) {
                    return true;
                }
                for (const auto& inner : s.then_block) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                if (s.else_block) {
                    for (const auto& inner : *s.else_block) {
                        if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                            return true;
                        }
                    }
                }
                return false;
            },
            [&](const ast::WhileStmt& s) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *s.condition)) {
                    return true;
                }
                for (const auto& inner : s.body) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::ForStmt& s) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *s.iterable)) {
                    return true;
                }
                for (const auto& inner : s.body) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::LoopStmt& s) -> bool {
                for (const auto& inner : s.body) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::BlockStmt& s) -> bool {
                for (const auto& inner : s.block.stmts) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const ast::ParallelForStmt& s) -> bool {
                if (expr_uses_ident_in_call_arg_context(name, *s.start) ||
                    expr_uses_ident_in_call_arg_context(name, *s.end)) {
                    return true;
                }
                for (const auto& inner : s.body) {
                    if (stmt_uses_ident_in_call_arg_context(name, *inner)) {
                        return true;
                    }
                }
                return false;
            },
            [&](const auto&) -> bool { return false; },
        }, stmt.kind);
    }

    [[nodiscard]] static bool uses_ident_in_call_arg_context_in_range(
        const std::string& name,
        const std::vector<ast::StmtPtr>& stmts,
        std::size_t start
    ) {
        for (std::size_t i = start; i < stmts.size(); ++i) {
            if (stmts[i]->is<ast::LetStmt>() && stmts[i]->as<ast::LetStmt>().name == name) {
                break;
            }
            if (stmt_uses_ident_in_call_arg_context(name, *stmts[i])) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static bool uses_ident_multiple_times_in_single_stmt_in_range(
        const std::string& name,
        const std::vector<ast::StmtPtr>& stmts,
        std::size_t start
    ) {
        for (std::size_t i = start; i < stmts.size(); ++i) {
            if (stmts[i]->is<ast::LetStmt>() && stmts[i]->as<ast::LetStmt>().name == name) {
                break;
            }
            if (count_ident_uses_in_stmt(name, *stmts[i]) > 1) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static std::optional<std::size_t> find_last_ident_use_in_range(
        const std::string& name,
        const std::vector<ast::StmtPtr>& stmts,
        std::size_t start
    ) {
        std::optional<std::size_t> last_use;
        for (std::size_t i = start; i < stmts.size(); ++i) {
            if (stmts[i]->is<ast::LetStmt>() && stmts[i]->as<ast::LetStmt>().name == name) {
                break;
            }
            if (count_ident_uses_in_stmt(name, *stmts[i]) != 0) {
                last_use = i;
            }
        }
        return last_use;
    }

    [[nodiscard]] static bool stmt_can_consume_inline(const ast::Stmt& stmt) {
        return stmt.is<ast::LetStmt>() ||
               stmt.is<ast::ExprStmt>() ||
               stmt.is<ast::ReturnStmt>() ||
               stmt.is<ast::IfStmt>() ||
               stmt.is<ast::BlockStmt>();
    }

    [[nodiscard]] bool can_skip_stmt(const std::vector<ast::StmtPtr>& stmts, std::size_t index) {
        const auto& stmt = *stmts[index];
        if (!stmt.is<ast::LetStmt>()) {
            return false;
        }

        const auto& let = stmt.as<ast::LetStmt>();
        if (let.is_mut || !let.init) {
            return false;
        }
        if (count_ident_uses_in_expr(let.name, *let.init) != 0) {
            return false;
        }
        if (!expr_is_side_effect_free(*let.init)) {
            return false;
        }
        return count_ident_uses_in_range(let.name, stmts, index + 1) == 0;
    }

    [[nodiscard]] bool infer_let_type(const ast::LetStmt& let, Type& type) {
        if (let.type) {
            auto resolved = canonicalize_type(*let.type);
            if (!resolved) return false;
            type = std::move(*resolved);
            return true;
        }
        if (let.init) {
            if (auto inferred = infer_expr_type(*let.init)) {
                type = std::move(*inferred);
            } else if (let.init->is<ast::StructExpr>()) {
                const auto& struct_lit = let.init->as<ast::StructExpr>();
                type.kind = struct_lit.name;
            } else if (let.init->is<ast::ArrayExpr>()) {
                type = Type::make_primitive(PrimitiveType::Ptr);
            } else {
                type = Type::make_primitive(PrimitiveType::I64);
            }
            return true;
        }
        error("cannot infer type for variable without type or initializer");
        return false;
    }

    [[nodiscard]] bool is_bool_type(const Type& type) const {
        if (auto* p = std::get_if<PrimitiveType>(&type.kind)) {
            return *p == PrimitiveType::Bool;
        }
        return false;
    }

    [[nodiscard]] bool is_string_type(const Type& type) const {
        if (auto* p = std::get_if<PrimitiveType>(&type.kind)) {
            return *p == PrimitiveType::Str;
        }
        return false;
    }

    [[nodiscard]] bool is_pointer_like_type(const Type& type) const {
        return std::holds_alternative<PointerType>(type.kind) ||
               std::holds_alternative<FunctionType>(type.kind) ||
               std::holds_alternative<StructType>(type.kind) ||
               std::holds_alternative<std::string>(type.kind);
    }

    [[nodiscard]] std::optional<Type> lookup_struct_field_type(std::string_view struct_name, std::string_view field_name) const {
        auto it = structs_.find(std::string(struct_name));
        if (it == structs_.end()) {
            return std::nullopt;
        }
        for (const auto& [fname, ftype] : it->second.fields) {
            if (fname == field_name) {
                return ftype.clone();
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Type> lookup_field_expr_type(const ast::FieldExpr& field) {
        if (auto qualified = get_qualified_name(field)) {
            if (auto resolved_fn = resolve_function_alias_name(*qualified)) {
                return make_function_value_type(*resolved_fn);
            }
        }

        std::optional<Type> base_type;

        if (field.base->is<ast::IdentExpr>()) {
            const auto& name = field.base->as<ast::IdentExpr>().name;
            if (current_scope_) {
                if (Symbol* sym = current_scope_->lookup(name)) {
                    base_type = sym->type.clone();
                } else if (const ast::Expr* inlined = current_scope_->lookup_inline(name)) {
                    base_type = infer_expr_type(*inlined);
                }
            }
            if (!base_type) {
                if (auto it = globals_.find(name); it != globals_.end()) {
                    base_type = it->second.type.clone();
                }
            }
        } else if (field.base->is<ast::FieldExpr>()) {
            base_type = lookup_field_expr_type(field.base->as<ast::FieldExpr>());
        }

        if (!base_type) {
            return std::nullopt;
        }

        auto struct_name = get_struct_name(*base_type);
        if (!struct_name) {
            return std::nullopt;
        }

        return lookup_struct_field_type(*struct_name, field.field);
    }

    [[nodiscard]] std::optional<Type> infer_expr_type(const ast::Expr& expr) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr& e) -> std::optional<Type> {
                return e.type.clone();
            },
            [&](const ast::IdentExpr& e) -> std::optional<Type> {
                if (current_scope_) {
                    if (Symbol* sym = current_scope_->lookup(e.name)) {
                        return sym->type.clone();
                    }
                    if (const ast::Expr* inlined = current_scope_->lookup_inline(e.name)) {
                        return infer_expr_type(*inlined);
                    }
                }
                if (auto it = globals_.find(e.name); it != globals_.end()) {
                    return it->second.type.clone();
                }
                return std::nullopt;
            },
            [&](const ast::UnaryExpr& e) -> std::optional<Type> {
                if (e.op == ast::UnaryExpr::Op::Not) {
                    return Type::make_primitive(PrimitiveType::Bool);
                }
                return infer_expr_type(*e.operand);
            },
            [&](const ast::BinaryExpr& e) -> std::optional<Type> {
                using Op = ast::BinaryExpr::Op;
                switch (e.op) {
                    case Op::Eq: case Op::Ne:
                    case Op::Lt: case Op::Le:
                    case Op::Gt: case Op::Ge:
                    case Op::And: case Op::Or:
                        return Type::make_primitive(PrimitiveType::Bool);
                    case Op::Assign:
                    case Op::AddAssign:
                    case Op::SubAssign:
                    case Op::MulAssign:
                    case Op::DivAssign:
                    case Op::ModAssign:
                        return infer_expr_type(*e.lhs);
                    default:
                        break;
                }

                auto lhs = infer_expr_type(*e.lhs);
                auto rhs = infer_expr_type(*e.rhs);
                if (lhs && lhs->is_float()) return lhs;
                if (rhs && rhs->is_float()) return rhs;
                if (lhs) return lhs;
                if (rhs) return rhs;
                return std::nullopt;
            },
            [&](const ast::CallExpr& e) -> std::optional<Type> {
                if (auto callee_type = infer_expr_type(*e.callee)) {
                    if (auto* fn = std::get_if<FunctionType>(&callee_type->kind)) {
                        return fn->ret->clone();
                    }
                }
                if (e.callee->is<ast::IdentExpr>()) {
                    std::string name = resolve_function_name(e.callee->as<ast::IdentExpr>().name);
                    if (auto it = functions_.find(name); it != functions_.end()) {
                        return it->second.return_type.clone();
                    }
                } else if (e.callee->is<ast::FieldExpr>()) {
                    const auto& field = e.callee->as<ast::FieldExpr>();
                    bool base_is_local = field.base->is<ast::IdentExpr>() && current_scope_ &&
                                         current_scope_->lookup(field.base->as<ast::IdentExpr>().name);
                    if (!base_is_local) {
                        if (auto qualified = get_qualified_name(*e.callee)) {
                            std::string resolved = resolve_function_name(*qualified);
                            if (auto it = functions_.find(resolved); it != functions_.end()) {
                                return it->second.return_type.clone();
                            }
                        }
                    }
                    auto owner_type = resolve_field_type(field);
                    if (owner_type) {
                        std::string mangled = *owner_type + "_" + field.field;
                        if (auto it = functions_.find(mangled); it != functions_.end()) {
                            return it->second.return_type.clone();
                        }
                    }
                }
                return std::nullopt;
            },
            [&](const ast::IndexExpr& e) -> std::optional<Type> {
                auto base = infer_expr_type(*e.base);
                if (!base) return std::nullopt;
                if (auto* arr = std::get_if<ArrayType>(&base->kind)) {
                    return arr->element->clone();
                }
                if (auto* ptr = std::get_if<PointerType>(&base->kind)) {
                    return ptr->pointee->clone();
                }
                if (is_pointer_like_type(*base)) {
                    return Type::make_primitive(PrimitiveType::I64);
                }
                return std::nullopt;
            },
            [&](const ast::FieldExpr& e) -> std::optional<Type> {
                return lookup_field_expr_type(e);
            },
            [&](const ast::CastExpr& e) -> std::optional<Type> {
                auto resolved = canonicalize_type(e.target_type);
                if (resolved) return resolved;
                return e.target_type.clone();
            },
            [&](const ast::ArrayExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::Ptr);
            },
            [&](const ast::StructExpr& e) -> std::optional<Type> {
                Type type;
                type.kind = e.name;
                return type;
            },
            [&](const ast::IfExpr& e) -> std::optional<Type> {
                if (!e.else_block) {
                    return std::nullopt;
                }
                auto then_type = infer_stmt_suite_value_type(e.then_block);
                auto else_type = infer_stmt_suite_value_type(*e.else_block);
                if (!then_type || !else_type) {
                    return std::nullopt;
                }
                if (types_equal(*then_type, *else_type)) {
                    return then_type;
                }
                return std::nullopt;
            },
            [&](const ast::BlockExpr& e) -> std::optional<Type> {
                return infer_block_result_type(e.block);
            },
            [&](const ast::SpawnExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::I64);
            },
            [&](const ast::AwaitExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::I64);
            },
            [&](const ast::AtomicLoadExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::I64);
            },
            [&](const ast::AtomicStoreExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::I64);
            },
            [&](const ast::AtomicAddExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::I64);
            },
            [&](const ast::AtomicCompareExchangeExpr&) -> std::optional<Type> {
                return Type::make_primitive(PrimitiveType::Bool);
            },
            [&](const auto&) -> std::optional<Type> {
                return std::nullopt;
            }
        }, expr.kind);
    }

    [[nodiscard]] static bool is_function_type(const Type& type) {
        return std::holds_alternative<FunctionType>(type.kind);
    }

    void emit_truncate_or_extend_rax(const Type& target) {
        if (is_bool_type(target)) {
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
            emit_.setcc(x64::Emitter::CC_NE, x64::Reg::RAX);
            emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
            return;
        }

        auto* p = std::get_if<PrimitiveType>(&target.kind);
        if (!p) {
            return;
        }

        switch (*p) {
            case PrimitiveType::I8:
                emit_.buffer().emit8(0x48);
                emit_.buffer().emit8(0x0F);
                emit_.buffer().emit8(0xBE);
                emit_.buffer().emit8(0xC0); // movsx rax, al
                return;
            case PrimitiveType::I16:
                emit_.buffer().emit8(0x48);
                emit_.buffer().emit8(0x0F);
                emit_.buffer().emit8(0xBF);
                emit_.buffer().emit8(0xC0); // movsx rax, ax
                return;
            case PrimitiveType::I32:
                emit_.buffer().emit8(0x48);
                emit_.buffer().emit8(0x63);
                emit_.buffer().emit8(0xC0); // movsxd rax, eax
                return;
            case PrimitiveType::U8:
                emit_.and_imm(x64::Reg::RAX, 0xFF);
                return;
            case PrimitiveType::U16:
                emit_.and_imm(x64::Reg::RAX, 0xFFFF);
                return;
            case PrimitiveType::U32:
                emit_.buffer().emit8(0x89); emit_.buffer().emit8(0xC0); // mov eax, eax
                return;
            default:
                return;
        }
    }

    [[nodiscard]] bool emit_convert_rax_f64_bits_to_i64() {
        emit_.sub_imm(x64::Reg::RSP, 8);
        emit_.mov_store(x64::Reg::RSP, 0, x64::Reg::RAX);
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x10); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24); // movsd xmm0, [rsp]
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x2C); emit_.buffer().emit8(0xC0); // cvttsd2si rax, xmm0
        emit_.add_smart(x64::Reg::RSP, 8);
        return true;
    }

    [[nodiscard]] bool emit_convert_rax_i64_to_f64_bits() {
        emit_.sub_imm(x64::Reg::RSP, 8);
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x2A); emit_.buffer().emit8(0xC0); // cvtsi2sd xmm0, rax
        emit_.buffer().emit8(0xF2); emit_.buffer().emit8(0x0F); emit_.buffer().emit8(0x11); emit_.buffer().emit8(0x04); emit_.buffer().emit8(0x24); // movsd [rsp], xmm0
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 0);
        emit_.add_smart(x64::Reg::RSP, 8);
        return true;
    }

    [[nodiscard]] bool emit_coerce_rax_to_type(const Type& target, const ast::Expr* source_expr = nullptr) {
        auto source_type = source_expr ? infer_expr_type(*source_expr) : std::nullopt;

        if (target.is_float()) {
            if (!source_type || !source_type->is_float()) {
                return emit_convert_rax_i64_to_f64_bits();
            }
            return true;
        }

        if (target.is_integer() || is_bool_type(target) || is_pointer_like_type(target)) {
            if (source_type && source_type->is_float()) {
                if (!emit_convert_rax_f64_bits_to_i64()) {
                    return false;
                }
            }
            emit_truncate_or_extend_rax(target);
            return true;
        }

        return true;
    }

    [[nodiscard]] bool emit_array_new_count(std::size_t count) {
        if (count > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
            error("array literal is too large");
            return false;
        }

        if (is_native_image_output()) {
            emit_.mov_imm32(x64::Reg::RAX, static_cast<std::int32_t>(count));
            emit_.push(x64::Reg::RAX);  // [stack: count]

            emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
            emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03);
            emit_.add_smart(x64::Reg::RAX, 16);
            emit_.push(x64::Reg::RAX);  // [stack: count, total_size]

            emit_.sub_imm(x64::Reg::RSP, 32);
            emit_iat_call_raw(pe::iat::GetProcessHeap);
            emit_.add_smart(x64::Reg::RSP, 32);

            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
            emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
            emit_.pop(x64::Reg::R8);

            emit_.sub_imm(x64::Reg::RSP, 32);
            emit_iat_call_raw(pe::iat::HeapAlloc);
            emit_.add_smart(x64::Reg::RSP, 32);

            emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
            emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);

            emit_.pop(x64::Reg::RCX);
            emit_.mov_store(x64::Reg::RAX, 8, x64::Reg::RCX);

            emit_.add_smart(x64::Reg::RAX, 16);
            return true;
        }

        if (!rt_.array_new) {
            error("array literal requires array_new support");
            return false;
        }

        emit_.mov_imm32(x64::Reg::RCX, static_cast<std::int32_t>(count));
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.array_new));
        emit_.call(x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool emit_array_store_imm(x64::Reg arr_reg, std::size_t index, x64::Reg value_reg) {
        if (index > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
            error("array literal index is too large");
            return false;
        }

        if (is_native_image_output()) {
            emit_.mov(x64::Reg::R11, arr_reg);
            std::int64_t byte_offset = static_cast<std::int64_t>(index) * 8;
            emit_.add_smart(x64::Reg::R11, byte_offset);
            emit_.mov_store(x64::Reg::R11, 0, value_reg);
            return true;
        }

        if (!rt_.array_set) {
            error("array literal requires array_set support");
            return false;
        }

        emit_.mov(x64::Reg::RCX, arr_reg);
        emit_.mov_imm32(x64::Reg::RDX, static_cast<std::int32_t>(index));
        emit_.mov(x64::Reg::R8, value_reg);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.array_set));
        emit_.call(x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool emit_coerce_reg_to_type(x64::Reg reg, const Type& target, const ast::Expr* source_expr = nullptr) {
        auto source_type = source_expr ? infer_expr_type(*source_expr) : std::nullopt;
        if (reg != x64::Reg::RAX && (!source_type || !source_type->is_float())) {
            if (is_pointer_like_type(target)) {
                return true;
            }
            if (auto* prim = std::get_if<PrimitiveType>(&target.kind)) {
                if (*prim == PrimitiveType::I64 || *prim == PrimitiveType::U64) {
                    return true;
                }
            }
        }

        if (reg != x64::Reg::RAX) {
            emit_.mov(x64::Reg::RAX, reg);
        }
        if (!emit_coerce_rax_to_type(target, source_expr)) {
            return false;
        }
        if (reg != x64::Reg::RAX) {
            emit_.mov(reg, x64::Reg::RAX);
        }
        return true;
    }

    [[nodiscard]] Symbol* define_let_symbol(const ast::LetStmt& let) {
        Type type;
        if (!infer_let_type(let, type)) {
            return nullptr;
        }
        Symbol& sym = current_scope_->define(let.name, std::move(type), let.is_mut);
        if (auto it = pending_named_register_bindings_.find(let.name); it != pending_named_register_bindings_.end()) {
            register_bindings_[&sym] = it->second;
        }
        return &sym;
    }

    [[nodiscard]] bool emit_let_init(const ast::LetStmt& let, const Symbol& sym) {
        if (!let.init) {
            return true;
        }
        if (auto bound = lookup_bound_reg(&sym)) {
            if (!generate_expr(*let.init)) {
                return false;
            }
            if (!emit_coerce_rax_to_type(sym.type, let.init.get())) {
                return false;
            }
            if (*bound != x64::Reg::RAX) {
                emit_.mov(*bound, x64::Reg::RAX);
            }
            bool just_bound_immutable_pure_let = !sym.is_mut && pending_named_register_bindings_.contains(sym.name);
            if (!should_elide_bound_stack_slot(&sym) && !just_bound_immutable_pure_let) {
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, *bound);
            }
            return true;
        }
        if (!generate_expr(*let.init)) {
            return false;
        }
        if (!emit_coerce_rax_to_type(sym.type, let.init.get())) {
            return false;
        }
        emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool generate_prefix_let_while_return_fastpath(const std::vector<ast::StmtPtr>& stmts, std::size_t start_index, std::size_t& consumed) {
        if (!current_scope_ || !stmts[start_index]->is<ast::LetStmt>()) {
            return false;
        }

        std::size_t while_index = start_index;
        while (while_index < stmts.size() && stmts[while_index]->is<ast::LetStmt>()) {
            const auto& let = stmts[while_index]->as<ast::LetStmt>();
            if (!let.is_mut || !let.init) {
                return false;
            }
            while_index++;
        }

        if (while_index == start_index || while_index >= stmts.size() || !stmts[while_index]->is<ast::WhileStmt>()) {
            return false;
        }

        std::size_t return_index = while_index + 1;
        while (return_index < stmts.size() && can_skip_stmt(stmts, return_index)) {
            return_index++;
        }
        if (return_index >= stmts.size() || !stmts[return_index]->is<ast::ReturnStmt>()) {
            return false;
        }

        std::size_t later_live = return_index + 1;
        while (later_live < stmts.size() && can_skip_stmt(stmts, later_live)) {
            later_live++;
        }
        if (later_live != stmts.size()) {
            return false;
        }

        std::vector<std::pair<const ast::LetStmt*, Symbol*>> lets;
        lets.reserve(while_index - start_index);
        for (std::size_t idx = start_index; idx < while_index; ++idx) {
            const auto& let = stmts[idx]->as<ast::LetStmt>();
            Symbol* sym = define_let_symbol(let);
            if (!sym) {
                return false;
            }
            lets.push_back({&let, sym});
        }

        const auto& while_stmt = stmts[while_index]->as<ast::WhileStmt>();
        const auto& ret_stmt = stmts[return_index]->as<ast::ReturnStmt>();

        {
            WhileAlternatingBranchReductionPlan alternating_plan;
            std::unordered_map<const Symbol*, const ast::Expr*> init_map;
            bool all_bound = true;
            for (const auto& [let, sym] : lets) {
                if (!can_generate_pure_expr(*let->init)) {
                    all_bound = false;
                    break;
                }
                init_map[sym] = let->init.get();
            }
            if (all_bound && try_make_while_alternating_branch_reduction_plan(while_stmt, ret_stmt, alternating_plan) &&
                generate_while_alternating_branch_reduced_return(alternating_plan, &init_map)) {
                consumed = return_index;
                return true;
            }
        }

        WhileMultiStatePlan plan;
        if (try_make_while_multistate_plan(while_stmt, plan)) {
            WhileMultiStateReductionPlan reduced_plan;
            if (try_make_while_multistate_reduction_plan(plan, ret_stmt, reduced_plan)) {
                std::unordered_map<const Symbol*, const ast::Expr*> init_map;
                std::unordered_set<const Symbol*> needed_symbols = {
                    reduced_plan.counter_sym,
                    reduced_plan.accum_sym,
                };
                for (Symbol* sym : reduced_plan.contributor_syms) {
                    needed_symbols.insert(sym);
                }

                bool all_bound = lets.size() == needed_symbols.size();
                if (all_bound) {
                    for (const auto& [let, sym] : lets) {
                        if (!needed_symbols.contains(sym) || !can_generate_pure_expr(*let->init)) {
                            all_bound = false;
                            break;
                        }
                        init_map[sym] = let->init.get();
                    }
                }

                if (all_bound && generate_while_multistate_reduced_return(reduced_plan, &init_map)) {
                    consumed = return_index;
                    return true;
                }
            }

            std::unordered_map<const Symbol*, const ast::Expr*> init_map;
            std::unordered_set<const Symbol*> bound_symbols;
            for (const auto& [sym, _] : plan.bindings) {
                bound_symbols.insert(sym);
            }

            bool all_bound = lets.size() == plan.bindings.size();
            if (all_bound) {
                for (const auto& [let, sym] : lets) {
                    if (!bound_symbols.contains(sym) || !can_generate_pure_expr(*let->init)) {
                        all_bound = false;
                        break;
                    }
                    init_map[sym] = let->init.get();
                }
            }

            if (all_bound && generate_while_multistate_registerized_return(plan, ret_stmt, &init_map)) {
                consumed = return_index;
                return true;
            }
        }

        WhileBoundPlan bound_plan;
        if (try_make_while_bound_plan(while_stmt, bound_plan)) {
            std::unordered_map<const Symbol*, const ast::Expr*> init_map;
            std::unordered_set<const Symbol*> bound_symbols;
            for (const auto& [sym, _] : bound_plan.bindings) {
                bound_symbols.insert(sym);
            }

            bool all_bound = lets.size() == bound_plan.bindings.size();
            if (all_bound) {
                for (const auto& [let, sym] : lets) {
                    if (!bound_symbols.contains(sym) || !can_generate_pure_expr(*let->init)) {
                        all_bound = false;
                        break;
                    }
                    init_map[sym] = let->init.get();
                }
            }

            if (all_bound && generate_while_bound_registerized_return(while_stmt, bound_plan, ret_stmt, &init_map)) {
                consumed = return_index;
                return true;
            }
        }

        WhileRegisterPlan single_plan;
        if (try_make_while_register_plan(while_stmt, single_plan)) {
            std::unordered_map<const Symbol*, const ast::Expr*> init_map;
            std::unordered_set<const Symbol*> bound_symbols;
            for (const auto& [sym, _] : single_plan.bindings) {
                bound_symbols.insert(sym);
            }

            bool all_bound = lets.size() == single_plan.bindings.size();
            if (all_bound) {
                for (const auto& [let, sym] : lets) {
                    if (!bound_symbols.contains(sym) || !can_generate_pure_expr(*let->init)) {
                        all_bound = false;
                        break;
                    }
                    init_map[sym] = let->init.get();
                }
            }

            if (all_bound && generate_while_registerized_return(while_stmt, single_plan, ret_stmt, &init_map)) {
                consumed = return_index;
                return true;
            }
        }

        for (const auto& [let, sym] : lets) {
            if (!emit_let_init(*let, *sym)) {
                return false;
            }
            update_owned_local_after_store(sym, let->init.get());
        }
        for (std::size_t idx = while_index; idx <= return_index; ++idx) {
            if (!generate_stmt(*stmts[idx])) {
                return false;
            }
        }

        consumed = return_index;
        return true;
    }

    [[nodiscard]] bool generate_stmt_list_from(const std::vector<ast::StmtPtr>& stmts, std::size_t start_index = 0) {
        if (!current_scope_) {
            for (std::size_t i = start_index; i < stmts.size(); ++i) {
                if (!generate_stmt(*stmts[i])) return false;
            }
            return true;
        }

        std::unordered_map<std::size_t, std::vector<std::string>> expirations;
        std::unordered_map<std::size_t, std::vector<const Symbol*>> reg_expirations;
        std::unordered_map<std::size_t, std::vector<std::string>> inline_call_reg_expirations;
        std::unordered_set<std::string> active_inline_safe_call_reg_lets;
        for (std::size_t i = start_index; i < stmts.size(); ++i) {
            if (can_skip_stmt(stmts, i)) {
                continue;
            }

            std::size_t consumed = i;
            if (stmts[i]->is<ast::LetStmt>() && generate_prefix_let_while_return_fastpath(stmts, i, consumed)) {
                i = consumed;
                continue;
            }

            if (stmts[i]->is<ast::WhileStmt>()) {
                std::size_t next_live = i + 1;
                while (next_live < stmts.size() && can_skip_stmt(stmts, next_live)) {
                    next_live++;
                }
                if (next_live < stmts.size() && stmts[next_live]->is<ast::ReturnStmt>()) {
                    std::size_t later_live = next_live + 1;
                    while (later_live < stmts.size() && can_skip_stmt(stmts, later_live)) {
                        later_live++;
                    }
                    if (later_live == stmts.size()) {
                        WhileAlternatingBranchReductionPlan alternating_plan;
                        if (try_make_while_alternating_branch_reduction_plan(stmts[i]->as<ast::WhileStmt>(), stmts[next_live]->as<ast::ReturnStmt>(), alternating_plan) &&
                            generate_while_alternating_branch_reduced_return(alternating_plan)) {
                            i = next_live;
                            continue;
                        }

                        WhileMultiStatePlan multi_plan;
                        if (try_make_while_multistate_plan(stmts[i]->as<ast::WhileStmt>(), multi_plan)) {
                            WhileMultiStateReductionPlan reduced_plan;
                            if (try_make_while_multistate_reduction_plan(multi_plan, stmts[next_live]->as<ast::ReturnStmt>(), reduced_plan) &&
                                generate_while_multistate_reduced_return(reduced_plan)) {
                                i = next_live;
                                continue;
                            }
                            if (generate_while_multistate_registerized_return(multi_plan, stmts[next_live]->as<ast::ReturnStmt>())) {
                                i = next_live;
                                continue;
                            }
                        }

                        WhileBoundPlan bound_plan;
                        if (try_make_while_bound_plan(stmts[i]->as<ast::WhileStmt>(), bound_plan) &&
                            generate_while_bound_registerized_return(stmts[i]->as<ast::WhileStmt>(), bound_plan, stmts[next_live]->as<ast::ReturnStmt>())) {
                            i = next_live;
                            continue;
                        }

                        WhileRegisterPlan plan;
                        if (try_make_while_register_plan(stmts[i]->as<ast::WhileStmt>(), plan) &&
                            generate_while_registerized_return(stmts[i]->as<ast::WhileStmt>(), plan, stmts[next_live]->as<ast::ReturnStmt>())) {
                            i = next_live;
                            continue;
                        }
                    }
                }
            }

            if (stmts[i]->is<ast::LetStmt>()) {
                const auto& let = stmts[i]->as<ast::LetStmt>();
                if (!let.is_mut && let.init && count_ident_uses_in_expr(let.name, *let.init) == 0 && expr_is_side_effect_free(*let.init) && can_generate_pure_expr(*let.init)) {
                    std::size_t next_live = i + 1;
                    while (next_live < stmts.size() && can_skip_stmt(stmts, next_live)) {
                        next_live++;
                    }
                    if (next_live < stmts.size() && stmt_can_consume_inline(*stmts[next_live])) {
                        std::size_t next_uses = count_ident_uses_in_stmt(let.name, *stmts[next_live]);
                        std::size_t later_uses = count_ident_uses_in_range(let.name, stmts, next_live + 1);
                        bool allow_multi_inline = next_uses > 1 && next_uses <= 3 && expr_is_single_use_inline_safe(*let.init);
                        if ((next_uses == 1 || allow_multi_inline) && later_uses == 0) {
                            current_scope_->push_inline(let.name, let.init.get());
                            expirations[next_live].push_back(let.name);
                            continue;
                        }
                    }
                }
            }

            if (stmts[i]->is<ast::LetStmt>()) {
                const auto& let = stmts[i]->as<ast::LetStmt>();
                bool inline_safe_call_init =
                    !let.is_mut && let.init && is_inline_safe_direct_call_expr(*let.init);
                bool later_call_arg_uses =
                    inline_safe_call_init && uses_ident_in_call_arg_context_in_range(let.name, stmts, i + 1);
                bool later_multi_use_stmt =
                    inline_safe_call_init && uses_ident_multiple_times_in_single_stmt_in_range(let.name, stmts, i + 1);
                bool registerizable_immutable_init =
                    !let.is_mut && let.init && can_registerize_immutable_let_init(*let.init);
                if (registerizable_immutable_init &&
                    !later_call_arg_uses &&
                    !later_multi_use_stmt &&
                    !(inline_safe_call_init && !active_inline_safe_call_reg_lets.empty())) {
                    std::size_t remaining_uses = count_ident_uses_in_range(let.name, stmts, i + 1);
                    if (remaining_uses >= 2) {
                        constexpr std::size_t kReservedScratchRegs = 2;
                        if (available_scratch_reg_count() > kReservedScratchRegs) {
                            auto reg = pick_scratch_reg();
                            if (!reg) {
                                continue;
                            }
                            pending_named_register_bindings_[let.name] = *reg;
                            if (!generate_stmt(*stmts[i])) {
                                pending_named_register_bindings_.erase(let.name);
                                return false;
                            }
                            pending_named_register_bindings_.erase(let.name);
                            if (Symbol* sym = current_scope_->lookup(let.name)) {
                                if (auto last_use = find_last_ident_use_in_range(let.name, stmts, i + 1)) {
                                    reg_expirations[*last_use].push_back(sym);
                                    if (inline_safe_call_init) {
                                        active_inline_safe_call_reg_lets.insert(let.name);
                                        inline_call_reg_expirations[*last_use].push_back(let.name);
                                    }
                                }
                            }
                            if (auto it = expirations.find(i); it != expirations.end()) {
                                for (const auto& name : it->second) {
                                    current_scope_->pop_inline(name);
                                }
                            }
                            if (auto it = reg_expirations.find(i); it != reg_expirations.end()) {
                                for (const Symbol* sym : it->second) {
                                    register_bindings_.erase(sym);
                                }
                            }
                            if (auto it = inline_call_reg_expirations.find(i); it != inline_call_reg_expirations.end()) {
                                for (const auto& name : it->second) {
                                    active_inline_safe_call_reg_lets.erase(name);
                                }
                            }
                            continue;
                        }
                    }
                }
            }

            if (!generate_stmt(*stmts[i])) return false;

            if (auto it = expirations.find(i); it != expirations.end()) {
                for (const auto& name : it->second) {
                    current_scope_->pop_inline(name);
                }
            }
            if (auto it = reg_expirations.find(i); it != reg_expirations.end()) {
                for (const Symbol* sym : it->second) {
                    register_bindings_.erase(sym);
                }
            }
            if (auto it = inline_call_reg_expirations.find(i); it != inline_call_reg_expirations.end()) {
                for (const auto& name : it->second) {
                    active_inline_safe_call_reg_lets.erase(name);
                }
            }
        }
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
                if (!generate_stmt_list(block.block.stmts)) {
                    pop_scope();
                    return false;
                }
                pop_scope();
                return true;
            },
        }, stmt.kind);
    }

    [[nodiscard]] bool generate_let(const ast::LetStmt& let) {
        Symbol* sym = define_let_symbol(let);
        if (!sym) {
            return false;
        }
        if (!emit_let_init(let, *sym)) {
            return false;
        }
        update_owned_local_after_store(sym, let.init.get());
        return true;
    }

    [[nodiscard]] bool generate_return(const ast::ReturnStmt& ret) {
        if (ret.value) {
            if (!generate_expr(*ret.value)) return false;
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        if (has_current_function_return_type_ && current_function_return_type_.is_float()) {
            emit_return_float_rax_to_xmm0(current_function_return_type_);
        }
        emit_current_epilogue();
        return true;
    }

    [[nodiscard]] bool generate_while_multistate_reduced_return(
        const WhileMultiStateReductionPlan& plan,
        const std::unordered_map<const Symbol*, const ast::Expr*>* init_map = nullptr
    ) {
        auto emit_closed_form_counted_accumulation = [&](x64::Reg term_reg, std::int64_t term_step) -> bool {
            emit_.mov(x64::Reg::R8, x64::Reg::RCX);
            emit_.mov_smart(x64::Reg::RCX, plan.limit);
            emit_.sub(x64::Reg::RCX, x64::Reg::R8);
            emit_.cmp_smart_imm(x64::Reg::RCX, 0);
            std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_LE);

            emit_.mov(x64::Reg::R9, x64::Reg::RCX);
            emit_.imul(term_reg, x64::Reg::RCX);

            if (term_step != 0) {
                emit_.mov(x64::Reg::R10, x64::Reg::R9);
                emit_.dec(x64::Reg::R10);
                emit_.imul(x64::Reg::R10, x64::Reg::R9);
                emit_.sar_imm(x64::Reg::R10, 1);
                emit_.imul_smart(x64::Reg::R10, x64::Reg::R10, term_step);
                emit_.add(term_reg, x64::Reg::R10);
            }

            emit_.add(x64::Reg::RAX, term_reg);
            emit_.patch_jump(exit_patch);
            return true;
        };

        auto init_symbol_reg = [&](Symbol* sym, x64::Reg reg) -> bool {
            if (init_map) {
                if (auto it = init_map->find(sym); it != init_map->end()) {
                    return generate_pure_expr_into(*it->second, reg);
                }
            }
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
            return true;
        };

        if (!init_symbol_reg(plan.counter_sym, x64::Reg::RCX)) {
            return false;
        }
        if (!init_symbol_reg(plan.accum_sym, x64::Reg::RAX)) {
            return false;
        }
        if (plan.contributor_syms.empty()) {
            return false;
        }

        if (!init_symbol_reg(plan.contributor_syms.front(), x64::Reg::RDX)) {
            return false;
        }
        for (std::size_t i = 1; i < plan.contributor_syms.size(); ++i) {
            auto scratch = pick_scratch_reg({x64::Reg::RAX, x64::Reg::RCX, x64::Reg::RDX});
            if (!scratch) {
                return false;
            }
            if (!init_symbol_reg(plan.contributor_syms[i], *scratch)) {
                return false;
            }
            emit_.add(x64::Reg::RDX, *scratch);
        }

        if (!emit_closed_form_counted_accumulation(x64::Reg::RDX, plan.term_step)) {
            return false;
        }
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] bool generate_while_alternating_branch_reduced_return(
        const WhileAlternatingBranchReductionPlan& plan,
        const std::unordered_map<const Symbol*, const ast::Expr*>* init_map = nullptr
    ) {
        auto init_symbol_reg = [&](Symbol* sym, x64::Reg reg) -> bool {
            if (init_map) {
                if (auto it = init_map->find(sym); it != init_map->end()) {
                    return generate_pure_expr_into(*it->second, reg);
                }
            }
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
            return true;
        };

        if (!init_symbol_reg(plan.accum_sym, x64::Reg::RAX) ||
            !init_symbol_reg(plan.counter_sym, x64::Reg::RDX) ||
            !init_symbol_reg(plan.even_sym, x64::Reg::R8) ||
            !init_symbol_reg(plan.odd_sym, x64::Reg::R9)) {
            return false;
        }

        emit_.cmp_smart_imm(x64::Reg::RDX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        emit_.mov_smart(x64::Reg::RCX, plan.limit);
        emit_.sub(x64::Reg::RCX, x64::Reg::RDX);
        emit_.mov(x64::Reg::R10, x64::Reg::RDX);
        emit_.and_imm(x64::Reg::R10, 1);
        emit_.cmp_smart_imm(x64::Reg::R10, 0);
        std::size_t odd_start_patch = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.mov(x64::Reg::RDX, x64::Reg::RCX);
        emit_.inc(x64::Reg::RDX);
        emit_.sar_imm(x64::Reg::RDX, 1);
        emit_.mov(x64::Reg::R11, x64::Reg::RCX);
        emit_.sar_imm(x64::Reg::R11, 1);
        std::size_t after_count_patch = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(odd_start_patch);
        emit_.mov(x64::Reg::RDX, x64::Reg::RCX);
        emit_.sar_imm(x64::Reg::RDX, 1);
        emit_.mov(x64::Reg::R11, x64::Reg::RCX);
        emit_.inc(x64::Reg::R11);
        emit_.sar_imm(x64::Reg::R11, 1);
        emit_.patch_jump(after_count_patch);

        auto emit_series_sum = [&](x64::Reg count_reg, x64::Reg start_reg, std::int64_t step) {
            emit_.cmp_smart_imm(count_reg, 0);
            std::size_t skip_patch = emit_.jcc_rel32(x64::Emitter::CC_LE);

            emit_.mov(x64::Reg::RCX, count_reg);
            emit_.imul(x64::Reg::RCX, start_reg);
            if (step != 0) {
                emit_.mov(x64::Reg::R10, count_reg);
                emit_.dec(x64::Reg::R10);
                emit_.imul(x64::Reg::R10, count_reg);
                emit_.sar_imm(x64::Reg::R10, 1);
                emit_.imul_smart(x64::Reg::R10, x64::Reg::R10, step);
                emit_.add(x64::Reg::RCX, x64::Reg::R10);
            }
            emit_.add(x64::Reg::RAX, x64::Reg::RCX);
            emit_.patch_jump(skip_patch);
        };

        emit_series_sum(x64::Reg::RDX, x64::Reg::R8, plan.even_step);
        emit_series_sum(x64::Reg::R11, x64::Reg::R9, plan.odd_step);
        emit_.patch_jump(exit_patch);
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] bool generate_expr_stmt(const ast::ExprStmt& stmt) {
        if (generate_self_update_expr_stmt(*stmt.expr)) {
            return true;
        }
        if (generate_field_self_update_expr_stmt(*stmt.expr)) {
            return true;
        }
        if (auto* assign = std::get_if<ast::BinaryExpr>(&stmt.expr->kind)) {
            if (auto overwrite = describe_owned_local_overwrite(*assign)) {
                error(*overwrite);
                return false;
            }
        }
        if (auto discarded = describe_discarded_owned_result(*stmt.expr)) {
            error(*discarded);
            return false;
        }
        if (!generate_expr(*stmt.expr)) {
            return false;
        }
        if (auto released = released_local_from_expr_stmt(*stmt.expr)) {
            owned_locals_.erase(*released);
        }
        return true;
    }

    [[nodiscard]] static bool is_str_type(const Type& type) {
        if (auto* prim = std::get_if<PrimitiveType>(&type.kind)) {
            return *prim == PrimitiveType::Str;
        }
        return false;
    }

    [[nodiscard]] std::optional<std::string> resolve_call_target_name(const ast::CallExpr& call) {
        if (call.callee->is<ast::IdentExpr>()) {
            return resolve_function_name(call.callee->as<ast::IdentExpr>().name);
        }
        if (auto qualified = get_qualified_name(*call.callee)) {
            return resolve_function_name(*qualified);
        }
        if (call.callee->is<ast::FieldExpr>()) {
            const auto& field = call.callee->as<ast::FieldExpr>();
            if (auto owner_type = resolve_field_type(field)) {
                std::string mangled = *owner_type + "_" + field.field;
                if (functions_.contains(mangled)) {
                    return mangled;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> resolve_spawn_target_name(const ast::SpawnExpr& spawn) {
        if (spawn.callee->is<ast::IdentExpr>()) {
            return resolve_function_name(spawn.callee->as<ast::IdentExpr>().name);
        }
        if (auto qualified = get_qualified_name(*spawn.callee)) {
            return resolve_function_name(*qualified);
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool is_obviously_heap_owning_builtin(std::string_view name) {
        static constexpr std::array<std::string_view, 12> kOwningBuiltins = {
            "malloc",
            "array_new",
            "new_array",
            "buffer_new",
            "read_file",
            "virtual_alloc",
            "make_string",
            "string_append",
            "concat",
            "int_to_string",
            "itoa",
            "string_substring",
        };
        return std::find(kOwningBuiltins.begin(), kOwningBuiltins.end(), name) != kOwningBuiltins.end() ||
               name == "substr";
    }

    [[nodiscard]] static bool is_obvious_owner_release_builtin(std::string_view name) {
        static constexpr std::array<std::string_view, 3> kReleaseBuiltins = {
            "free",
            "array_free",
            "virtual_free",
        };
        return std::find(kReleaseBuiltins.begin(), kReleaseBuiltins.end(), name) != kReleaseBuiltins.end();
    }

    [[nodiscard]] bool expr_obviously_produces_owned_value(const ast::Expr& expr) {
        if (auto* cast = std::get_if<ast::CastExpr>(&expr.kind)) {
            return expr_obviously_produces_owned_value(*cast->expr);
        }

        if (std::holds_alternative<ast::ArrayExpr>(expr.kind)) {
            return true;
        }

        auto* call = std::get_if<ast::CallExpr>(&expr.kind);
        if (!call) {
            return false;
        }

        auto target_name = resolve_call_target_name(*call);
        if (!target_name) {
            return false;
        }

        if (is_obviously_heap_owning_builtin(*target_name)) {
            return true;
        }

        auto inferred = infer_expr_type(expr);
        return inferred && is_str_type(*inferred);
    }

    [[nodiscard]] std::optional<const Symbol*> released_local_from_expr_stmt(const ast::Expr& expr) {
        auto* call = std::get_if<ast::CallExpr>(&expr.kind);
        if (!call || call->args.empty() || !current_scope_) {
            return std::nullopt;
        }

        auto target_name = resolve_call_target_name(*call);
        if (!target_name || !is_obvious_owner_release_builtin(*target_name)) {
            return std::nullopt;
        }

        auto* ident = std::get_if<ast::IdentExpr>(&call->args[0]->kind);
        if (!ident) {
            return std::nullopt;
        }

        return current_scope_->lookup(ident->name);
    }

    void update_owned_local_after_store(const Symbol* sym, const ast::Expr* value_expr) {
        if (!sym) {
            return;
        }
        if (!value_expr) {
            owned_locals_.erase(sym);
            return;
        }
        if (expr_obviously_produces_owned_value(*value_expr)) {
            owned_locals_[sym] = OwnedLocalState::Live;
            return;
        }
        owned_locals_.erase(sym);
    }

    [[nodiscard]] std::optional<std::string> describe_owned_local_overwrite(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;

        if (bin.op != Op::Assign || !current_scope_ || !bin.lhs->is<ast::IdentExpr>()) {
            return std::nullopt;
        }

        const auto& ident = bin.lhs->as<ast::IdentExpr>();
        Symbol* sym = current_scope_->lookup(ident.name);
        if (!sym || !sym->is_mut) {
            return std::nullopt;
        }

        auto it = owned_locals_.find(sym);
        if (it == owned_locals_.end() || it->second != OwnedLocalState::Live) {
            return std::nullopt;
        }

        if (bin.rhs->is<ast::IdentExpr>() && bin.rhs->as<ast::IdentExpr>().name == ident.name) {
            return std::nullopt;
        }

        return std::format(
            "assignment to '{}' overwrites a live owned value; free it, return it, or move it before rebinding",
            ident.name);
    }

    [[nodiscard]] std::optional<std::string> describe_discarded_owned_result(const ast::Expr& expr) {
        if (auto* cast = std::get_if<ast::CastExpr>(&expr.kind)) {
            return describe_discarded_owned_result(*cast->expr);
        }

        auto* call = std::get_if<ast::CallExpr>(&expr.kind);
        if (!call) {
            return std::nullopt;
        }

        auto target_name = resolve_call_target_name(*call);
        if (auto inferred = infer_expr_type(expr); inferred && is_str_type(*inferred)) {
            if (target_name) {
                return std::format(
                    "discarded result from '{}' returns 'str'; keep it, free it, or return it",
                    *target_name);
            }
            return "discarded result returns 'str'; keep it, free it, or return it";
        }

        if (target_name && is_obviously_heap_owning_builtin(*target_name)) {
            return std::format(
                "discarded result from '{}' allocates owned memory; keep it and free it later",
                *target_name);
        }

        return std::nullopt;
    }

    [[nodiscard]] bool analyze_self_update_expr(const ast::Expr& expr, SelfUpdateInfo& info) {
        using Op = ast::BinaryExpr::Op;

        auto* assign = std::get_if<ast::BinaryExpr>(&expr.kind);
        if (!assign || assign->op != Op::Assign) {
            return false;
        }
        auto* lhs_ident = std::get_if<ast::IdentExpr>(&assign->lhs->kind);
        if (!lhs_ident || !current_scope_) {
            return false;
        }

        Symbol* sym = current_scope_->lookup(lhs_ident->name);
        if (!sym || !sym->is_mut) {
            return false;
        }

        auto* rhs_bin = std::get_if<ast::BinaryExpr>(&assign->rhs->kind);
        if (!rhs_bin) {
            return false;
        }
        auto* rhs_lhs_ident = std::get_if<ast::IdentExpr>(&rhs_bin->lhs->kind);
        if (!rhs_lhs_ident || rhs_lhs_ident->name != lhs_ident->name) {
            return false;
        }
        if (rhs_bin->op != Op::Add && rhs_bin->op != Op::Sub) {
            return false;
        }

        info.sym = sym;
        info.op = rhs_bin->op;
        info.rhs = rhs_bin->rhs.get();
        info.has_rhs_imm = try_get_i64_immediate(*rhs_bin->rhs, info.rhs_imm);
        return true;
    }

    [[nodiscard]] bool analyze_general_self_update_expr(const ast::Expr& expr, SelfUpdateInfo& info) {
        using Op = ast::BinaryExpr::Op;

        auto* assign = std::get_if<ast::BinaryExpr>(&expr.kind);
        if (!assign || assign->op != Op::Assign || !current_scope_) {
            return false;
        }

        auto* lhs_ident = std::get_if<ast::IdentExpr>(&assign->lhs->kind);
        if (!lhs_ident) {
            return false;
        }

        Symbol* sym = current_scope_->lookup(lhs_ident->name);
        if (!sym || !sym->is_mut) {
            return false;
        }

        auto* rhs_bin = std::get_if<ast::BinaryExpr>(&assign->rhs->kind);
        if (!rhs_bin || rhs_bin->op == Op::Assign) {
            return false;
        }

        auto* rhs_lhs_ident = std::get_if<ast::IdentExpr>(&rhs_bin->lhs->kind);
        if (!rhs_lhs_ident || rhs_lhs_ident->name != lhs_ident->name) {
            return false;
        }

        info.sym = sym;
        info.op = rhs_bin->op;
        info.rhs = rhs_bin->rhs.get();
        info.has_rhs_imm = try_get_i64_immediate(*rhs_bin->rhs, info.rhs_imm);
        return true;
    }

    [[nodiscard]] bool analyze_field_self_update_expr(const ast::Expr& expr, FieldSelfUpdateInfo& info) {
        using Op = ast::BinaryExpr::Op;

        auto* assign = std::get_if<ast::BinaryExpr>(&expr.kind);
        if (!assign || assign->op != Op::Assign || !current_scope_) {
            return false;
        }

        auto* lhs_field = std::get_if<ast::FieldExpr>(&assign->lhs->kind);
        if (!lhs_field || !lhs_field->base->is<ast::IdentExpr>()) {
            return false;
        }

        const auto& base_name = lhs_field->base->as<ast::IdentExpr>().name;
        Symbol* base_sym = current_scope_->lookup(base_name);
        if (!base_sym || !allows_interior_mutation(*base_sym)) {
            return false;
        }

        auto* rhs_bin = std::get_if<ast::BinaryExpr>(&assign->rhs->kind);
        if (!rhs_bin || (rhs_bin->op != Op::Add && rhs_bin->op != Op::Sub)) {
            return false;
        }

        auto* rhs_lhs_field = std::get_if<ast::FieldExpr>(&rhs_bin->lhs->kind);
        if (!rhs_lhs_field || !rhs_lhs_field->base->is<ast::IdentExpr>()) {
            return false;
        }
        if (rhs_lhs_field->field != lhs_field->field) {
            return false;
        }
        if (rhs_lhs_field->base->as<ast::IdentExpr>().name != base_name) {
            return false;
        }

        auto struct_name = get_struct_name(base_sym->type);
        if (!struct_name) {
            return false;
        }
        auto struct_it = structs_.find(*struct_name);
        if (struct_it == structs_.end()) {
            return false;
        }
        auto offset_opt = struct_it->second.get_field_offset(lhs_field->field);
        if (!offset_opt) {
            return false;
        }

        info.base_sym = base_sym;
        info.field_offset = static_cast<std::int32_t>(*offset_opt);
        info.op = rhs_bin->op;
        info.rhs = rhs_bin->rhs.get();
        info.has_rhs_imm = try_get_i64_immediate(*rhs_bin->rhs, info.rhs_imm);
        return true;
    }

    [[nodiscard]] static bool try_match_counter_linear_expr(
        const ast::Expr& expr,
        const std::string& counter_name,
        const std::unordered_map<std::string, const ast::Expr*>& inline_lets,
        std::int64_t& scale,
        std::int64_t& disp
    ) {
        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            if (ident->name == counter_name) {
                scale = 1;
                disp = 0;
                return true;
            }
            if (auto it = inline_lets.find(ident->name); it != inline_lets.end()) {
                return try_match_counter_linear_expr(*it->second, counter_name, inline_lets, scale, disp);
            }
            return false;
        }

        if (auto* bin = std::get_if<ast::BinaryExpr>(&expr.kind)) {
            using Op = ast::BinaryExpr::Op;
            std::int64_t imm = 0;

            if (bin->op == Op::Mul) {
                const ast::Expr* other = nullptr;
                if (try_get_i64_immediate(*bin->lhs, imm)) {
                    other = bin->rhs.get();
                } else if (try_get_i64_immediate(*bin->rhs, imm)) {
                    other = bin->lhs.get();
                }
                if (!other) {
                    return false;
                }
                std::int64_t inner_scale = 0;
                std::int64_t inner_disp = 0;
                if (!try_match_counter_linear_expr(*other, counter_name, inline_lets, inner_scale, inner_disp)) {
                    return false;
                }
                if (inner_disp != 0) {
                    return false;
                }
                scale = inner_scale * imm;
                disp = 0;
                return true;
            }

            if (bin->op == Op::Add || bin->op == Op::Sub) {
                std::int64_t inner_scale = 0;
                std::int64_t inner_disp = 0;
                if (try_match_counter_linear_expr(*bin->lhs, counter_name, inline_lets, inner_scale, inner_disp) &&
                    try_get_i64_immediate(*bin->rhs, imm)) {
                    scale = inner_scale;
                    disp = inner_disp + (bin->op == Op::Add ? imm : -imm);
                    return true;
                }
                if (bin->op == Op::Add && try_get_i64_immediate(*bin->lhs, imm) &&
                    try_match_counter_linear_expr(*bin->rhs, counter_name, inline_lets, inner_scale, inner_disp)) {
                    scale = inner_scale;
                    disp = inner_disp + imm;
                    return true;
                }
            }
        }

        return false;
    }

    [[nodiscard]] bool try_match_lea_rhs(const ast::Expr& expr, LeaRhsPattern& pattern) {
        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            if (current_scope_) {
                if (Symbol* sym = current_scope_->lookup(ident->name)) {
                    if (auto bound = lookup_bound_reg(sym)) {
                        pattern.index_reg = *bound;
                        pattern.scale = 1;
                        pattern.disp = 0;
                        return true;
                    }
                }
                if (const ast::Expr* inline_expr = current_scope_->lookup_inline(ident->name)) {
                    return try_match_lea_rhs(*inline_expr, pattern);
                }
            }
            return false;
        }

        if (auto* bin = std::get_if<ast::BinaryExpr>(&expr.kind)) {
            using Op = ast::BinaryExpr::Op;

            std::int64_t imm = 0;
            if (bin->op == Op::Mul) {
                const ast::Expr* other = nullptr;
                if (try_get_i64_immediate(*bin->lhs, imm)) {
                    other = bin->rhs.get();
                } else if (try_get_i64_immediate(*bin->rhs, imm)) {
                    other = bin->lhs.get();
                }

                if (other && (imm == 1 || imm == 2 || imm == 4 || imm == 8)) {
                    LeaRhsPattern inner;
                    if (try_match_lea_rhs(*other, inner) && inner.scale == 1 && inner.disp == 0) {
                        pattern.index_reg = inner.index_reg;
                        pattern.scale = static_cast<std::uint8_t>(imm);
                        pattern.disp = 0;
                        return true;
                    }
                }
                return false;
            }

            if (bin->op == Op::Add || bin->op == Op::Sub) {
                LeaRhsPattern inner;
                if (try_match_lea_rhs(*bin->lhs, inner) && try_get_i64_immediate(*bin->rhs, imm)) {
                    std::int64_t disp64 = inner.disp + (bin->op == Op::Add ? imm : -imm);
                    if (disp64 >= (std::numeric_limits<std::int32_t>::min)() && disp64 <= (std::numeric_limits<std::int32_t>::max)()) {
                        pattern = inner;
                        pattern.disp = static_cast<std::int32_t>(disp64);
                        return true;
                    }
                }
                if (bin->op == Op::Add && try_get_i64_immediate(*bin->lhs, imm) && try_match_lea_rhs(*bin->rhs, inner)) {
                    std::int64_t disp64 = inner.disp + imm;
                    if (disp64 >= (std::numeric_limits<std::int32_t>::min)() && disp64 <= (std::numeric_limits<std::int32_t>::max)()) {
                        pattern = inner;
                        pattern.disp = static_cast<std::int32_t>(disp64);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    [[nodiscard]] bool generate_self_update_expr_stmt(const ast::Expr& expr) {
        SelfUpdateInfo update;
        if (!analyze_self_update_expr(expr, update) &&
            !analyze_general_self_update_expr(expr, update)) {
            return false;
        }

        if (auto bound = lookup_bound_reg(update.sym)) {
            if ((update.op == ast::BinaryExpr::Op::Add || update.op == ast::BinaryExpr::Op::Sub) &&
                update.has_rhs_imm) {
                std::int64_t delta = update.op == ast::BinaryExpr::Op::Add ? update.rhs_imm : -update.rhs_imm;
                emit_.add_smart(*bound, delta);
                if (auto it = counter_inductions_.find(update.sym); it != counter_inductions_.end()) {
                    emit_.add_smart(it->second.term_reg, it->second.step * delta);
                }
                return true;
            }
            if (!update.rhs || !can_generate_pure_expr(*update.rhs)) {
                return false;
            }
            if (update.op == ast::BinaryExpr::Op::Add) {
                if (auto it = accumulator_inductions_.find(update.sym); it != accumulator_inductions_.end()) {
                    emit_.add(*bound, it->second.term_reg);
                    return true;
                }
            }
            if (update.has_rhs_imm && supports_binary_rhs_imm(update.op, update.rhs_imm)) {
                return emit_binary_into_with_rhs_imm(*bound, update.op, update.rhs_imm);
            }
            auto rhs_reg = pick_scratch_reg({*bound});
            if (!rhs_reg) {
                return false;
            }
            if (!generate_pure_expr_into(*update.rhs, *rhs_reg)) {
                return false;
            }
            return emit_binary_into_with_rhs_reg(*bound, update.op, *rhs_reg);
        }

        if (update.has_rhs_imm &&
            supports_binary_rhs_imm(update.op, update.rhs_imm) &&
            update.op != ast::BinaryExpr::Op::Assign) {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, update.sym->stack_offset);
            if (!emit_binary_into_with_rhs_imm(x64::Reg::RAX, update.op, update.rhs_imm)) {
                return false;
            }
            emit_.mov_store(x64::Reg::RBP, update.sym->stack_offset, x64::Reg::RAX);
            return true;
        }

        if (update.has_rhs_imm &&
            update.rhs_imm >= (std::numeric_limits<std::int32_t>::min)() &&
            update.rhs_imm <= (std::numeric_limits<std::int32_t>::max)()) {
            auto imm32 = static_cast<std::int32_t>(update.rhs_imm);
            if (update.op == ast::BinaryExpr::Op::Add) {
                if (imm32 == 1) emit_.inc_mem(x64::Reg::RBP, update.sym->stack_offset);
                else if (imm32 == -1) emit_.dec_mem(x64::Reg::RBP, update.sym->stack_offset);
                else emit_.add_mem_imm(x64::Reg::RBP, update.sym->stack_offset, imm32);
            } else {
                if (imm32 == 1) emit_.dec_mem(x64::Reg::RBP, update.sym->stack_offset);
                else if (imm32 == -1) emit_.inc_mem(x64::Reg::RBP, update.sym->stack_offset);
                else emit_.sub_mem_imm(x64::Reg::RBP, update.sym->stack_offset, imm32);
            }
            return true;
        }

        if (!update.rhs || !can_generate_pure_expr(*update.rhs)) {
            return false;
        }

        auto rhs_reg = pick_scratch_reg({x64::Reg::RAX});
        if (!rhs_reg) {
            return false;
        }
        if (!generate_pure_expr_into(*update.rhs, *rhs_reg)) {
            return false;
        }
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, update.sym->stack_offset);
        if (!emit_binary_into_with_rhs_reg(x64::Reg::RAX, update.op, *rhs_reg)) {
            return false;
        }
        emit_.mov_store(x64::Reg::RBP, update.sym->stack_offset, x64::Reg::RAX);
        return true;

        return false;
    }

    [[nodiscard]] bool generate_field_self_update_expr_stmt(const ast::Expr& expr) {
        FieldSelfUpdateInfo update;
        if (!analyze_field_self_update_expr(expr, update)) {
            return false;
        }

        if (update.has_rhs_imm &&
            update.rhs_imm >= (std::numeric_limits<std::int32_t>::min)() &&
            update.rhs_imm <= (std::numeric_limits<std::int32_t>::max)()) {
            auto delta = update.op == ast::BinaryExpr::Op::Add ? update.rhs_imm : -update.rhs_imm;
            auto delta32 = static_cast<std::int32_t>(delta);
            if (auto bound = lookup_bound_reg(update.base_sym)) {
                if (delta32 == 1) emit_.inc_mem(*bound, update.field_offset);
                else if (delta32 == -1) emit_.dec_mem(*bound, update.field_offset);
                else if (delta32 > 0) emit_.add_mem_imm(*bound, update.field_offset, delta32);
                else emit_.sub_mem_imm(*bound, update.field_offset, -delta32);
                return true;
            }

            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, update.base_sym->stack_offset);
            if (delta32 == 1) emit_.inc_mem(x64::Reg::RAX, update.field_offset);
            else if (delta32 == -1) emit_.dec_mem(x64::Reg::RAX, update.field_offset);
            else if (delta32 > 0) emit_.add_mem_imm(x64::Reg::RAX, update.field_offset, delta32);
            else emit_.sub_mem_imm(x64::Reg::RAX, update.field_offset, -delta32);
            return true;
        }

        if (!update.rhs || !can_generate_pure_expr(*update.rhs)) {
            return false;
        }

        if (auto bound = lookup_bound_reg(update.base_sym)) {
            auto rhs_reg = pick_scratch_reg({*bound, x64::Reg::RDX});
            if (!rhs_reg) {
                return false;
            }
            if (!generate_pure_expr_into(*update.rhs, *rhs_reg)) {
                return false;
            }
            auto value_reg = pick_scratch_reg({*bound, *rhs_reg, x64::Reg::RDX});
            if (!value_reg) {
                return false;
            }
            emit_.mov_load(*value_reg, *bound, update.field_offset);
            if (!emit_binary_into_with_rhs_reg(*value_reg, update.op, *rhs_reg)) {
                return false;
            }
            emit_.mov_store(*bound, update.field_offset, *value_reg);
            return true;
        }

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, update.base_sym->stack_offset);
        auto rhs_reg = pick_scratch_reg({x64::Reg::RAX, x64::Reg::RDX});
        if (!rhs_reg) {
            return false;
        }
        if (!generate_pure_expr_into(*update.rhs, *rhs_reg)) {
            return false;
        }
        auto value_reg = pick_scratch_reg({x64::Reg::RAX, *rhs_reg, x64::Reg::RDX});
        if (!value_reg) {
            return false;
        }
        emit_.mov_load(*value_reg, x64::Reg::RAX, update.field_offset);
        if (!emit_binary_into_with_rhs_reg(*value_reg, update.op, *rhs_reg)) {
            return false;
        }
        emit_.mov_store(x64::Reg::RAX, update.field_offset, *value_reg);
        return true;
    }

    [[nodiscard]] static void push_unique_symbol(std::vector<Symbol*>& symbols, Symbol* sym) {
        if (!sym) {
            return;
        }
        if (std::find(symbols.begin(), symbols.end(), sym) == symbols.end()) {
            symbols.push_back(sym);
        }
    }

    [[nodiscard]] bool try_make_while_multistate_reduction_plan(
        const WhileMultiStatePlan& plan,
        const ast::ReturnStmt& ret,
        WhileMultiStateReductionPlan& reduced
    ) {
        const ast::Expr* ret_expr = ret.value.get();
        if (!ret_expr || !current_scope_) {
            return false;
        }

        auto* ident = std::get_if<ast::IdentExpr>(&ret_expr->kind);
        if (!ident) {
            return false;
        }

        Symbol* accum_sym = current_scope_->lookup(ident->name);
        if (!accum_sym || accum_sym == plan.counter_sym) {
            return false;
        }

        struct StepInfo {
            std::size_t pos = 0;
            std::int64_t delta = 0;
        };

        std::unordered_map<Symbol*, std::size_t> add_positions;
        std::unordered_map<Symbol*, StepInfo> step_infos;
        std::size_t min_step_pos = std::numeric_limits<std::size_t>::max();

        for (std::size_t index = 0; index < plan.updates.size(); ++index) {
            const auto& update = plan.updates[index];
            if (update.sym == plan.counter_sym) {
                continue;
            }

            if (update.sym == accum_sym) {
                if (update.op != ast::BinaryExpr::Op::Add || update.has_rhs_imm || !update.rhs_sym ||
                    update.rhs_sym == plan.counter_sym || update.rhs_sym == accum_sym ||
                    add_positions.contains(update.rhs_sym)) {
                    return false;
                }
                add_positions[update.rhs_sym] = index;
                continue;
            }

            if (!update.has_rhs_imm || step_infos.contains(update.sym)) {
                return false;
            }

            std::int64_t delta = update.op == ast::BinaryExpr::Op::Add ? update.rhs_imm : -update.rhs_imm;
            step_infos[update.sym] = StepInfo{index, delta};
            min_step_pos = std::min(min_step_pos, index);
        }

        if (add_positions.size() < 2 || add_positions.size() != step_infos.size()) {
            return false;
        }

        std::vector<std::pair<std::size_t, Symbol*>> contributors;
        contributors.reserve(add_positions.size());
        std::int64_t term_step = 0;
        for (const auto& [sym, add_pos] : add_positions) {
            auto step_it = step_infos.find(sym);
            if (step_it == step_infos.end() || add_pos >= min_step_pos || add_pos >= step_it->second.pos) {
                return false;
            }
            contributors.push_back({add_pos, sym});
            term_step += step_it->second.delta;
        }

        std::sort(contributors.begin(), contributors.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        reduced.limit = plan.limit;
        reduced.counter_sym = plan.counter_sym;
        reduced.accum_sym = accum_sym;
        reduced.contributor_syms.clear();
        reduced.contributor_syms.reserve(contributors.size());
        for (const auto& [_, sym] : contributors) {
            reduced.contributor_syms.push_back(sym);
        }
        reduced.term_step = term_step;
        return true;
    }

    [[nodiscard]] bool try_make_while_alternating_branch_reduction_plan(
        const ast::WhileStmt& while_stmt,
        const ast::ReturnStmt& ret,
        WhileAlternatingBranchReductionPlan& reduced
    ) {
        using Op = ast::BinaryExpr::Op;

        if (!current_scope_) {
            return false;
        }

        auto* ret_ident = ret.value ? std::get_if<ast::IdentExpr>(&ret.value->kind) : nullptr;
        if (!ret_ident) {
            return false;
        }
        Symbol* accum_sym = current_scope_->lookup(ret_ident->name);
        if (!accum_sym) {
            return false;
        }

        auto* cond = std::get_if<ast::BinaryExpr>(&while_stmt.condition->kind);
        if (!cond || cond->op != Op::Lt) {
            return false;
        }
        auto* counter_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
        if (!counter_ident) {
            return false;
        }
        std::int64_t limit = 0;
        if (!try_get_i64_immediate(*cond->rhs, limit)) {
            return false;
        }
        Symbol* counter_sym = current_scope_->lookup(counter_ident->name);
        if (!counter_sym || !counter_sym->is_mut) {
            return false;
        }

        if (while_stmt.body.size() != 2 || !while_stmt.body[0]->is<ast::IfStmt>() || !while_stmt.body[1]->is<ast::ExprStmt>()) {
            return false;
        }

        SelfUpdateInfo counter_update;
        if (!analyze_self_update_expr(*while_stmt.body[1]->as<ast::ExprStmt>().expr, counter_update) ||
            counter_update.sym != counter_sym || !counter_update.has_rhs_imm) {
            return false;
        }
        std::int64_t counter_delta = counter_update.op == Op::Add ? counter_update.rhs_imm : -counter_update.rhs_imm;
        if (counter_delta != 1) {
            return false;
        }

        const auto& if_stmt = while_stmt.body[0]->as<ast::IfStmt>();
        auto* branch_cond = std::get_if<ast::BinaryExpr>(&if_stmt.condition->kind);
        if (!branch_cond || branch_cond->op != Op::Eq) {
            return false;
        }

        auto matches_even_test = [&](const ast::Expr& lhs, const ast::Expr& rhs) {
            std::int64_t imm = 0;
            auto* and_expr = std::get_if<ast::BinaryExpr>(&lhs.kind);
            if (!and_expr || and_expr->op != Op::BitAnd || !try_get_i64_immediate(rhs, imm) || imm != 0) {
                return false;
            }
            auto* lhs_ident = std::get_if<ast::IdentExpr>(&and_expr->lhs->kind);
            if (!lhs_ident || lhs_ident->name != counter_ident->name) {
                return false;
            }
            return try_get_i64_immediate(*and_expr->rhs, imm) && imm == 1;
        };
        if (!matches_even_test(*branch_cond->lhs, *branch_cond->rhs) && !matches_even_test(*branch_cond->rhs, *branch_cond->lhs)) {
            return false;
        }

        auto parse_branch = [&](const std::vector<ast::StmtPtr>& block, Symbol*& stream_sym, std::int64_t& stream_step) {
            if (block.size() != 2 || !block[0]->is<ast::ExprStmt>() || !block[1]->is<ast::ExprStmt>()) {
                return false;
            }

            SelfUpdateInfo add_update;
            SelfUpdateInfo stream_update;
            if (!analyze_self_update_expr(*block[0]->as<ast::ExprStmt>().expr, add_update) ||
                !analyze_self_update_expr(*block[1]->as<ast::ExprStmt>().expr, stream_update)) {
                return false;
            }
            if (add_update.sym != accum_sym || add_update.op != Op::Add || !add_update.rhs || add_update.has_rhs_imm) {
                return false;
            }
            auto* rhs_ident = std::get_if<ast::IdentExpr>(&add_update.rhs->kind);
            if (!rhs_ident) {
                return false;
            }
            stream_sym = current_scope_->lookup(rhs_ident->name);
            if (!stream_sym || stream_sym != stream_update.sym || !stream_update.has_rhs_imm) {
                return false;
            }
            if (stream_sym == accum_sym || stream_sym == counter_sym) {
                return false;
            }
            stream_step = stream_update.op == Op::Add ? stream_update.rhs_imm : -stream_update.rhs_imm;
            return true;
        };

        Symbol* even_sym = nullptr;
        Symbol* odd_sym = nullptr;
        std::int64_t even_step = 0;
        std::int64_t odd_step = 0;
        if (!if_stmt.else_block ||
            !parse_branch(if_stmt.then_block, even_sym, even_step) ||
            !parse_branch(*if_stmt.else_block, odd_sym, odd_step)) {
            return false;
        }
        if (!even_sym || !odd_sym || even_sym == odd_sym) {
            return false;
        }

        reduced.limit = limit;
        reduced.counter_sym = counter_sym;
        reduced.accum_sym = accum_sym;
        reduced.even_sym = even_sym;
        reduced.odd_sym = odd_sym;
        reduced.even_step = even_step;
        reduced.odd_step = odd_step;
        return true;
    }

    [[nodiscard]] bool collect_bound_symbols_from_pure_expr(const ast::Expr& expr, std::vector<Symbol*>& symbols) {
        return std::visit(overloaded{
            [&](const ast::LiteralExpr&) -> bool { return true; },
            [&](const ast::IdentExpr& ident) -> bool {
                if (!current_scope_) {
                    return false;
                }
                if (Symbol* sym = current_scope_->lookup(ident.name)) {
                    if (sym->is_mut) {
                        push_unique_symbol(symbols, sym);
                    }
                    return true;
                }
                return true;
            },
            [&](const ast::UnaryExpr& un) -> bool {
                return collect_bound_symbols_from_pure_expr(*un.operand, symbols);
            },
            [&](const ast::BinaryExpr& bin) -> bool {
                return collect_bound_symbols_from_pure_expr(*bin.lhs, symbols) &&
                       collect_bound_symbols_from_pure_expr(*bin.rhs, symbols);
            },
            [&](const auto&) -> bool { return false; },
        }, expr.kind);
    }

    [[nodiscard]] bool collect_bound_symbols_from_stmt(const ast::Stmt& stmt, Symbol* counter_sym, std::vector<Symbol*>& symbols) {
        return std::visit(overloaded{
            [&](const ast::LetStmt& let) -> bool {
                if (let.is_mut || !let.init || !expr_is_side_effect_free(*let.init) || !can_generate_pure_expr(*let.init)) {
                    return false;
                }
                return collect_bound_symbols_from_pure_expr(*let.init, symbols);
            },
            [&](const ast::ExprStmt& expr_stmt) -> bool {
                SelfUpdateInfo update;
                if (!analyze_self_update_expr(*expr_stmt.expr, update)) {
                    return false;
                }
                if (update.sym == counter_sym) {
                    return false;
                }
                if (!update.rhs || !expr_is_side_effect_free(*update.rhs) || !can_generate_pure_expr(*update.rhs)) {
                    return false;
                }
                push_unique_symbol(symbols, update.sym);
                return collect_bound_symbols_from_pure_expr(*update.rhs, symbols);
            },
            [&](const ast::IfStmt& if_stmt) -> bool {
                if (!expr_is_side_effect_free(*if_stmt.condition) || !can_generate_pure_expr(*if_stmt.condition)) {
                    return false;
                }
                if (!collect_bound_symbols_from_pure_expr(*if_stmt.condition, symbols)) {
                    return false;
                }
                for (const auto& branch_stmt : if_stmt.then_block) {
                    if (!collect_bound_symbols_from_stmt(*branch_stmt, counter_sym, symbols)) {
                        return false;
                    }
                }
                if (if_stmt.else_block) {
                    for (const auto& branch_stmt : *if_stmt.else_block) {
                        if (!collect_bound_symbols_from_stmt(*branch_stmt, counter_sym, symbols)) {
                            return false;
                        }
                    }
                }
                return true;
            },
            [&](const ast::BlockStmt& block) -> bool {
                for (const auto& inner : block.block.stmts) {
                    if (!collect_bound_symbols_from_stmt(*inner, counter_sym, symbols)) {
                        return false;
                    }
                }
                return true;
            },
            [&](const auto&) -> bool { return false; },
        }, stmt.kind);
    }

    [[nodiscard]] bool try_make_while_bound_plan(const ast::WhileStmt& while_stmt, WhileBoundPlan& plan) {
        using Op = ast::BinaryExpr::Op;

        if (!current_scope_) {
            return false;
        }

        auto* cond = std::get_if<ast::BinaryExpr>(&while_stmt.condition->kind);
        if (!cond || cond->op != Op::Lt) {
            return false;
        }
        auto* counter_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
        if (!counter_ident) {
            return false;
        }

        std::int64_t limit = 0;
        if (!try_get_i64_immediate(*cond->rhs, limit)) {
            return false;
        }

        Symbol* counter_sym = current_scope_->lookup(counter_ident->name);
        if (!counter_sym || !counter_sym->is_mut) {
            return false;
        }

        std::vector<Symbol*> symbols;
        bool saw_counter_update = false;
        for (const auto& stmt : while_stmt.body) {
            if (stmt->is<ast::ExprStmt>()) {
                SelfUpdateInfo update;
                if (analyze_self_update_expr(*stmt->as<ast::ExprStmt>().expr, update) && update.sym == counter_sym) {
                    if (!update.has_rhs_imm) {
                        return false;
                    }
                    std::int64_t delta = update.op == Op::Add ? update.rhs_imm : -update.rhs_imm;
                    if (delta != 1) {
                        return false;
                    }
                    saw_counter_update = true;
                    continue;
                }
            }

            if (!collect_bound_symbols_from_stmt(*stmt, counter_sym, symbols)) {
                return false;
            }
        }

        symbols.erase(std::remove(symbols.begin(), symbols.end(), counter_sym), symbols.end());

        if (!saw_counter_update) {
            return false;
        }
        if (symbols.size() < 2 || symbols.size() > 4) {
            return false;
        }

        plan.limit = limit;
        plan.bindings.clear();
        plan.bindings.push_back({counter_sym, x64::Reg::RCX});

        constexpr std::array<x64::Reg, 4> value_regs = {
            x64::Reg::RAX,
            x64::Reg::RDX,
            x64::Reg::R8,
            x64::Reg::R9,
        };
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            plan.bindings.push_back({symbols[i], value_regs[i]});
        }

        return true;
    }

    [[nodiscard]] bool try_make_while_multistate_plan(const ast::WhileStmt& while_stmt, WhileMultiStatePlan& plan) {
        using Op = ast::BinaryExpr::Op;

        if (!current_scope_) {
            return false;
        }

        auto* cond = std::get_if<ast::BinaryExpr>(&while_stmt.condition->kind);
        if (!cond || cond->op != Op::Lt) {
            return false;
        }
        auto* counter_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
        if (!counter_ident) {
            return false;
        }

        std::int64_t limit = 0;
        if (!try_get_i64_immediate(*cond->rhs, limit)) {
            return false;
        }

        Symbol* counter_sym = current_scope_->lookup(counter_ident->name);
        if (!counter_sym || !counter_sym->is_mut) {
            return false;
        }

        plan.limit = limit;
        plan.counter_sym = counter_sym;
        plan.bindings.clear();
        plan.updates.clear();
        plan.bindings.push_back({counter_sym, x64::Reg::RCX});

        bool saw_counter_update = false;
        std::vector<Symbol*> ordered_symbols;

        for (const auto& stmt : while_stmt.body) {
            if (!stmt->is<ast::ExprStmt>()) {
                return false;
            }

            SelfUpdateInfo update;
            if (!analyze_self_update_expr(*stmt->as<ast::ExprStmt>().expr, update)) {
                return false;
            }

            WhileMultiStatePlan::Update plan_update{
                .sym = update.sym,
                .op = update.op,
            };

            if (update.sym == counter_sym) {
                if (!update.has_rhs_imm) {
                    return false;
                }
                std::int64_t delta = update.op == Op::Add ? update.rhs_imm : -update.rhs_imm;
                if (delta != 1) {
                    return false;
                }
                plan_update.op = Op::Add;
                plan_update.has_rhs_imm = true;
                plan_update.rhs_imm = 1;
                saw_counter_update = true;
                plan.updates.push_back(plan_update);
                continue;
            }

            push_unique_symbol(ordered_symbols, update.sym);

            if (update.has_rhs_imm) {
                plan_update.has_rhs_imm = true;
                plan_update.rhs_imm = update.rhs_imm;
            } else if (update.rhs) {
                auto* rhs_ident = std::get_if<ast::IdentExpr>(&update.rhs->kind);
                if (!rhs_ident) {
                    return false;
                }
                Symbol* rhs_sym = current_scope_->lookup(rhs_ident->name);
                if (!rhs_sym) {
                    return false;
                }
                if (rhs_sym != counter_sym && !rhs_sym->is_mut) {
                    return false;
                }
                if (rhs_sym != counter_sym) {
                    push_unique_symbol(ordered_symbols, rhs_sym);
                }
                plan_update.rhs_sym = rhs_sym;
            } else {
                return false;
            }

            plan.updates.push_back(plan_update);
        }

        if (!saw_counter_update) {
            return false;
        }
        if (ordered_symbols.size() < 2 || ordered_symbols.size() > 4) {
            return false;
        }

        constexpr std::array<x64::Reg, 4> value_regs = {
            x64::Reg::RAX,
            x64::Reg::RDX,
            x64::Reg::R8,
            x64::Reg::R9,
        };
        for (std::size_t i = 0; i < ordered_symbols.size(); ++i) {
            plan.bindings.push_back({ordered_symbols[i], value_regs[i]});
        }

        return true;
    }

    [[nodiscard]] static std::optional<x64::Reg> lookup_multistate_plan_reg(const WhileMultiStatePlan& plan, const Symbol* sym) {
        for (const auto& [bound_sym, reg] : plan.bindings) {
            if (bound_sym == sym) {
                return reg;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool emit_multistate_while_update(const WhileMultiStatePlan& plan, const WhileMultiStatePlan::Update& update) {
        auto dst = lookup_multistate_plan_reg(plan, update.sym);
        if (!dst) {
            return false;
        }

        if (update.has_rhs_imm) {
            std::int64_t delta = update.op == ast::BinaryExpr::Op::Add ? update.rhs_imm : -update.rhs_imm;
            emit_.add_smart(*dst, delta);
            return true;
        }

        auto rhs = lookup_multistate_plan_reg(plan, update.rhs_sym);
        if (!rhs) {
            return false;
        }

        return emit_binary_into_with_rhs_reg(*dst, update.op, *rhs);
    }

    [[nodiscard]] bool generate_while_multistate_registerized(const WhileMultiStatePlan& plan) {
        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        for (const auto& update : plan.updates) {
            if (!emit_multistate_while_update(plan, update)) {
                return false;
            }
        }

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        emit_.patch_jump(exit_patch);
        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, reg);
        }

        return true;
    }

    [[nodiscard]] bool generate_while_multistate_registerized_return(
        const WhileMultiStatePlan& plan,
        const ast::ReturnStmt& ret,
        const std::unordered_map<const Symbol*, const ast::Expr*>* init_map = nullptr
    ) {
        const ast::Expr* ret_expr = ret.value.get();
        x64::Reg return_reg = x64::Reg::RAX;

        if (ret_expr) {
            auto* ident = std::get_if<ast::IdentExpr>(&ret_expr->kind);
            if (!ident || !current_scope_) {
                return false;
            }
            Symbol* ret_sym = current_scope_->lookup(ident->name);
            auto bound = lookup_multistate_plan_reg(plan, ret_sym);
            if (!bound) {
                return false;
            }
            return_reg = *bound;
        }

        for (const auto& [sym, reg] : plan.bindings) {
            if (init_map) {
                if (auto it = init_map->find(sym); it != init_map->end()) {
                    if (!generate_pure_expr_into(*it->second, reg)) {
                        return false;
                    }
                    continue;
                }
            }
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        for (const auto& update : plan.updates) {
            if (!emit_multistate_while_update(plan, update)) {
                return false;
            }
        }

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        emit_.patch_jump(exit_patch);
        if (!ret_expr) {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        } else if (return_reg != x64::Reg::RAX) {
            emit_.mov(x64::Reg::RAX, return_reg);
        }
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] bool generate_while_bound_registerized(const ast::WhileStmt& while_stmt, const WhileBoundPlan& plan) {
        break_patches_.push_back({});
        continue_patches_.push_back({});

        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        push_scope();
        for (const auto& [sym, reg] : plan.bindings) {
            register_bindings_[sym] = reg;
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        if (!generate_stmt_list(while_stmt.body)) {
            for (const auto& [sym, _] : plan.bindings) {
                register_bindings_.erase(sym);
            }
            pop_scope();
            continue_patches_.pop_back();
            break_patches_.pop_back();
            return false;
        }

        for (const auto& [sym, _] : plan.bindings) {
            register_bindings_.erase(sym);
        }
        pop_scope();

        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        std::size_t exit_pos = emit_.buffer().pos();
        emit_.patch_jump(exit_patch);
        patch_jumps_to(break_patches_.back(), exit_pos);
        break_patches_.pop_back();

        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, reg);
        }

        return true;
    }

    [[nodiscard]] bool generate_while_bound_registerized_return(
        const ast::WhileStmt& while_stmt,
        const WhileBoundPlan& plan,
        const ast::ReturnStmt& ret,
        const std::unordered_map<const Symbol*, const ast::Expr*>* init_map = nullptr
    ) {
        const ast::Expr* ret_expr = ret.value.get();
        x64::Reg return_reg = x64::Reg::RAX;
        if (ret_expr) {
            auto* ident = std::get_if<ast::IdentExpr>(&ret_expr->kind);
            if (!ident || !current_scope_) {
                return false;
            }
            Symbol* ret_sym = current_scope_->lookup(ident->name);
            if (!ret_sym) {
                return false;
            }

            bool found = false;
            for (const auto& [sym, reg] : plan.bindings) {
                if (sym == ret_sym) {
                    return_reg = reg;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        break_patches_.push_back({});
        continue_patches_.push_back({});

        for (const auto& [sym, reg] : plan.bindings) {
            if (init_map) {
                if (auto it = init_map->find(sym); it != init_map->end()) {
                    if (!generate_pure_expr_into(*it->second, reg)) {
                        return false;
                    }
                    continue;
                }
            }
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        push_scope();
        for (const auto& [sym, reg] : plan.bindings) {
            register_bindings_[sym] = reg;
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        if (!generate_stmt_list(while_stmt.body)) {
            for (const auto& [sym, _] : plan.bindings) {
                register_bindings_.erase(sym);
            }
            pop_scope();
            continue_patches_.pop_back();
            break_patches_.pop_back();
            return false;
        }

        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        std::size_t exit_pos = emit_.buffer().pos();
        emit_.patch_jump(exit_patch);
        patch_jumps_to(break_patches_.back(), exit_pos);
        break_patches_.pop_back();

        for (const auto& [sym, _] : plan.bindings) {
            register_bindings_.erase(sym);
        }
        pop_scope();

        if (!ret_expr) {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        } else if (return_reg != x64::Reg::RAX) {
            emit_.mov(x64::Reg::RAX, return_reg);
        }
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] static bool is_ptr_like_type(const Type& type) {
        if (auto* prim = std::get_if<PrimitiveType>(&type.kind)) {
            return *prim == PrimitiveType::Ptr;
        }
        return std::holds_alternative<PointerType>(type.kind) ||
               std::holds_alternative<FunctionType>(type.kind);
    }

    [[nodiscard]] bool emit_ptr_symbol_into(Symbol* sym, x64::Reg dst) {
        if (!sym) {
            return false;
        }
        if (auto bound = lookup_bound_reg(sym)) {
            if (*bound != dst) {
                emit_.mov(dst, *bound);
            }
            return true;
        }
        emit_.mov_load(dst, x64::Reg::RBP, sym->stack_offset);
        return true;
    }

    [[nodiscard]] static std::optional<WhileSimdKernelPlan::Op> try_get_simd_kernel_op(std::string_view name) {
        if (name == "simd_i32x16_add") return WhileSimdKernelPlan::Op::Add;
        if (name == "simd_i32x16_sub") return WhileSimdKernelPlan::Op::Sub;
        if (name == "simd_i32x16_mul") return WhileSimdKernelPlan::Op::Mul;
        return std::nullopt;
    }

    [[nodiscard]] bool try_make_while_simd_kernel_plan(const ast::WhileStmt& while_stmt, WhileSimdKernelPlan& plan) {
        using Op = ast::BinaryExpr::Op;

        if (!current_scope_) {
            return false;
        }

        auto* cond = std::get_if<ast::BinaryExpr>(&while_stmt.condition->kind);
        if (!cond || cond->op != Op::Lt) {
            return false;
        }
        auto* counter_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
        if (!counter_ident) {
            return false;
        }

        std::int64_t limit = 0;
        if (!try_get_i64_immediate(*cond->rhs, limit)) {
            return false;
        }

        Symbol* counter_sym = current_scope_->lookup(counter_ident->name);
        if (!counter_sym || !counter_sym->is_mut) {
            return false;
        }
        if (while_stmt.body.size() < 2) {
            return false;
        }

        const auto* tail_expr_stmt = std::get_if<ast::ExprStmt>(&while_stmt.body.back()->kind);
        if (!tail_expr_stmt) {
            return false;
        }
        auto is_counter_increment = [&](const ast::Expr& expr) -> bool {
            SelfUpdateInfo counter_update;
            if (analyze_self_update_expr(expr, counter_update) &&
                counter_update.sym == counter_sym &&
                counter_update.has_rhs_imm &&
                counter_update.op == Op::Add &&
                counter_update.rhs_imm == 1) {
                return true;
            }
            auto* un = std::get_if<ast::UnaryExpr>(&expr.kind);
            if (!un) {
                return false;
            }
            using UnOp = ast::UnaryExpr::Op;
            if (un->op != UnOp::PreInc && un->op != UnOp::PostInc) {
                return false;
            }
            auto* ident = std::get_if<ast::IdentExpr>(&un->operand->kind);
            return ident && ident->name == counter_ident->name;
        };

        if (!is_counter_increment(*tail_expr_stmt->expr)) {
            return false;
        }

        plan.limit = limit;
        plan.counter_sym = counter_sym;
        plan.zmm_bindings.clear();
        plan.init_loads.clear();
        plan.final_stores.clear();
        plan.steps.clear();

        constexpr std::array<std::uint8_t, 16> kZmmRegs = {
            0, 1, 2, 3, 4, 5, 6, 7,
            8, 9, 10, 11, 12, 13, 14, 15
        };

        std::size_t next_zmm = 0;
        std::unordered_map<Symbol*, std::uint8_t> zmm_map;
        std::unordered_set<Symbol*> available_values;
        std::unordered_set<Symbol*> final_store_set;
        std::unordered_set<Symbol*> init_load_set;

        auto bind_sym = [&](Symbol* sym) -> bool {
            if (!sym) return false;
            if (zmm_map.contains(sym)) return true;
            if (next_zmm >= kZmmRegs.size()) return false;
            std::uint8_t reg = kZmmRegs[next_zmm++];
            zmm_map[sym] = reg;
            plan.zmm_bindings.push_back({sym, reg});
            return true;
        };

        auto require_sym_value = [&](Symbol* sym) {
            if (available_values.contains(sym)) {
                return;
            }
            available_values.insert(sym);
            if (init_load_set.insert(sym).second) {
                plan.init_loads.push_back(sym);
            }
        };

        for (std::size_t i = 0; i + 1 < while_stmt.body.size(); ++i) {
            const auto* expr_stmt = std::get_if<ast::ExprStmt>(&while_stmt.body[i]->kind);
            if (!expr_stmt) {
                return false;
            }
            const auto* call = std::get_if<ast::CallExpr>(&expr_stmt->expr->kind);
            if (!call || !call->callee->is<ast::IdentExpr>()) {
                return false;
            }
            auto op = try_get_simd_kernel_op(call->callee->as<ast::IdentExpr>().name);
            if (!op || call->args.size() != 3) {
                return false;
            }

            auto* dst_ident = std::get_if<ast::IdentExpr>(&call->args[0]->kind);
            auto* lhs_ident = std::get_if<ast::IdentExpr>(&call->args[1]->kind);
            auto* rhs_ident = std::get_if<ast::IdentExpr>(&call->args[2]->kind);
            if (!dst_ident || !lhs_ident || !rhs_ident) {
                return false;
            }

            Symbol* dst_sym = current_scope_->lookup(dst_ident->name);
            Symbol* lhs_sym = current_scope_->lookup(lhs_ident->name);
            Symbol* rhs_sym = current_scope_->lookup(rhs_ident->name);
            if (!dst_sym || !lhs_sym || !rhs_sym) {
                return false;
            }
            if (dst_sym == counter_sym || lhs_sym == counter_sym || rhs_sym == counter_sym) {
                return false;
            }
            if (!bind_sym(dst_sym) || !bind_sym(lhs_sym) || !bind_sym(rhs_sym)) {
                return false;
            }

            require_sym_value(lhs_sym);
            require_sym_value(rhs_sym);
            available_values.insert(dst_sym);

            if (final_store_set.insert(dst_sym).second) {
                plan.final_stores.push_back(dst_sym);
            }

            plan.steps.push_back(WhileSimdKernelPlan::Step{
                .op = *op,
                .dst = dst_sym,
                .lhs = lhs_sym,
                .rhs = rhs_sym,
            });
        }

        return !plan.steps.empty();
    }

    [[nodiscard]] bool generate_while_simd_kernel_registerized(const WhileSimdKernelPlan& plan) {
        auto lookup_zmm = [&](Symbol* sym) -> std::optional<std::uint8_t> {
            for (const auto& [bound_sym, zmm] : plan.zmm_bindings) {
                if (bound_sym == sym) {
                    return zmm;
                }
            }
            return std::nullopt;
        };

        auto emit_body = [&]() -> bool {
            for (const auto& step : plan.steps) {
                auto zdst = lookup_zmm(step.dst);
                auto zlhs = lookup_zmm(step.lhs);
                auto zrhs = lookup_zmm(step.rhs);
                if (!zdst || !zlhs || !zrhs) {
                    return false;
                }

                if (*zdst != *zlhs) {
                    emit_.vmovdqa32_zmm(*zdst, *zlhs);
                }

                switch (step.op) {
                    case WhileSimdKernelPlan::Op::Add:
                        emit_.vpaddd_zmm(*zdst, *zdst, *zrhs);
                        break;
                    case WhileSimdKernelPlan::Op::Sub:
                        emit_.vpsubd_zmm(*zdst, *zdst, *zrhs);
                        break;
                    case WhileSimdKernelPlan::Op::Mul:
                        emit_.vpmulld_zmm(*zdst, *zdst, *zrhs);
                        break;
                }
            }
            return true;
        };

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, plan.counter_sym->stack_offset);

        for (Symbol* sym : plan.init_loads) {
            auto zmm = lookup_zmm(sym);
            if (!zmm) {
                return false;
            }
            if (!emit_ptr_symbol_into(sym, x64::Reg::R10)) {
                return false;
            }
            emit_.vmovdqu32_zmm_load(*zmm, x64::Reg::R10);
        }

        constexpr std::int64_t kUnroll = 4;
        if (plan.limit >= kUnroll) {
            const std::int64_t threshold = plan.limit - (kUnroll - 1);
            std::size_t unroll_loop_start = emit_.buffer().pos();
            emit_.cmp_smart_imm(x64::Reg::RCX, threshold);
            std::size_t tail_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

            for (std::int64_t i = 0; i < kUnroll; ++i) {
                if (!emit_body()) {
                    return false;
                }
            }

            emit_.add_smart(x64::Reg::RCX, static_cast<std::int32_t>(kUnroll));
            std::int32_t rel = static_cast<std::int32_t>(unroll_loop_start - emit_.buffer().pos() - 5);
            emit_.jmp_rel32(rel);

            emit_.patch_jump(tail_patch);
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        if (!emit_body()) {
            return false;
        }

        emit_.inc(x64::Reg::RCX);
        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        emit_.patch_jump(exit_patch);

        for (Symbol* sym : plan.final_stores) {
            auto zmm = lookup_zmm(sym);
            if (!zmm) {
                return false;
            }
            if (!emit_ptr_symbol_into(sym, x64::Reg::R10)) {
                return false;
            }
            emit_.vmovdqu32_zmm_store(x64::Reg::R10, 0, *zmm);
        }

        emit_.mov_store(x64::Reg::RBP, plan.counter_sym->stack_offset, x64::Reg::RCX);
        return true;
    }

    [[nodiscard]] bool try_make_while_register_plan(const ast::WhileStmt& while_stmt, WhileRegisterPlan& plan) {
        using Op = ast::BinaryExpr::Op;

        if (!current_scope_) {
            return false;
        }

        auto* cond = std::get_if<ast::BinaryExpr>(&while_stmt.condition->kind);
        if (!cond || cond->op != Op::Lt) {
            return false;
        }
        auto* counter_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
        if (!counter_ident) {
            return false;
        }

        std::int64_t limit = 0;
        if (!try_get_i64_immediate(*cond->rhs, limit)) {
            return false;
        }

        Symbol* counter_sym = current_scope_->lookup(counter_ident->name);
        if (!counter_sym || !counter_sym->is_mut) {
            return false;
        }

        plan.limit = limit;
        plan.bindings.clear();
        plan.bindings.push_back({counter_sym, x64::Reg::RCX});
        plan.induction.reset();

        Symbol* acc_sym = nullptr;
        bool saw_counter_update = false;
        std::unordered_map<std::string, const ast::Expr*> inline_lets;

        for (const auto& stmt : while_stmt.body) {
            if (stmt->is<ast::LetStmt>()) {
                const auto& let = stmt->as<ast::LetStmt>();
                if (let.is_mut || !let.init || !expr_is_side_effect_free(*let.init) || !can_generate_pure_expr(*let.init)) {
                    return false;
                }
                inline_lets[let.name] = let.init.get();
                continue;
            }

            if (!stmt->is<ast::ExprStmt>()) {
                return false;
            }

            SelfUpdateInfo update;
            if (!analyze_self_update_expr(*stmt->as<ast::ExprStmt>().expr, update)) {
                return false;
            }

            if (update.sym == counter_sym) {
                if (!update.has_rhs_imm) {
                    return false;
                }
                std::int64_t delta = update.op == Op::Add ? update.rhs_imm : -update.rhs_imm;
                if (delta != 1) {
                    return false;
                }
                saw_counter_update = true;
                continue;
            }

            if (!update.rhs || !can_generate_pure_expr(*update.rhs)) {
                return false;
            }

            if (acc_sym && acc_sym != update.sym) {
                return false;
            }
            acc_sym = update.sym;

            if (update.op == Op::Add) {
                std::int64_t step = 0;
                std::int64_t disp64 = 0;
                if (try_match_counter_linear_expr(*update.rhs, counter_ident->name, inline_lets, step, disp64) &&
                    disp64 >= (std::numeric_limits<std::int32_t>::min)() &&
                    disp64 <= (std::numeric_limits<std::int32_t>::max)()) {
                    plan.induction = WhileRegisterPlan::InductionPlan{
                        .accum_sym = acc_sym,
                        .term_reg = x64::Reg::RDX,
                        .step = step,
                        .disp = static_cast<std::int32_t>(disp64)
                    };
                }
            }
        }

        if (!saw_counter_update || !acc_sym) {
            return false;
        }

        plan.bindings.push_back({acc_sym, x64::Reg::RAX});
        return true;
    }

    [[nodiscard]] bool generate_while_registerized_return(
        const ast::WhileStmt& while_stmt,
        const WhileRegisterPlan& plan,
        const ast::ReturnStmt& ret,
        const std::unordered_map<const Symbol*, const ast::Expr*>* init_map = nullptr
    ) {
        const ast::Expr* ret_expr = ret.value.get();
        x64::Reg return_reg = x64::Reg::RAX;
        if (ret_expr) {
            auto* ident = std::get_if<ast::IdentExpr>(&ret_expr->kind);
            if (!ident || !current_scope_) {
                return false;
            }
            Symbol* ret_sym = current_scope_->lookup(ident->name);
            if (!ret_sym) {
                return false;
            }

            bool found = false;
            for (const auto& [sym, reg] : plan.bindings) {
                if (sym == ret_sym) {
                    return_reg = reg;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        break_patches_.push_back({});
        continue_patches_.push_back({});

        for (const auto& [sym, reg] : plan.bindings) {
            if (init_map) {
                if (auto it = init_map->find(sym); it != init_map->end()) {
                    if (!generate_pure_expr_into(*it->second, reg)) {
                        return false;
                    }
                    continue;
                }
            }
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        if (plan.induction) {
            emit_.mov(plan.induction->term_reg, x64::Reg::RCX);
            if (plan.induction->step != 1) {
                emit_.imul_smart(plan.induction->term_reg, plan.induction->term_reg, plan.induction->step);
            }
            if (plan.induction->disp != 0) {
                emit_.add_smart(plan.induction->term_reg, plan.induction->disp);
            }

            if (return_reg == x64::Reg::RAX) {
                emit_.mov(x64::Reg::R8, x64::Reg::RCX);
                emit_.mov_smart(x64::Reg::RCX, plan.limit);
                emit_.sub(x64::Reg::RCX, x64::Reg::R8);
                emit_.cmp_smart_imm(x64::Reg::RCX, 0);
                std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_LE);

                emit_.mov(x64::Reg::R9, x64::Reg::RCX);
                emit_.imul(plan.induction->term_reg, x64::Reg::RCX);

                if (plan.induction->step != 0) {
                    emit_.mov(x64::Reg::R10, x64::Reg::R9);
                    emit_.dec(x64::Reg::R10);
                    emit_.imul(x64::Reg::R10, x64::Reg::R9);
                    emit_.sar_imm(x64::Reg::R10, 1);
                    emit_.imul_smart(x64::Reg::R10, x64::Reg::R10, plan.induction->step);
                    emit_.add(plan.induction->term_reg, x64::Reg::R10);
                }

                emit_.add(x64::Reg::RAX, plan.induction->term_reg);
                emit_.patch_jump(exit_patch);
                emit_.epilogue();
                return true;
            }
        }

        push_scope();
        for (const auto& [sym, reg] : plan.bindings) {
            register_bindings_[sym] = reg;
        }
        if (plan.induction) {
            InductionBinding binding{
                .term_reg = plan.induction->term_reg,
                .step = plan.induction->step,
            };
            accumulator_inductions_[plan.induction->accum_sym] = binding;
            counter_inductions_[plan.bindings.front().first] = binding;
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        if (!generate_stmt_list(while_stmt.body)) {
            for (const auto& [sym, _] : plan.bindings) {
                register_bindings_.erase(sym);
            }
            if (plan.induction) {
                accumulator_inductions_.erase(plan.induction->accum_sym);
                counter_inductions_.erase(plan.bindings.front().first);
            }
            pop_scope();
            continue_patches_.pop_back();
            break_patches_.pop_back();
            return false;
        }

        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        std::size_t exit_pos = emit_.buffer().pos();
        emit_.patch_jump(exit_patch);
        patch_jumps_to(break_patches_.back(), exit_pos);
        break_patches_.pop_back();

        if (plan.induction) {
            accumulator_inductions_.erase(plan.induction->accum_sym);
            counter_inductions_.erase(plan.bindings.front().first);
        }
        for (const auto& [sym, _] : plan.bindings) {
            register_bindings_.erase(sym);
        }
        pop_scope();

        if (!ret_expr) {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        } else if (return_reg != x64::Reg::RAX) {
            emit_.mov(x64::Reg::RAX, return_reg);
        }
        emit_.epilogue();
        return true;
    }

    [[nodiscard]] bool generate_while_registerized(const ast::WhileStmt& while_stmt, const WhileRegisterPlan& plan) {
        break_patches_.push_back({});
        continue_patches_.push_back({});

        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_load(reg, x64::Reg::RBP, sym->stack_offset);
        }

        if (plan.induction) {
            emit_.mov(plan.induction->term_reg, x64::Reg::RCX);
            if (plan.induction->step != 1) {
                emit_.imul_smart(plan.induction->term_reg, plan.induction->term_reg, plan.induction->step);
            }
            if (plan.induction->disp != 0) {
                emit_.add_smart(plan.induction->term_reg, plan.induction->disp);
            }
        }

        push_scope();
        for (const auto& [sym, reg] : plan.bindings) {
            register_bindings_[sym] = reg;
        }
        if (plan.induction) {
            InductionBinding binding{
                .term_reg = plan.induction->term_reg,
                .step = plan.induction->step,
            };
            accumulator_inductions_[plan.induction->accum_sym] = binding;
            counter_inductions_[plan.bindings.front().first] = binding;
        }

        std::size_t loop_start = emit_.buffer().pos();
        emit_.cmp_smart_imm(x64::Reg::RCX, plan.limit);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);

        if (!generate_stmt_list(while_stmt.body)) {
            for (const auto& [sym, _] : plan.bindings) {
                register_bindings_.erase(sym);
            }
            if (plan.induction) {
                accumulator_inductions_.erase(plan.induction->accum_sym);
                counter_inductions_.erase(plan.bindings.front().first);
            }
            pop_scope();
            continue_patches_.pop_back();
            break_patches_.pop_back();
            return false;
        }

        for (const auto& [sym, _] : plan.bindings) {
            register_bindings_.erase(sym);
        }
        if (plan.induction) {
            accumulator_inductions_.erase(plan.induction->accum_sym);
            counter_inductions_.erase(plan.bindings.front().first);
        }
        pop_scope();

        patch_jumps_to(continue_patches_.back(), loop_start);
        continue_patches_.pop_back();

        std::int32_t rel = static_cast<std::int32_t>(loop_start - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(rel);

        std::size_t exit_pos = emit_.buffer().pos();
        emit_.patch_jump(exit_patch);
        patch_jumps_to(break_patches_.back(), exit_pos);
        break_patches_.pop_back();

        for (const auto& [sym, reg] : plan.bindings) {
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, reg);
        }

        return true;
    }

    [[nodiscard]] bool generate_if(const ast::IfStmt& if_stmt) {
        std::size_t jz_patch = 0;
        if (!emit_condition_false_jump(*if_stmt.condition, jz_patch)) return false;

        push_scope();
        if (!generate_stmt_list(if_stmt.then_block)) {
            pop_scope();
            return false;
        }
        pop_scope();

        if (if_stmt.has_else()) {
            std::size_t jmp_patch = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(jz_patch);

            push_scope();
            if (!generate_stmt_list(*if_stmt.else_block)) {
                pop_scope();
                return false;
            }
            pop_scope();

            emit_.patch_jump(jmp_patch);
        } else {
            emit_.patch_jump(jz_patch);
        }

        return true;
    }

    [[nodiscard]] bool generate_if_expr(const ast::IfExpr& if_expr) {
        if (!if_expr.else_block) {
            error("if expression requires an else branch");
            return false;
        }

        std::size_t jz_patch = 0;
        if (!emit_condition_false_jump(*if_expr.condition, jz_patch)) return false;

        push_scope();
        if (!generate_stmt_suite_value(if_expr.then_block)) {
            pop_scope();
            return false;
        }
        pop_scope();

        std::size_t done_patch = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(jz_patch);

        push_scope();
        if (!generate_stmt_suite_value(*if_expr.else_block)) {
            pop_scope();
            return false;
        }
        pop_scope();

        emit_.patch_jump(done_patch);
        return true;
    }

    [[nodiscard]] bool generate_block_expr(const ast::BlockExpr& block_expr) {
        push_scope();
        for (const auto& stmt : block_expr.block.stmts) {
            if (!generate_stmt(*stmt)) {
                pop_scope();
                return false;
            }
        }

        if (block_expr.block.result) {
            if (!generate_expr(*block_expr.block.result)) {
                pop_scope();
                return false;
            }
        } else {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }

        pop_scope();
        return true;
    }

    [[nodiscard]] bool generate_while(const ast::WhileStmt& while_stmt) {
        WhileSimdKernelPlan simd_plan;
        if (try_make_while_simd_kernel_plan(while_stmt, simd_plan)) {
            return generate_while_simd_kernel_registerized(simd_plan);
        }

        WhileMultiStatePlan multi_plan;
        if (try_make_while_multistate_plan(while_stmt, multi_plan)) {
            return generate_while_multistate_registerized(multi_plan);
        }

        WhileRegisterPlan plan;
        if (try_make_while_register_plan(while_stmt, plan)) {
            return generate_while_registerized(while_stmt, plan);
        }

        WhileBoundPlan bound_plan;
        if (try_make_while_bound_plan(while_stmt, bound_plan)) {
            return generate_while_bound_registerized(while_stmt, bound_plan);
        }

        break_patches_.push_back({});
        continue_patches_.push_back({});
        std::size_t loop_start = emit_.buffer().pos();

        std::size_t jz_patch = 0;
        if (!emit_condition_false_jump(*while_stmt.condition, jz_patch)) return false;

        // Body (with new scope)
        push_scope();
        if (!generate_stmt_list(while_stmt.body)) {
            pop_scope();
            return false;
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

        if (!generate_stmt_list(loop_stmt.body)) {
            pop_scope();
            return false;
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
        if (!generate_stmt_list(for_stmt.body)) {
            pop_scope();
            return false;
        }
        
        // increment - continue jumps here (skip body, do increment, loop back)
        std::size_t increment_pos = emit_.buffer().pos();
        patch_jumps_to(continue_patches_.back(), increment_pos);
        continue_patches_.pop_back();
        
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym.stack_offset);
        emit_.add_smart(x64::Reg::RAX, 1);
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

    [[nodiscard]] static bool try_get_i64_immediate(const ast::LiteralExpr& lit, std::int64_t& value) {
        if (auto* i = std::get_if<std::int64_t>(&lit.value)) {
            value = *i;
            return true;
        }
        if (auto* b = std::get_if<bool>(&lit.value)) {
            value = *b ? 1 : 0;
            return true;
        }
        if (auto* c = std::get_if<char32_t>(&lit.value)) {
            value = static_cast<std::int64_t>(*c);
            return true;
        }
        if (auto* u = std::get_if<std::uint64_t>(&lit.value)) {
            if (*u <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
                value = static_cast<std::int64_t>(*u);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static bool try_get_i64_immediate(const ast::Expr& expr, std::int64_t& value) {
        if (auto* lit = std::get_if<ast::LiteralExpr>(&expr.kind)) {
            return try_get_i64_immediate(*lit, value);
        }
        return false;
    }

    [[nodiscard]] static bool supports_binary_rhs_imm(ast::BinaryExpr::Op op, std::int64_t imm) {
        using Op = ast::BinaryExpr::Op;
        const bool fits_i32 =
            imm >= (std::numeric_limits<std::int32_t>::min)() &&
            imm <= (std::numeric_limits<std::int32_t>::max)();
        switch (op) {
            case Op::Add:
            case Op::Mul:
            case Op::Shl:
            case Op::Shr:
            case Op::Eq:
            case Op::Ne:
            case Op::Lt:
            case Op::Gt:
            case Op::Le:
            case Op::Ge:
                return true;
            case Op::Sub:
                return imm != (std::numeric_limits<std::int64_t>::min)();
            case Op::Div:
            case Op::Mod:
                return imm == 1;
            case Op::BitAnd:
            case Op::BitOr:
            case Op::BitXor:
                return fits_i32;
            default:
                return false;
        }
    }

    [[nodiscard]] static std::optional<std::uint8_t> false_branch_cc(ast::BinaryExpr::Op op) {
        using Op = ast::BinaryExpr::Op;
        switch (op) {
            case Op::Eq:
                return x64::Emitter::CC_NE;
            case Op::Ne:
                return x64::Emitter::CC_E;
            case Op::Lt:
                return x64::Emitter::CC_GE;
            case Op::Gt:
                return x64::Emitter::CC_LE;
            case Op::Le:
                return x64::Emitter::CC_G;
            case Op::Ge:
                return x64::Emitter::CC_L;
            default:
                return std::nullopt;
        }
    }

    [[nodiscard]] bool emit_condition_false_jump(const ast::Expr& condition, std::size_t& jump_patch) {
        if (auto* bin = std::get_if<ast::BinaryExpr>(&condition.kind)) {
            if (auto false_cc = false_branch_cc(bin->op)) {
                std::int64_t rhs_imm = 0;
                if (try_get_i64_immediate(*bin->rhs, rhs_imm)) {
                    if (current_scope_ && bin->lhs->is<ast::IdentExpr>() &&
                        rhs_imm >= (std::numeric_limits<std::int32_t>::min)() &&
                        rhs_imm <= (std::numeric_limits<std::int32_t>::max)()) {
                        if (Symbol* sym = current_scope_->lookup(bin->lhs->as<ast::IdentExpr>().name)) {
                            if (auto bound = lookup_bound_reg(sym)) {
                                emit_.cmp_smart_imm(*bound, rhs_imm);
                                jump_patch = emit_.jcc_rel32(*false_cc);
                                return true;
                            }
                            emit_.cmp_mem_imm(x64::Reg::RBP, sym->stack_offset, static_cast<std::int32_t>(rhs_imm));
                            jump_patch = emit_.jcc_rel32(*false_cc);
                            return true;
                        }
                    }
                    if (can_generate_pure_expr(*bin->lhs)) {
                        auto lhs_reg = pick_scratch_reg();
                        if (!lhs_reg) return false;
                        if (!generate_pure_expr_into(*bin->lhs, *lhs_reg)) return false;
                        emit_.cmp_smart_imm(*lhs_reg, rhs_imm);
                    } else {
                        if (!generate_expr(*bin->lhs)) return false;
                        emit_.cmp_smart_imm(x64::Reg::RAX, rhs_imm);
                    }
                    jump_patch = emit_.jcc_rel32(*false_cc);
                    return true;
                }

                if (bin->rhs->is<ast::IdentExpr>()) {
                    if (can_generate_pure_expr(*bin->lhs)) {
                        auto lhs_reg = pick_scratch_reg();
                        if (!lhs_reg) return false;
                        auto rhs_reg = pick_scratch_reg({*lhs_reg});
                        if (!rhs_reg) return false;
                        if (!generate_pure_expr_into(*bin->lhs, *lhs_reg)) return false;
                        if (!generate_ident_into(bin->rhs->as<ast::IdentExpr>(), *rhs_reg)) return false;
                        emit_.cmp(*lhs_reg, *rhs_reg);
                        jump_patch = emit_.jcc_rel32(*false_cc);
                        return true;
                    }

                    if (!generate_expr(*bin->lhs)) return false;
                    auto rhs_reg = pick_scratch_reg({x64::Reg::RAX});
                    if (!rhs_reg) return false;
                    if (!generate_ident_into(bin->rhs->as<ast::IdentExpr>(), *rhs_reg)) return false;
                    emit_.cmp(x64::Reg::RAX, *rhs_reg);
                    jump_patch = emit_.jcc_rel32(*false_cc);
                    return true;
                }

                if (can_generate_pure_expr(*bin->lhs) && can_generate_pure_expr(*bin->rhs)) {
                    auto lhs_reg = pick_scratch_reg();
                    if (!lhs_reg) return false;
                    auto rhs_reg = pick_scratch_reg({*lhs_reg});
                    if (!rhs_reg) return false;
                    if (!generate_pure_expr_into(*bin->lhs, *lhs_reg)) return false;
                    if (!generate_pure_expr_into(*bin->rhs, *rhs_reg)) return false;
                    emit_.cmp(*lhs_reg, *rhs_reg);
                    jump_patch = emit_.jcc_rel32(*false_cc);
                    return true;
                }

                if (!generate_expr(*bin->lhs)) return false;
                emit_.push(x64::Reg::RAX);
                if (!generate_expr(*bin->rhs)) return false;
                auto rhs_reg = pick_scratch_reg({x64::Reg::RAX});
                if (!rhs_reg) return false;
                if (*rhs_reg != x64::Reg::RAX) {
                    emit_.mov(*rhs_reg, x64::Reg::RAX);
                }
                emit_.pop(x64::Reg::RAX);
                emit_.cmp(x64::Reg::RAX, *rhs_reg);
                jump_patch = emit_.jcc_rel32(*false_cc);
                return true;
            }
        }

        if (can_generate_pure_expr(condition)) {
            auto cond_reg = pick_scratch_reg();
            if (!cond_reg) return false;
            if (!generate_pure_expr_into(condition, *cond_reg)) return false;
            emit_.test(*cond_reg, *cond_reg);
        } else {
            if (!generate_expr(condition)) return false;
            emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        }
        jump_patch = emit_.jcc_rel32(x64::Emitter::CC_E);
        return true;
    }

    void emit_bool_result(std::uint8_t cc, x64::Reg dst = x64::Reg::RAX) {
        emit_.setcc(cc, dst);
        emit_.movzx_byte(dst, dst);
    }

    [[nodiscard]] bool emit_binary_into_with_rhs_imm(x64::Reg dst, ast::BinaryExpr::Op op, std::int64_t imm) {
        using Op = ast::BinaryExpr::Op;
        switch (op) {
            case Op::Add:
                emit_.add_smart(dst, imm);
                return true;
            case Op::Sub:
                emit_.add_smart(dst, -imm);
                return true;
            case Op::Mul:
                if (imm == 0) {
                    emit_.xor_32(dst, dst);
                    return true;
                }
                if (imm == 1) {
                    return true;
                }
                if (imm == -1) {
                    emit_.neg(dst);
                    return true;
                }
                emit_.imul_smart(dst, dst, imm);
                return true;
            case Op::Div:
                return true;
            case Op::Mod:
                emit_.xor_32(dst, dst);
                return true;
            case Op::BitAnd:
                if (imm == 0) {
                    emit_.xor_32(dst, dst);
                    return true;
                }
                if (imm == -1) {
                    return true;
                }
                if (imm >= (std::numeric_limits<std::int32_t>::min)() &&
                    imm <= (std::numeric_limits<std::int32_t>::max)()) {
                    emit_.and_imm(dst, static_cast<std::int32_t>(imm));
                } else {
                    emit_.mov_imm64(x64::Reg::R11, static_cast<std::uint64_t>(imm));
                    emit_.and_(dst, x64::Reg::R11);
                }
                return true;
            case Op::BitOr:
                if (imm == 0) {
                    return true;
                }
                if (imm >= (std::numeric_limits<std::int32_t>::min)() &&
                    imm <= (std::numeric_limits<std::int32_t>::max)()) {
                    emit_.or_imm(dst, static_cast<std::int32_t>(imm));
                } else {
                    emit_.mov_imm64(x64::Reg::R11, static_cast<std::uint64_t>(imm));
                    emit_.or_(dst, x64::Reg::R11);
                }
                return true;
            case Op::BitXor:
                if (imm == 0) {
                    return true;
                }
                if (imm == -1) {
                    emit_.not_(dst);
                    return true;
                }
                if (imm >= (std::numeric_limits<std::int32_t>::min)() &&
                    imm <= (std::numeric_limits<std::int32_t>::max)()) {
                    emit_.xor_imm(dst, static_cast<std::int32_t>(imm));
                } else {
                    emit_.mov_imm64(x64::Reg::R11, static_cast<std::uint64_t>(imm));
                    emit_.xor_(dst, x64::Reg::R11);
                }
                return true;
            case Op::Shl:
                if (imm != 0) {
                    emit_.shl_imm(dst, static_cast<std::uint8_t>(imm));
                }
                return true;
            case Op::Shr:
                if (imm != 0) {
                    emit_.sar_imm(dst, static_cast<std::uint8_t>(imm));
                }
                return true;
            case Op::Eq:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_E, dst);
                return true;
            case Op::Ne:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_NE, dst);
                return true;
            case Op::Lt:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_L, dst);
                return true;
            case Op::Gt:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_G, dst);
                return true;
            case Op::Le:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_LE, dst);
                return true;
            case Op::Ge:
                emit_.cmp_smart_imm(dst, imm);
                emit_bool_result(x64::Emitter::CC_GE, dst);
                return true;
            default:
                error("unsupported binary operator");
                return false;
        }
    }

    [[nodiscard]] bool emit_binary_with_rhs_imm(ast::BinaryExpr::Op op, std::int64_t imm) {
        return emit_binary_into_with_rhs_imm(x64::Reg::RAX, op, imm);
    }

    [[nodiscard]] bool emit_binary_into_with_rhs_reg(x64::Reg dst, ast::BinaryExpr::Op op, x64::Reg rhs) {
        using Op = ast::BinaryExpr::Op;
        switch (op) {
            case Op::Add:
                emit_.add(dst, rhs);
                return true;
            case Op::Sub:
                emit_.sub(dst, rhs);
                return true;
            case Op::Mul:
                emit_.imul(dst, rhs);
                return true;
            case Op::Div:
                if (dst != x64::Reg::RAX) return false;
                emit_.cqo();
                emit_.idiv(rhs);
                return true;
            case Op::Mod:
                if (dst != x64::Reg::RAX) return false;
                emit_.cqo();
                emit_.idiv(rhs);
                emit_.mov(dst, x64::Reg::RDX);
                return true;
            case Op::BitAnd:
                emit_.and_(dst, rhs);
                return true;
            case Op::BitOr:
                emit_.or_(dst, rhs);
                return true;
            case Op::BitXor:
                emit_.xor_(dst, rhs);
                return true;
            case Op::Shl:
                if (rhs != x64::Reg::RCX) {
                    if (dst == x64::Reg::RCX) return false;
                    emit_.mov(x64::Reg::RCX, rhs);
                }
                emit_.shl_cl(dst);
                return true;
            case Op::Shr:
                if (rhs != x64::Reg::RCX) {
                    if (dst == x64::Reg::RCX) return false;
                    emit_.mov(x64::Reg::RCX, rhs);
                }
                emit_.sar_cl(dst);
                return true;
            case Op::Eq:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_E, dst);
                return true;
            case Op::Ne:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_NE, dst);
                return true;
            case Op::Lt:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_L, dst);
                return true;
            case Op::Gt:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_G, dst);
                return true;
            case Op::Le:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_LE, dst);
                return true;
            case Op::Ge:
                emit_.cmp(dst, rhs);
                emit_bool_result(x64::Emitter::CC_GE, dst);
                return true;
            default:
                error("unsupported binary operator");
                return false;
        }
    }

    [[nodiscard]] bool emit_binary_into_with_rhs_mem(x64::Reg dst, ast::BinaryExpr::Op op, x64::Reg base, std::int32_t offset) {
        using Op = ast::BinaryExpr::Op;
        switch (op) {
            case Op::Add:
                emit_.add_mem(dst, base, offset);
                return true;
            case Op::Sub:
                emit_.sub_mem(dst, base, offset);
                return true;
            case Op::Eq:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_E, dst);
                return true;
            case Op::Ne:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_NE, dst);
                return true;
            case Op::Lt:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_L, dst);
                return true;
            case Op::Gt:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_G, dst);
                return true;
            case Op::Le:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_LE, dst);
                return true;
            case Op::Ge:
                emit_.cmp_mem(dst, base, offset);
                emit_bool_result(x64::Emitter::CC_GE, dst);
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool emit_binary_with_rhs_reg(ast::BinaryExpr::Op op, x64::Reg rhs) {
        return emit_binary_into_with_rhs_reg(x64::Reg::RAX, op, rhs);
    }

    [[nodiscard]] bool emit_binary_with_rhs_mem(ast::BinaryExpr::Op op, x64::Reg base, std::int32_t offset) {
        return emit_binary_into_with_rhs_mem(x64::Reg::RAX, op, base, offset);
    }

    [[nodiscard]] bool generate_bound_ident_into(const ast::IdentExpr& ident, x64::Reg dst) {
        Symbol* sym = current_scope_->lookup(ident.name);
        if (sym) {
            if (auto bound = lookup_bound_reg(sym)) {
                if (dst != *bound) {
                    emit_.mov(dst, *bound);
                }
                return true;
            }
            emit_.mov_load(dst, x64::Reg::RBP, sym->stack_offset);
            return true;
        }

        auto it = globals_.find(ident.name);
        if (it != globals_.end()) {
            const auto& gv = it->second;

            if (has_global_slot_) {
                if (!emit_load_global_base_ptr(dst)) return false;
                emit_.mov_load(dst, dst, static_cast<std::int32_t>(gv.offset));
                return true;
            }

            if (gv.init && gv.init->is<ast::LiteralExpr>()) {
                const auto& lit = gv.init->as<ast::LiteralExpr>();
                std::int64_t imm = 0;
                if (try_get_i64_immediate(lit, imm)) {
                    emit_.mov_smart(dst, imm);
                    return true;
                }
            }

            emit_.mov_smart(dst, 0);
            return true;
        }

        error_undefined_name("variable", ident.name, collect_value_name_candidates());
        return false;
    }

    [[nodiscard]] bool generate_pure_field_into(const ast::FieldExpr& field, x64::Reg dst) {
        if (auto qualified = get_qualified_name(field)) {
            if (auto resolved_fn = resolve_function_alias_name(*qualified)) {
                std::size_t fn_addr_fixup = emit_lea_rip_disp32(dst);
                call_fixups_.push_back({fn_addr_fixup, *resolved_fn});
                return true;
            }
            if (auto global_name = resolve_global_alias(*qualified)) {
                ast::IdentExpr ident{.name = *global_name};
                return generate_bound_ident_into(ident, dst);
            }
        }

        if (auto base_name = get_qualified_name(*field.base)) {
            auto enum_it = enums_.find(resolve_enum_name(*base_name));
            if (enum_it != enums_.end()) {
                auto variant_it = enum_it->second.find(field.field);
                if (variant_it != enum_it->second.end()) {
                    emit_.mov_smart(dst, variant_it->second);
                    return true;
                }
                error(std::format("enum '{}' has no variant '{}'", enum_it->first, field.field));
                return false;
            }
        }

        std::string struct_name;
        Symbol* base_sym = nullptr;

        if (field.base->is<ast::IdentExpr>()) {
            const auto& ident = field.base->as<ast::IdentExpr>();
            Symbol* sym = current_scope_->lookup(ident.name);
            if (sym) {
                auto sn = get_struct_name(sym->type);
                if (!sn) {
                    error(std::format("variable '{}' is not a struct type", ident.name));
                    return false;
                }
                base_sym = sym;
                struct_name = *sn;
                if (!generate_bound_ident_into(ident, dst)) {
                    return false;
                }
            } else if (current_scope_) {
                if (const ast::Expr* inline_expr = current_scope_->lookup_inline(ident.name)) {
                    auto inline_type = infer_expr_type(*inline_expr);
                    if (!inline_type) {
                        error_undefined_name("variable", ident.name, collect_value_name_candidates());
                        return false;
                    }
                    auto sn = get_struct_name(*inline_type);
                    if (!sn) {
                        error(std::format("variable '{}' is not a struct type", ident.name));
                        return false;
                    }
                    struct_name = *sn;
                    if (!generate_pure_expr_into(*inline_expr, dst)) {
                        return false;
                    }
                } else {
                    error_undefined_name("variable", ident.name, collect_value_name_candidates());
                    return false;
                }
            } else {
                error_undefined_name("variable", ident.name, collect_value_name_candidates());
                return false;
            }
        } else if (field.base->is<ast::FieldExpr>()) {
            auto base_type = resolve_field_type(field.base->as<ast::FieldExpr>());
            if (!base_type) {
                error("cannot resolve type of chained field access");
                return false;
            }
            struct_name = *base_type;
            if (!generate_pure_field_into(field.base->as<ast::FieldExpr>(), dst)) {
                return false;
            }
        } else {
            return false;
        }

        auto struct_it = structs_.find(struct_name);
        if (struct_it == structs_.end()) {
            error_unknown_type("struct type", struct_name);
            return false;
        }

        auto offset_opt = struct_it->second.get_field_offset(field.field);
        if (!offset_opt) {
            error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
            return false;
        }

        std::int32_t offset = static_cast<std::int32_t>(*offset_opt);
        if (base_sym) {
            if (auto bound = lookup_bound_reg(base_sym)) {
                emit_.mov_load(dst, *bound, offset);
                return true;
            }
            emit_.mov_load(dst, x64::Reg::RBP, base_sym->stack_offset);
            emit_.mov_load(dst, dst, offset);
            return true;
        }

        emit_.mov_load(dst, dst, offset);
        return true;
    }

    [[nodiscard]] bool generate_pure_expr_into(const ast::Expr& expr, x64::Reg dst) {
        if (auto* lit = std::get_if<ast::LiteralExpr>(&expr.kind)) {
            std::int64_t imm = 0;
            if (try_get_i64_immediate(*lit, imm)) {
                emit_.mov_smart(dst, imm);
                return true;
            }
            if (auto* u = std::get_if<std::uint64_t>(&lit->value)) {
                emit_.mov_imm64(dst, *u);
                return true;
            }
            if (auto* d = std::get_if<double>(&lit->value)) {
                emit_.mov_imm64(dst, std::bit_cast<std::uint64_t>(*d));
                return true;
            }
            if (std::holds_alternative<std::string>(lit->value)) {
                if (dst == x64::Reg::RAX) {
                    return generate_literal(*lit);
                }
                emit_.push(x64::Reg::RAX);
                if (!generate_literal(*lit)) return false;
                emit_.mov(dst, x64::Reg::RAX);
                emit_.pop(x64::Reg::RAX);
                return true;
            }
            return false;
        }

        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            if (current_scope_) {
                if (const ast::Expr* inline_expr = current_scope_->lookup_inline(ident->name)) {
                    if (generate_pure_expr_into(*inline_expr, dst)) {
                        return true;
                    }
                }
            }
            return generate_bound_ident_into(*ident, dst);
        }

        if (auto* field = std::get_if<ast::FieldExpr>(&expr.kind)) {
            return generate_pure_field_into(*field, dst);
        }

        if (auto* un = std::get_if<ast::UnaryExpr>(&expr.kind)) {
            if (!generate_pure_expr_into(*un->operand, dst)) return false;
            using Op = ast::UnaryExpr::Op;
            switch (un->op) {
                case Op::Neg:
                    emit_.neg(dst);
                    return true;
                case Op::Not:
                    emit_.test(dst, dst);
                    emit_bool_result(x64::Emitter::CC_E, dst);
                    return true;
                case Op::BitNot:
                    emit_.not_(dst);
                    return true;
                default:
                    return false;
            }
        }

        if (auto* bin = std::get_if<ast::BinaryExpr>(&expr.kind)) {
            std::int64_t rhs_imm = 0;
            if (try_get_i64_immediate(*bin->rhs, rhs_imm) && supports_binary_rhs_imm(bin->op, rhs_imm)) {
                if (!generate_pure_expr_into(*bin->lhs, dst)) return false;
                return emit_binary_into_with_rhs_imm(dst, bin->op, rhs_imm);
            }

            auto rhs_reg = pick_scratch_reg({dst});
            if (!rhs_reg) {
                return false;
            }

            if (bin->rhs->is<ast::FieldExpr>()) {
                const auto& field = bin->rhs->as<ast::FieldExpr>();
                if (field.base->is<ast::IdentExpr>()) {
                    const auto& var_name = field.base->as<ast::IdentExpr>().name;
                    if (Symbol* sym = current_scope_->lookup(var_name)) {
                        if (auto sn = get_struct_name(sym->type)) {
                            if (auto struct_it = structs_.find(*sn); struct_it != structs_.end()) {
                                if (auto offset_opt = struct_it->second.get_field_offset(field.field)) {
                                    if (auto bound = lookup_bound_reg(sym)) {
                                        if (!generate_pure_expr_into(*bin->lhs, dst)) return false;
                                        if (emit_binary_into_with_rhs_mem(dst, bin->op, *bound, static_cast<std::int32_t>(*offset_opt))) {
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!generate_pure_expr_into(*bin->rhs, *rhs_reg)) return false;
            if (!generate_pure_expr_into(*bin->lhs, dst)) return false;
            return emit_binary_into_with_rhs_reg(dst, bin->op, *rhs_reg);
        }

        return false;
    }

    [[nodiscard]] bool generate_ident_into(const ast::IdentExpr& ident, x64::Reg dst) {
        if (current_scope_) {
            if (const ast::Expr* inline_expr = current_scope_->lookup_inline(ident.name)) {
                if (can_generate_pure_expr(*inline_expr) && generate_pure_expr_into(*inline_expr, dst)) {
                    return true;
                }
                if (dst == x64::Reg::RAX) {
                    return generate_expr(*inline_expr);
                }
                emit_.push(x64::Reg::RAX);
                if (!generate_expr(*inline_expr)) return false;
                emit_.mov(dst, x64::Reg::RAX);
                emit_.pop(x64::Reg::RAX);
                return true;
            }
        }

        return generate_bound_ident_into(ident, dst);
    }

    [[nodiscard]] bool generate_expr(const ast::Expr& expr) {
        // optimizer integration disabled for now - will add back when patterns are implemented
        // auto hash = optimizer::hash_expr(&expr);
        // auto* pattern = optimizer::find_pattern(hash);
        // if (pattern && pattern->match(&expr)) {
        //     // save checkpoint
        //     auto checkpoint_ptr = emit_.buffer().data() + emit_.buffer().pos();
        //     auto checkpoint_size = emit_.buffer().pos();
        //     
        //     // try to emit optimized code
        //     optimizer::Emit opt_ctx{
        //         .code_ptr = emit_.buffer().data() + emit_.buffer().pos(),
        //         .code_size = emit_.buffer().pos(),
        //         .code_cap = 0,  // not used for now
        //         .checkpoint_ptr = checkpoint_ptr,
        //         .checkpoint_size = checkpoint_size
        //     };
        //     
        //     auto* result = pattern->emit(&opt_ctx, &expr);
        //     if (result) {
        //         // success - manually emit the bytes that were written
        //         // the pattern wrote directly to code_ptr, but we need to update our buffer
        //         // for now just fall through since patterns return nullptr anyway
        //     }
        //     // failed - rollback and fall through to normal codegen
        // }
        
        // normal codegen fallback
        return std::visit(overloaded{
            [&](const ast::LiteralExpr& e) { return generate_literal(e); },
            [&](const ast::IdentExpr& e)   { return generate_ident(e); },
            [&](const ast::BinaryExpr& e)  { return generate_binary(e); },
            [&](const ast::UnaryExpr& e)   { return generate_unary(e); },
            [&](const ast::CallExpr& e)    { return generate_call(e); },
            [&](const ast::IndexExpr& e)   { return generate_index(e); },
            [&](const ast::FieldExpr& e)   { return generate_field(e); },
            [&](const ast::CastExpr& e)    {
                if (!generate_expr(*e.expr)) {
                    return false;
                }
                return emit_coerce_rax_to_type(e.target_type, e.expr.get());
            },
            [&](const ast::ArrayExpr& e)   { return generate_array_literal(e); },
            [&](const ast::StructExpr& e)  { return generate_struct_literal(e); },
            [&](const ast::IfExpr& e)      { return generate_if_expr(e); },
            [&](const ast::BlockExpr& e)   { return generate_block_expr(e); },
            [&](const ast::SpawnExpr& e)   { return generate_spawn(e); },
            [&](const ast::AwaitExpr& e)   { return generate_await(e); },
            [&](const ast::AtomicLoadExpr& e){ return generate_atomic_load(e); },
            [&](const ast::AtomicStoreExpr& e){ return generate_atomic_store(e); },
            [&](const ast::AtomicAddExpr& e){ return generate_atomic_add(e); },
            [&](const ast::AtomicCompareExchangeExpr& e){ return generate_atomic_compare_exchange(e); },
            [&](const auto&) -> bool {
                error("unknown expression type");
                return false;
            },
        }, expr.kind);
    }

    // resolve the struct type name that a field expression evaluates to
    // e.g. for `outer.inner` where outer is Outer and inner is of type Inner, returns "Inner"
    std::optional<std::string> resolve_field_type(const ast::FieldExpr& field) {
        auto field_type = lookup_field_expr_type(field);
        if (!field_type) {
            return std::nullopt;
        }
        return get_struct_name(*field_type);
    }

    [[nodiscard]] bool generate_field(const ast::FieldExpr& field) {
        if (auto qualified = get_qualified_name(field)) {
            if (auto resolved_fn = resolve_function_alias_name(*qualified)) {
                std::size_t fn_addr_fixup = emit_lea_rip_disp32(x64::Reg::RAX);
                call_fixups_.push_back({fn_addr_fixup, *resolved_fn});
                return true;
            }
            if (auto global_name = resolve_global_alias(*qualified)) {
                ast::IdentExpr ident{.name = *global_name};
                return generate_ident(ident);
            }
        }

        // check if this is an enum variant access (EnumName.Variant or module.EnumName.Variant)
        if (auto base_name = get_qualified_name(*field.base)) {
            auto enum_it = enums_.find(resolve_enum_name(*base_name));
            if (enum_it != enums_.end()) {
                auto variant_it = enum_it->second.find(field.field);
                if (variant_it != enum_it->second.end()) {
                    emit_.mov_smart(x64::Reg::RAX, variant_it->second);
                    return true;
                }
                error(std::format("enum '{}' has no variant '{}'", enum_it->first, field.field));
                return false;
            }
        }
        
        // resolve the struct type of the base expression
        std::string struct_name;
        Symbol* base_sym = nullptr;
        
        if (field.base->is<ast::IdentExpr>()) {
            const std::string& var_name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(var_name);
            if (!sym) {
                error_undefined_name("variable", var_name, collect_value_name_candidates());
                return false;
            }
            auto sn = get_struct_name(sym->type);
            if (!sn) {
                error(std::format("variable '{}' is not a struct type", var_name));
                return false;
            }
            struct_name = *sn;
            base_sym = sym;
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
            error_unknown_type("struct type", struct_name);
            return false;
        }
        
        auto offset_opt = struct_it->second.get_field_offset(field.field);
        if (!offset_opt) {
            error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
            return false;
        }
        
        std::int32_t offset = static_cast<std::int32_t>(*offset_opt);
        
        if (base_sym) {
            if (auto bound = lookup_bound_reg(base_sym)) {
                emit_.mov_load(x64::Reg::RAX, *bound, offset);
                return true;
            }
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, base_sym->stack_offset);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, offset);
            return true;
        }

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, offset);
        
        return true;
    }

    [[nodiscard]] bool generate_struct_literal(const ast::StructExpr& lit) {
        // Look up struct definition
        std::string resolved_name = resolve_type_name(lit.name);
        auto it = structs_.find(resolved_name);
        if (it == structs_.end()) {
            error_unknown_type("struct/class", lit.name);
            return false;
        }
        
        const StructInfo& info = it->second;
        
        // Allocate memory for struct
        std::size_t size = info.total_size > 0 ? info.total_size : 8;  // min 8 bytes
        
        if (is_native_image_output()) {
            // Use HeapAlloc like generate_dll_malloc
            emit_.sub_imm(x64::Reg::RSP, 32);
            
            emit_iat_call_raw(pe::iat::GetProcessHeap);
            
            emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap -> RCX
            emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
            emit_.mov_imm32(x64::Reg::R8, static_cast<std::int32_t>(size));  // size -> R8
            
            emit_iat_call_raw(pe::iat::HeapAlloc);
            
            emit_.add_smart(x64::Reg::RSP, 32);
        } else if (rt_.malloc_fn) {
            emit_.mov_imm32(x64::Reg::RCX, static_cast<std::int32_t>(size));
            emit_.sub_imm(x64::Reg::RSP, 32);
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.malloc_fn));
            emit_.call(x64::Reg::RAX);
            emit_.add_smart(x64::Reg::RSP, 32);
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
                emit_.add_smart(x64::Reg::RAX, static_cast<std::int32_t>(offset));
            }
            emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);
        }
        
        // Pop struct pointer back to RAX as result
        emit_.pop(x64::Reg::RAX);
        
        return true;
    }

    [[nodiscard]] bool generate_array_literal(const ast::ArrayExpr& arr) {
        if (!emit_array_new_count(arr.elements.size())) {
            return false;
        }

        emit_.push(x64::Reg::RAX); // keep array pointer on stack

        for (std::size_t i = 0; i < arr.elements.size(); ++i) {
            if (!generate_expr(*arr.elements[i])) {
                emit_.pop(x64::Reg::RAX);
                return false;
            }

            emit_.mov(x64::Reg::R8, x64::Reg::RAX);
            emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 0);
            if (!emit_array_store_imm(x64::Reg::RCX, i, x64::Reg::R8)) {
                emit_.pop(x64::Reg::RAX);
                return false;
            }
        }

        if (is_native_image_output()) {
            emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 0);
            emit_.mov_imm32(x64::Reg::RAX, static_cast<std::int32_t>(arr.elements.size()));
            emit_.mov_store(x64::Reg::RCX, -16, x64::Reg::RAX);
        }

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
        // Pop base into RCX
        emit_.pop(x64::Reg::RCX);
        
        // Add: base + (index * 8)
        emit_.lea_scaled(x64::Reg::RAX, x64::Reg::RCX, x64::Reg::RAX, 8);
        if (!is_native_image_output()) {
            emit_.add_smart(x64::Reg::RAX, 8); // host fallback arrays store length at arr[0]
        }
        
        // Dereference: load value at that address
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        
        return true;
    }

    [[nodiscard]] bool generate_literal(const ast::LiteralExpr& lit) {
        if (auto* i = std::get_if<std::int64_t>(&lit.value)) {
            emit_.mov_smart(x64::Reg::RAX, *i);
            return true;
        }
        if (auto* u = std::get_if<std::uint64_t>(&lit.value)) {
            emit_.mov_imm64(x64::Reg::RAX, *u);
            return true;
        }
        if (auto* b = std::get_if<bool>(&lit.value)) {
            emit_.mov_smart(x64::Reg::RAX, *b ? 1 : 0);
            return true;
        }
        if (auto* d = std::get_if<double>(&lit.value)) {
            emit_.mov_imm64(x64::Reg::RAX, std::bit_cast<std::uint64_t>(*d));
            return true;
        }
        if (auto* c = std::get_if<char32_t>(&lit.value)) {
            // Char as integer
            emit_.mov_smart(x64::Reg::RAX, static_cast<std::int64_t>(*c));
            return true;
        }
        if (auto* s = std::get_if<std::string>(&lit.value)) {
            // String literal handling
            
            if (is_native_image_output()) {
                // in native-image output we need to embed the string inline
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
                emit_.mov_smart(x64::Reg::RAX, 0);
            }
            return true;
        }
        
        error("unsupported literal type");
        return false;
    }

    [[nodiscard]] bool generate_ident(const ast::IdentExpr& ident) {
        return generate_ident_into(ident, x64::Reg::RAX);
    }

    [[nodiscard]] bool generate_binary(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;

        // try constant folding - if both operands are literals, compute at compile time
        if (auto* lhs_lit = std::get_if<ast::LiteralExpr>(&bin.lhs->kind)) {
            if (auto* rhs_lit = std::get_if<ast::LiteralExpr>(&bin.rhs->kind)) {
                // both are literals - try to fold
                if (auto* lhs_val = std::get_if<std::int64_t>(&lhs_lit->value)) {
                    if (auto* rhs_val = std::get_if<std::int64_t>(&rhs_lit->value)) {
                        std::int64_t result = 0;
                        bool folded = false;
                        
                        switch (bin.op) {
                            case Op::Add: result = *lhs_val + *rhs_val; folded = true; break;
                            case Op::Sub: result = *lhs_val - *rhs_val; folded = true; break;
                            case Op::Mul: result = *lhs_val * *rhs_val; folded = true; break;
                            case Op::Div: if (*rhs_val != 0) { result = *lhs_val / *rhs_val; folded = true; } break;
                            case Op::Mod: if (*rhs_val != 0) { result = *lhs_val % *rhs_val; folded = true; } break;
                            case Op::BitAnd: result = *lhs_val & *rhs_val; folded = true; break;
                            case Op::BitOr: result = *lhs_val | *rhs_val; folded = true; break;
                            case Op::BitXor: result = *lhs_val ^ *rhs_val; folded = true; break;
                            case Op::Shl: result = *lhs_val << *rhs_val; folded = true; break;
                            case Op::Shr: result = *lhs_val >> *rhs_val; folded = true; break;
                            default: break;
                        }
                        
                        if (folded) {
                            emit_.mov_smart(x64::Reg::RAX, result);
                            return true;
                        }
                    }
                }
            }
        }

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
            emit_bool_result(x64::Emitter::CC_NE);
            std::size_t done = emit_.jmp_rel32_placeholder();
            emit_.patch_jump(skip_rhs);
            emit_.mov_imm32(x64::Reg::RAX, 1); // result = 1
            emit_.patch_jump(done);
            return true;
        }

        std::int64_t rhs_imm = 0;
        if (try_get_i64_immediate(*bin.rhs, rhs_imm) && supports_binary_rhs_imm(bin.op, rhs_imm)) {
            if (!generate_expr(*bin.lhs)) return false;
            return emit_binary_with_rhs_imm(bin.op, rhs_imm);
        }

        if (bin.rhs->is<ast::IdentExpr>()) {
            if (!generate_expr(*bin.lhs)) return false;
            auto rhs_reg = pick_scratch_reg({x64::Reg::RAX});
            if (!rhs_reg) return false;
            if (!generate_ident_into(bin.rhs->as<ast::IdentExpr>(), *rhs_reg)) return false;
            return emit_binary_with_rhs_reg(bin.op, *rhs_reg);
        }

        if (bin.rhs->is<ast::FieldExpr>()) {
            const auto& field = bin.rhs->as<ast::FieldExpr>();
            if (field.base->is<ast::IdentExpr>()) {
                const auto& var_name = field.base->as<ast::IdentExpr>().name;
                if (Symbol* sym = current_scope_->lookup(var_name)) {
                    if (auto sn = get_struct_name(sym->type)) {
                        if (auto struct_it = structs_.find(*sn); struct_it != structs_.end()) {
                            if (auto offset_opt = struct_it->second.get_field_offset(field.field)) {
                                if (auto bound = lookup_bound_reg(sym)) {
                                    if (!generate_expr(*bin.lhs)) return false;
                                    if (emit_binary_with_rhs_mem(bin.op, *bound, static_cast<std::int32_t>(*offset_opt))) {
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Generate left operand -> RAX
        if (!generate_expr(*bin.lhs)) return false;
        
        // Save to stack temporarily
        emit_.push(x64::Reg::RAX);

        // Generate right operand -> RAX
        if (!generate_expr(*bin.rhs)) return false;

        // Move right to a scratch reg, pop left to RAX
        auto rhs_reg = pick_scratch_reg({x64::Reg::RAX});
        if (!rhs_reg) return false;
        emit_.mov(*rhs_reg, x64::Reg::RAX);
        emit_.pop(x64::Reg::RAX);

        return emit_binary_with_rhs_reg(bin.op, *rhs_reg);
    }

    [[nodiscard]] bool generate_assignment(const ast::BinaryExpr& bin) {
        if (auto overwrite = describe_owned_local_overwrite(bin)) {
            error(*overwrite);
            return false;
        }

        // Right side -> RAX
        if (!generate_expr(*bin.rhs)) return false;

        // handle field assignment (e.g., player.health = 100 or outer.inner.x = 42)
        if (bin.lhs->is<ast::FieldExpr>()) {
            const auto& field = bin.lhs->as<ast::FieldExpr>();
            
            // rax has the value to store, save it
            emit_.push(x64::Reg::RAX);
            
            // figure out the struct type that contains the final field
            std::string struct_name;
            Symbol* base_sym = nullptr;
            
            if (field.base->is<ast::IdentExpr>()) {
                // simple case: var.field = value
                const std::string& var_name = field.base->as<ast::IdentExpr>().name;
                Symbol* sym = current_scope_->lookup(var_name);
                if (!sym) {
                    error_undefined_name("variable", var_name, collect_value_name_candidates());
                    return false;
                }
                if (!allows_interior_mutation(*sym)) {
                    error(describe_field_mutation_target(var_name));
                    return false;
                }
                auto sn = get_struct_name(sym->type);
                if (!sn) {
                    error(std::format("variable '{}' is not a struct type", var_name));
                    return false;
                }
                struct_name = *sn;
                base_sym = sym;
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
                error("unsupported assignment target: only identifiers and field accesses may be assigned");
                return false;
            }
            
            // look up field offset on the resolved struct
            auto struct_it = structs_.find(struct_name);
            if (struct_it == structs_.end()) {
                error_unknown_type("struct type", struct_name);
                return false;
            }
            auto offset_opt = struct_it->second.get_field_offset(field.field);
            auto field_type = lookup_struct_field_type(struct_name, field.field);
            if (!offset_opt) {
                error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
                return false;
            }
            if (!field_type) {
                error(std::format("struct '{}' has no typed field '{}'", struct_name, field.field));
                return false;
            }
            std::int32_t offset = static_cast<std::int32_t>(*offset_opt);
            if (!emit_coerce_rax_to_type(*field_type, bin.rhs.get())) {
                return false;
            }
            
            // pop value into a scratch reg, store at [rax]
            auto value_reg = pick_scratch_reg({x64::Reg::RAX});
            if (!value_reg) {
                error("no scratch register available for field assignment");
                return false;
            }
            emit_.pop(*value_reg);

            if (base_sym) {
                if (auto bound = lookup_bound_reg(base_sym)) {
                    emit_.mov_store(*bound, offset, *value_reg);
                } else {
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, base_sym->stack_offset);
                    emit_.mov_store(x64::Reg::RAX, offset, *value_reg);
                }
            } else {
                emit_.mov_store(x64::Reg::RAX, offset, *value_reg);
            }
            
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
                error(describe_rebinding_target("variable", ident.name));
                return false;
            }
            if (auto bound = lookup_bound_reg(sym)) {
                if (!emit_coerce_rax_to_type(sym->type, bin.rhs.get())) {
                    return false;
                }
                if (!should_elide_bound_stack_slot(sym)) {
                    emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
                }
                if (*bound != x64::Reg::RAX) {
                    emit_.mov(*bound, x64::Reg::RAX);
                }
                update_owned_local_after_store(sym, bin.rhs.get());
                return true;
            }
            if (!emit_coerce_rax_to_type(sym->type, bin.rhs.get())) {
                return false;
            }
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
            update_owned_local_after_store(sym, bin.rhs.get());
            return true;
        }
        
        // try globals
        auto it = globals_.find(ident.name);
        if (it != globals_.end()) {
            if (!it->second.is_mut) {
                error(describe_rebinding_target("global", ident.name));
                return false;
            }
            if (!emit_coerce_rax_to_type(it->second.type, bin.rhs.get())) {
                return false;
            }
            
            if (has_global_slot_) {
                // value is in RAX, save it
                emit_.push(x64::Reg::RAX);
                
                // load base pointer
                if (!emit_load_global_base_ptr(x64::Reg::RAX)) return false;
                
                // pop value into RCX, store at base + offset
                emit_.pop(x64::Reg::RCX);
                emit_.mov_store(x64::Reg::RAX, static_cast<std::int32_t>(it->second.offset), x64::Reg::RCX);
                return true;
            }
            
            error("global variable storage is unavailable");
            return false;
        }
        
        error_undefined_name("variable", ident.name, collect_value_name_candidates());
        return false;
    }

    [[nodiscard]] bool generate_compound_assign(const ast::BinaryExpr& bin) {
        using Op = ast::BinaryExpr::Op;
        
        // handle field compound assign: player.health += 10
        if (bin.lhs->is<ast::FieldExpr>()) {
            const auto& field = bin.lhs->as<ast::FieldExpr>();
            if (!field.base->is<ast::IdentExpr>()) {
                error("unsupported assignment form: compound assignment on chained fields is not supported yet");
                return false;
            }
            
            const std::string& var_name = field.base->as<ast::IdentExpr>().name;
            Symbol* sym = current_scope_->lookup(var_name);
            if (!sym) {
                error_undefined_name("variable", var_name, collect_value_name_candidates());
                return false;
            }
            if (!allows_interior_mutation(*sym)) {
                error(describe_field_mutation_target(var_name));
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
                error_unknown_type("struct", struct_name);
                return false;
            }
            
            auto offset_opt = struct_it->second.get_field_offset(field.field);
            auto field_type = lookup_struct_field_type(struct_name, field.field);
            if (!offset_opt) {
                error(std::format("struct '{}' has no field '{}'", struct_name, field.field));
                return false;
            }
            if (!field_type) {
                error(std::format("struct '{}' has no typed field '{}'", struct_name, field.field));
                return false;
            }
            std::int32_t foff = static_cast<std::int32_t>(*offset_opt);
            
            // eval rhs
            if (!generate_expr(*bin.rhs)) return false;
            emit_.push(x64::Reg::RAX); // save rhs
            
            // load base pointer and current field value
            if (auto bound = lookup_bound_reg(sym)) {
                emit_.mov_load(x64::Reg::RAX, *bound, foff); // current value
            } else {
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, foff); // current value
            }
            
            // pop rhs into a scratch reg so bound param registers stay intact
            auto rhs_reg = pick_scratch_reg({x64::Reg::RAX, x64::Reg::RDX});
            if (!rhs_reg) {
                error("no scratch register available for field compound assignment");
                return false;
            }
            emit_.pop(*rhs_reg);
            
            // apply op
            switch (bin.op) {
                case Op::AddAssign: emit_.add(x64::Reg::RAX, *rhs_reg); break;
                case Op::SubAssign: emit_.sub(x64::Reg::RAX, *rhs_reg); break;
                case Op::MulAssign: emit_.imul(x64::Reg::RAX, *rhs_reg); break;
                case Op::DivAssign: emit_.cqo(); emit_.idiv(*rhs_reg); break;
                case Op::ModAssign: emit_.cqo(); emit_.idiv(*rhs_reg); emit_.mov(x64::Reg::RAX, x64::Reg::RDX); break;
                default: error("unsupported compound op"); return false;
            }
            if (!emit_coerce_rax_to_type(*field_type)) {
                return false;
            }
            
            // store result back
            emit_.mov(*rhs_reg, x64::Reg::RAX); // save result
            if (auto bound = lookup_bound_reg(sym)) {
                emit_.mov_store(*bound, foff, *rhs_reg);
            } else {
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset); // reload base
                emit_.mov_store(x64::Reg::RAX, foff, *rhs_reg);
            }
            
            return true;
        }
        
        // left side must be an identifier
        if (!bin.lhs->is<ast::IdentExpr>()) {
            error("unsupported assignment target: left side of compound assignment must be an identifier or field access");
            return false;
        }

        const auto& ident = bin.lhs->as<ast::IdentExpr>();
        Symbol* sym = current_scope_->lookup(ident.name);
        if (!sym) {
            error_undefined_name("variable", ident.name, collect_value_name_candidates());
            return false;
        }
        if (!sym->is_mut) {
            error(describe_rebinding_target("variable", ident.name));
            return false;
        }

        std::int64_t rhs_imm = 0;
        if (sym->type.size_bytes() >= 8 &&
            (bin.op == Op::AddAssign || bin.op == Op::SubAssign) &&
            try_get_i64_immediate(*bin.rhs, rhs_imm)) {
            std::int64_t delta = bin.op == Op::AddAssign ? rhs_imm : -rhs_imm;
            if (auto bound = lookup_bound_reg(sym)) {
                emit_.add_smart(*bound, delta);
                emit_.mov(x64::Reg::RAX, *bound);
                if (!should_elide_bound_stack_slot(sym)) {
                    emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
                }
                return true;
            }

            if (delta >= (std::numeric_limits<std::int32_t>::min)() &&
                delta <= (std::numeric_limits<std::int32_t>::max)()) {
                auto delta32 = static_cast<std::int32_t>(delta);
                if (delta32 == 1) {
                    emit_.inc_mem(x64::Reg::RBP, sym->stack_offset);
                } else if (delta32 == -1) {
                    emit_.dec_mem(x64::Reg::RBP, sym->stack_offset);
                } else {
                    emit_.add_mem_imm(x64::Reg::RBP, sym->stack_offset, delta32);
                }
                emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                return true;
            }
        }

        // Generate right side -> RAX
        if (!generate_expr(*bin.rhs)) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // Save RHS to RCX

        // Register-bound locals must be updated in the register, not the stack spill slot.
        auto bound = lookup_bound_reg(sym);
        if (bound) {
            emit_.mov(x64::Reg::RAX, *bound);
        } else {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
        }
        
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
        if (!emit_coerce_rax_to_type(sym->type)) {
            return false;
        }
        
        // Store result
        if (bound) {
            if (!should_elide_bound_stack_slot(sym)) {
                emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
            }
            if (*bound != x64::Reg::RAX) {
                emit_.mov(*bound, x64::Reg::RAX);
            }
        } else {
            emit_.mov_store(x64::Reg::RBP, sym->stack_offset, x64::Reg::RAX);
        }
        return true;
    }

    [[nodiscard]] bool generate_unary(const ast::UnaryExpr& un) {
        using Op = ast::UnaryExpr::Op;

        // Addr-of needs the raw operand, not its loaded value.
        if (un.op != Op::AddrOf && un.op != Op::AddrOfMut) {
            // Generate operand -> RAX
            if (!generate_expr(*un.operand)) return false;
        }

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
                    if (sym) {
                        emit_.lea(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                    } else {
                        std::string resolved_name = resolve_function_name(ident.name);
                        auto fn_it = functions_.find(resolved_name);
                        if (fn_it == functions_.end()) {
                            error_undefined_name("variable", ident.name, collect_value_name_candidates());
                            return false;
                        }
                        std::size_t fn_addr_fixup = emit_lea_rip_disp32(x64::Reg::RAX);
                        call_fixups_.push_back({fn_addr_fixup, resolved_name});
                    }
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
                    error_undefined_name("variable", ident.name, collect_value_name_candidates());
                    return false;
                    }
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                    if (un.op == Op::PreInc) {
                        emit_.add_smart(x64::Reg::RAX, 1);
                    } else {
                        emit_.add_smart(x64::Reg::RAX, -1);
                    }
                    if (!emit_coerce_rax_to_type(sym->type)) {
                        return false;
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
                        error_undefined_name("variable", ident.name, collect_value_name_candidates());
                        return false;
                    }
                    // Load current value (this is what we return)
                    emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, sym->stack_offset);
                    // Save to temp register
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                    // Increment/decrement
                    if (un.op == Op::PostInc) {
                        emit_.add_smart(x64::Reg::RCX, 1);
                    } else {
                        emit_.add_smart(x64::Reg::RCX, -1);
                    }
                    if (!emit_coerce_reg_to_type(x64::Reg::RCX, sym->type)) {
                        return false;
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

    [[nodiscard]] bool generate_direct_call_named(const ast::CallExpr& call, const std::string& target_fn_name) {
        std::string resolved_target = target_fn_name;
        if (resolved_target.empty()) {
            if (call.callee->is<ast::IdentExpr>()) {
                const auto& raw_name = call.callee->as<ast::IdentExpr>().name;
                if (!raw_name.empty()) {
                    resolved_target = resolve_function_name(raw_name);
                }
            } else if (call.callee->is<ast::FieldExpr>()) {
                if (auto qualified = get_qualified_name(*call.callee)) {
                    resolved_target = resolve_function_name(*qualified);
                }
            }
        }

        auto it = functions_.find(resolved_target);
        if (it == functions_.end()) {
            error_undefined_name("function", resolved_target, collect_function_name_candidates());
            return false;
        }

        if (should_inline_direct_call(call, it->second)) {
            return generate_inlined_safe_call(call, it->second);
        }

        std::size_t nargs = call.args.size();
        std::size_t reg_args = nargs < 4 ? nargs : 4;
        std::size_t stack_args = nargs > 4 ? nargs - 4 : 0;

        for (std::size_t i = 0; i < call.args.size(); ++i) {
            if (!generate_expr(*call.args[i])) return false;
            if (!emit_coerce_rax_to_type(it->second.param_types[i], call.args[i].get())) return false;
            emit_.push(x64::Reg::RAX);
        }

        std::size_t alloc = 32 + stack_args * 8;
        if (stack_args == 0 && (nargs & 1) != 0) {
            alloc += 8;
        }
        emit_.sub_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc));

        for (std::size_t i = 0; i < reg_args; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            if (!emit_indirect_arg_to_abi_slot(i, it->second.param_types[i], src_off, 0, false)) {
                return false;
            }
        }

        for (std::size_t i = 4; i < nargs; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            std::int32_t dest_off = static_cast<std::int32_t>(32 + (i - 4) * 8);
            if (!emit_indirect_arg_to_abi_slot(i, it->second.param_types[i], src_off, dest_off, true)) {
                return false;
            }
        }

        emit_.buffer().emit8(0xE8);
        std::size_t fixup_site = emit_.buffer().pos();
        emit_.buffer().emit32(0);

        call_fixups_.push_back(CallFixup{
            .call_site = fixup_site,
            .target_fn = resolved_target
        });

        emit_.add_smart(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));

        if (it->second.return_type.is_float()) {
            emit_capture_indirect_float_return(it->second.return_type);
        } else if (it->second.return_type.is_integer() || is_bool_type(it->second.return_type)) {
            emit_truncate_or_extend_rax(it->second.return_type);
        }
        return true;
    }

    [[nodiscard]] bool should_inline_direct_call(const ast::CallExpr& call, const FunctionInfo& info) const {
        if (info.is_extern || !info.decl || !info.single_use_inline_safe) {
            return false;
        }
        if (call.args.size() != info.param_types.size()) {
            return false;
        }
        if (is_string_type(info.return_type)) {
            return false;
        }
        for (const auto& param_type : info.param_types) {
            if (is_string_type(param_type)) {
                return false;
            }
        }
        if (!info.decl->has_body() || info.decl->body_ref().size() > 8) {
            return false;
        }
        for (const auto& arg : call.args) {
            if (!expr_is_side_effect_free(*arg)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool can_registerize_immutable_let_init(const ast::Expr& expr) const {
        if (expr_is_side_effect_free(expr) && can_generate_pure_expr(expr)) {
            return true;
        }

        auto* call = std::get_if<ast::CallExpr>(&expr.kind);
        if (!call) {
            return false;
        }

        std::string resolved_target;
        if (call->callee->is<ast::IdentExpr>()) {
            resolved_target = resolve_function_name(call->callee->as<ast::IdentExpr>().name);
        } else if (call->callee->is<ast::FieldExpr>()) {
            if (auto qualified = get_qualified_name(*call->callee)) {
                resolved_target = resolve_function_name(*qualified);
            }
        }

        if (resolved_target.empty()) {
            return false;
        }

        auto it = functions_.find(resolved_target);
        if (it == functions_.end()) {
            return false;
        }

        return should_inline_direct_call(*call, it->second);
    }

    [[nodiscard]] bool is_inline_safe_direct_call_expr(const ast::Expr& expr) const {
        auto* call = std::get_if<ast::CallExpr>(&expr.kind);
        if (!call) {
            return false;
        }

        std::string resolved_target;
        if (call->callee->is<ast::IdentExpr>()) {
            resolved_target = resolve_function_name(call->callee->as<ast::IdentExpr>().name);
        } else if (call->callee->is<ast::FieldExpr>()) {
            if (auto qualified = get_qualified_name(*call->callee)) {
                resolved_target = resolve_function_name(*qualified);
            }
        }

        if (resolved_target.empty()) {
            return false;
        }

        auto it = functions_.find(resolved_target);
        if (it == functions_.end()) {
            return false;
        }

        return should_inline_direct_call(*call, it->second);
    }

    [[nodiscard]] bool generate_inline_stmt_list(
        const std::vector<ast::StmtPtr>& stmts,
        std::vector<std::size_t>& return_patches
    ) {
        for (const auto& stmt : stmts) {
            if (!generate_inline_stmt(*stmt, return_patches)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool generate_inline_let(const ast::LetStmt& let) {
        Symbol* sym = define_let_symbol(let);
        if (!sym) {
            return false;
        }

        if (!let.init) {
            update_owned_local_after_store(sym, nullptr);
            return true;
        }

        if (auto bound = lookup_bound_reg(sym)) {
            const ast::Expr& init = *let.init;
            if (expr_is_side_effect_free(init) && can_generate_pure_expr(init)) {
                if (!generate_pure_expr_into(init, *bound)) {
                    return false;
                }
                if (!emit_coerce_reg_to_type(*bound, sym->type, let.init.get())) {
                    return false;
                }
                if (!should_elide_bound_stack_slot(sym)) {
                    emit_.mov_store(x64::Reg::RBP, sym->stack_offset, *bound);
                }
                update_owned_local_after_store(sym, let.init.get());
                return true;
            }
        }

        if (!emit_let_init(let, *sym)) {
            return false;
        }
        update_owned_local_after_store(sym, let.init.get());
        return true;
    }

    [[nodiscard]] bool generate_inline_stmt(const ast::Stmt& stmt, std::vector<std::size_t>& return_patches) {
        if (stmt.span.start.line > 0) {
            line_map_.push_back({
                static_cast<std::uint32_t>(emit_.buffer().pos()),
                stmt.span.start.line
            });
        }

        return std::visit(overloaded{
            [&](const ast::LetStmt& s) {
                return generate_inline_let(s);
            },
            [&](const ast::ExprStmt& s) {
                return generate_expr_stmt(s);
            },
            [&](const ast::ReturnStmt& s) {
                if (s.value) {
                    if (!generate_expr(*s.value)) return false;
                } else {
                    emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
                }
                return_patches.push_back(emit_.jmp_rel32_placeholder());
                return true;
            },
            [&](const ast::IfStmt& s) {
                std::size_t else_patch = 0;
                if (!emit_condition_false_jump(*s.condition, else_patch)) return false;

                push_scope();
                if (!generate_inline_stmt_list(s.then_block, return_patches)) {
                    pop_scope();
                    return false;
                }
                pop_scope();

                if (s.has_else()) {
                    std::size_t done_patch = emit_.jmp_rel32_placeholder();
                    emit_.patch_jump(else_patch);

                    push_scope();
                    if (!generate_inline_stmt_list(*s.else_block, return_patches)) {
                        pop_scope();
                        return false;
                    }
                    pop_scope();

                    emit_.patch_jump(done_patch);
                } else {
                    emit_.patch_jump(else_patch);
                }
                return true;
            },
            [&](const ast::BlockStmt& s) {
                push_scope();
                if (!generate_inline_stmt_list(s.block.stmts, return_patches)) {
                    pop_scope();
                    return false;
                }
                pop_scope();
                return true;
            },
            [&](const auto&) {
                error("unsupported statement in inline-safe function");
                return false;
            }
        }, stmt.kind);
    }

    [[nodiscard]] bool generate_inlined_safe_call(const ast::CallExpr& call, const FunctionInfo& info) {
        if (!info.decl) {
            return false;
        }

        std::unordered_map<std::string, x64::Reg> inline_named_regs;
        FunctionLeafRegisterPlan inline_plan;
        if (try_make_function_leaf_register_plan(*info.decl, inline_plan)) {
            constexpr std::array<x64::Reg, 6> inline_candidates = {
                x64::Reg::R11,
                x64::Reg::R10,
                x64::Reg::R9,
                x64::Reg::R8,
                x64::Reg::RDX,
                x64::Reg::RCX,
            };

            std::unordered_set<x64::Reg> used_regs;
            auto try_bind_name = [&](const std::string& name) -> bool {
                for (x64::Reg reg : inline_candidates) {
                    if (used_regs.contains(reg) || reg_is_bound(reg)) {
                        continue;
                    }
                    inline_named_regs[name] = reg;
                    used_regs.insert(reg);
                    return true;
                }
                return false;
            };

            bool binding_ok = true;
            for (const auto& param : info.decl->params) {
                if (!try_bind_name(param.name)) {
                    binding_ok = false;
                    break;
                }
            }
            if (binding_ok) {
                for (const auto& [name, _] : inline_plan.local_regs) {
                    if (inline_named_regs.contains(name)) {
                        continue;
                    }
                    if (!try_bind_name(name)) {
                        binding_ok = false;
                        break;
                    }
                }
            }
            if (!binding_ok) {
                inline_named_regs.clear();
            }
        }

        push_scope();

        std::vector<std::string> inline_local_names;
        inline_local_names.reserve(inline_named_regs.size());
        for (const auto& [name, reg] : inline_named_regs) {
            if (std::none_of(info.decl->params.begin(), info.decl->params.end(), [&](const ast::Param& p) { return p.name == name; })) {
                pending_named_register_bindings_[name] = reg;
                inline_local_names.push_back(name);
            }
        }
        bound_stack_elision_names_.push_back({});
        for (const auto& name : inline_local_names) {
            bound_stack_elision_names_.back().insert(name);
        }

        std::vector<std::string> param_binding_names;
        param_binding_names.reserve(info.decl->params.size());
        std::vector<ast::Expr> param_binding_exprs;
        param_binding_exprs.reserve(info.decl->params.size());
        std::vector<Symbol*> hidden_arg_symbols;
        hidden_arg_symbols.reserve(info.decl->params.size());
        auto cleanup_inline_scope = [&]() {
            for (const auto& name : param_binding_names) {
                current_scope_->pop_inline(name);
            }
            for (const auto& name : inline_local_names) {
                pending_named_register_bindings_.erase(name);
            }
            for (Symbol* sym : hidden_arg_symbols) {
                register_bindings_.erase(sym);
            }
            bound_stack_elision_names_.pop_back();
            pop_scope();
        };
        for (std::size_t i = 0; i < info.decl->params.size(); ++i) {
            std::string hidden_name = std::format("__opus_inline_arg_{}_{}", emit_.buffer().pos(), i);
            Symbol& sym = current_scope_->define(hidden_name, info.param_types[i].clone(), false);
            if (auto reg_it = inline_named_regs.find(info.decl->params[i].name); reg_it != inline_named_regs.end()) {
                register_bindings_[&sym] = reg_it->second;
                bound_stack_elision_names_.back().insert(hidden_name);
            }
            if (!generate_expr(*call.args[i])) {
                cleanup_inline_scope();
                return false;
            }
            if (!emit_coerce_rax_to_type(info.param_types[i], call.args[i].get())) {
                cleanup_inline_scope();
                return false;
            }
            if (!should_elide_bound_stack_slot(&sym)) {
                emit_.mov_store(x64::Reg::RBP, sym.stack_offset, x64::Reg::RAX);
            }
            if (auto reg_it = inline_named_regs.find(info.decl->params[i].name); reg_it != inline_named_regs.end() &&
                reg_it->second != x64::Reg::RAX) {
                emit_.mov(reg_it->second, x64::Reg::RAX);
            }

            ast::Expr alias_expr;
            alias_expr.kind = ast::IdentExpr{hidden_name};
            param_binding_exprs.push_back(std::move(alias_expr));
            current_scope_->push_inline(info.decl->params[i].name, &param_binding_exprs.back());
            param_binding_names.push_back(info.decl->params[i].name);
            hidden_arg_symbols.push_back(&sym);
        }

        std::function<bool(const ast::Stmt&)> stmt_guarantees_return;
        stmt_guarantees_return = [&](const ast::Stmt& stmt) -> bool {
            auto stmt_list_guarantees_return = [&](const std::vector<ast::StmtPtr>& stmts) -> bool {
                return !stmts.empty() && stmt_guarantees_return(*stmts.back());
            };

            return std::visit(overloaded{
                [&](const ast::ReturnStmt&) {
                    return true;
                },
                [&](const ast::BlockStmt& s) {
                    return stmt_list_guarantees_return(s.block.stmts);
                },
                [&](const ast::IfStmt& s) {
                    return !s.then_block.empty() &&
                           s.has_else() &&
                           stmt_list_guarantees_return(s.then_block) &&
                           stmt_list_guarantees_return(*s.else_block);
                },
                [&](const auto&) {
                    return false;
                }
            }, stmt.kind);
        };

        if (!info.decl->has_body()) {
            cleanup_inline_scope();
            return false;
        }

        const auto& body = info.decl->body_ref();
        if (body.empty() || !stmt_guarantees_return(*body.back())) {
            emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        }
        std::vector<std::size_t> return_patches;
        if (!generate_inline_stmt_list(body, return_patches)) {
            cleanup_inline_scope();
            return false;
        }

        std::size_t done = emit_.buffer().pos();
        for (std::size_t patch : return_patches) {
            auto rel = static_cast<std::int32_t>(done) - static_cast<std::int32_t>(patch + 4);
            if (rel == 0 && patch > 0) {
                auto* code = emit_.buffer().data();
                code[patch - 1] = 0x90;
                code[patch + 0] = 0x90;
                code[patch + 1] = 0x90;
                code[patch + 2] = 0x90;
                code[patch + 3] = 0x90;
                continue;
            }
            emit_.buffer().patch32(patch, static_cast<std::uint32_t>(rel));
        }

        cleanup_inline_scope();
        return true;
    }

    void emit_sse_stack_operand(std::uint8_t reg_field, std::int32_t offset) {
        if (offset == 0) {
            emit_.buffer().emit8(static_cast<std::uint8_t>((reg_field << 3) | 0x04));
            emit_.buffer().emit8(0x24);
            return;
        }
        if (offset >= -128 && offset <= 127) {
            emit_.buffer().emit8(static_cast<std::uint8_t>(0x40 | (reg_field << 3) | 0x04));
            emit_.buffer().emit8(0x24);
            emit_.buffer().emit8(static_cast<std::uint8_t>(offset));
            return;
        }
        emit_.buffer().emit8(static_cast<std::uint8_t>(0x80 | (reg_field << 3) | 0x04));
        emit_.buffer().emit8(0x24);
        emit_.buffer().emit32(static_cast<std::uint32_t>(offset));
    }

    void emit_xmm_load_f64_from_stack(std::uint8_t xmm_index, std::int32_t offset) {
        emit_.buffer().emit8(0xF2);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x10);
        emit_sse_stack_operand(xmm_index & 7, offset);
    }

    void emit_xmm_convert_f64_to_f32(std::uint8_t xmm_index) {
        emit_.buffer().emit8(0xF2);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x5A);
        emit_.buffer().emit8(static_cast<std::uint8_t>(0xC0 | ((xmm_index & 7) << 3) | (xmm_index & 7)));
    }

    void emit_xmm_convert_f32_to_f64(std::uint8_t xmm_index) {
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x5A);
        emit_.buffer().emit8(static_cast<std::uint8_t>(0xC0 | ((xmm_index & 7) << 3) | (xmm_index & 7)));
    }

    void emit_xmm_store_f64_to_stack(std::uint8_t xmm_index, std::int32_t offset) {
        emit_.buffer().emit8(0xF2);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x11);
        emit_sse_stack_operand(xmm_index & 7, offset);
    }

    void emit_xmm_store_f32_to_stack(std::uint8_t xmm_index, std::int32_t offset) {
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x11);
        emit_sse_stack_operand(xmm_index & 7, offset);
    }

    void emit_xmm_load_f64_from_rbp(std::uint8_t xmm_index, std::int32_t offset) {
        emit_.buffer().emit8(0xF2);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x10);
        if (offset >= -128 && offset <= 127) {
            emit_.buffer().emit8(static_cast<std::uint8_t>(0x40 | ((xmm_index & 7) << 3) | 0x05));
            emit_.buffer().emit8(static_cast<std::uint8_t>(offset));
            return;
        }
        emit_.buffer().emit8(static_cast<std::uint8_t>(0x80 | ((xmm_index & 7) << 3) | 0x05));
        emit_.buffer().emit32(static_cast<std::uint32_t>(offset));
    }

    void emit_xmm_load_f32_from_rbp(std::uint8_t xmm_index, std::int32_t offset) {
        emit_.buffer().emit8(0xF3);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x10);
        if (offset >= -128 && offset <= 127) {
            emit_.buffer().emit8(static_cast<std::uint8_t>(0x40 | ((xmm_index & 7) << 3) | 0x05));
            emit_.buffer().emit8(static_cast<std::uint8_t>(offset));
            return;
        }
        emit_.buffer().emit8(static_cast<std::uint8_t>(0x80 | ((xmm_index & 7) << 3) | 0x05));
        emit_.buffer().emit32(static_cast<std::uint32_t>(offset));
    }

    [[nodiscard]] bool emit_capture_float_param_to_rax(std::size_t arg_index, const Type& target_type) {
        emit_.sub_imm(x64::Reg::RSP, 8);
        std::uint8_t xmm_index = static_cast<std::uint8_t>(arg_index & 7);
        if (auto* prim = std::get_if<PrimitiveType>(&target_type.kind); prim && *prim == PrimitiveType::F32) {
            emit_xmm_convert_f32_to_f64(xmm_index);
        }
        emit_xmm_store_f64_to_stack(xmm_index, 0);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 0);
        emit_.add_smart(x64::Reg::RSP, 8);
        return true;
    }

    [[nodiscard]] bool emit_capture_stack_float_param_to_rax(std::int32_t src_offset, const Type& target_type) {
        emit_.sub_imm(x64::Reg::RSP, 8);
        if (auto* prim = std::get_if<PrimitiveType>(&target_type.kind); prim && *prim == PrimitiveType::F32) {
            emit_xmm_load_f32_from_rbp(0, src_offset);
            emit_xmm_convert_f32_to_f64(0);
        } else {
            emit_xmm_load_f64_from_rbp(0, src_offset);
        }
        emit_xmm_store_f64_to_stack(0, 0);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 0);
        emit_.add_smart(x64::Reg::RSP, 8);
        return true;
    }

    [[nodiscard]] bool emit_indirect_arg_to_abi_slot(
        std::size_t arg_index,
        const Type& param_type,
        std::int32_t src_off,
        std::int32_t dest_off,
        bool to_stack
    ) {
        if (param_type.is_float()) {
            emit_xmm_load_f64_from_stack(to_stack ? 0 : static_cast<std::uint8_t>(arg_index), src_off);
            if (auto* prim = std::get_if<PrimitiveType>(&param_type.kind); prim && *prim == PrimitiveType::F32) {
                emit_xmm_convert_f64_to_f32(to_stack ? 0 : static_cast<std::uint8_t>(arg_index));
            }
            if (to_stack) {
                emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
                emit_.mov_store(x64::Reg::RSP, dest_off, x64::Reg::RAX);
                if (auto* prim = std::get_if<PrimitiveType>(&param_type.kind); prim && *prim == PrimitiveType::F32) {
                    emit_xmm_store_f32_to_stack(0, dest_off);
                } else {
                    emit_xmm_store_f64_to_stack(0, dest_off);
                }
            }
            return true;
        }

        emit_.mov_load(to_stack ? x64::Reg::RAX : x64::ARG_REGS[arg_index], x64::Reg::RSP, src_off);
        if (to_stack) {
            emit_.mov_store(x64::Reg::RSP, dest_off, x64::Reg::RAX);
        }
        return true;
    }

    void emit_capture_indirect_float_return(const Type& ret_type) {
        emit_.sub_imm(x64::Reg::RSP, 8);
        if (auto* prim = std::get_if<PrimitiveType>(&ret_type.kind); prim && *prim == PrimitiveType::F32) {
            emit_xmm_convert_f32_to_f64(0);
        }
        emit_xmm_store_f64_to_stack(0, 0);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 0);
        emit_.add_smart(x64::Reg::RSP, 8);
    }

    void emit_return_float_rax_to_xmm0(const Type& ret_type) {
        emit_.buffer().emit8(0x66);
        emit_.buffer().emit8(0x48);
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x6E);
        emit_.buffer().emit8(0xC0);  // movq xmm0, rax
        if (auto* prim = std::get_if<PrimitiveType>(&ret_type.kind); prim && *prim == PrimitiveType::F32) {
            emit_xmm_convert_f64_to_f32(0);
        }
    }

    [[nodiscard]] bool generate_indirect_call(const ast::CallExpr& call, const FunctionType& fn_type) {
        if (fn_type.is_variadic) {
            error("variadic function pointer calls are not supported yet");
            return false;
        }
        if (call.args.size() != fn_type.params.size()) {
            error(std::format("function pointer expected {} args but got {}", fn_type.params.size(), call.args.size()));
            return false;
        }

        std::size_t nargs = call.args.size();
        std::size_t reg_args = nargs < 4 ? nargs : 4;
        std::size_t stack_args = nargs > 4 ? nargs - 4 : 0;

        for (std::size_t i = 0; i < nargs; ++i) {
            if (!generate_expr(*call.args[i])) return false;
            if (!emit_coerce_rax_to_type(*fn_type.params[i], call.args[i].get())) return false;
            emit_.push(x64::Reg::RAX);
        }

        if (!generate_expr(*call.callee)) return false;
        emit_.push(x64::Reg::RAX);
        emit_.pop(x64::Reg::R11);

        std::size_t alloc = 32 + stack_args * 8;
        if (stack_args == 0 && (nargs & 1) != 0) {
            alloc += 8;
        }
        emit_.sub_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc));

        for (std::size_t i = 0; i < reg_args; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            if (!emit_indirect_arg_to_abi_slot(i, *fn_type.params[i], src_off, 0, false)) {
                return false;
            }
        }

        for (std::size_t i = 4; i < nargs; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (nargs - 1 - i) * 8);
            std::int32_t dest_off = static_cast<std::int32_t>(32 + (i - 4) * 8);
            if (!emit_indirect_arg_to_abi_slot(i, *fn_type.params[i], src_off, dest_off, true)) {
                return false;
            }
        }

        emit_.call(x64::Reg::R11);
        emit_.add_smart(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));

        if (fn_type.ret && fn_type.ret->is_float()) {
            emit_capture_indirect_float_return(*fn_type.ret);
        } else if (fn_type.ret && (fn_type.ret->is_integer() || is_bool_type(*fn_type.ret))) {
            emit_truncate_or_extend_rax(*fn_type.ret);
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
            error_undefined_name("variable", var_name, collect_value_name_candidates());
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
        emit_.add_smart(x64::Reg::RSP, static_cast<std::int32_t>(alloc + nargs * 8));
        
        return true;
    }

    [[nodiscard]] bool generate_call(const ast::CallExpr& call) {
        if (auto callee_type = infer_expr_type(*call.callee)) {
            if (auto* fn = std::get_if<FunctionType>(&callee_type->kind)) {
                return generate_indirect_call(call, *fn);
            }
        }

        // Handle module-qualified calls before method calls: math.add(1, 2)
        if (call.callee->is<ast::FieldExpr>()) {
            const auto& field = call.callee->as<ast::FieldExpr>();
            bool base_is_local = field.base->is<ast::IdentExpr>() && current_scope_ &&
                                 current_scope_->lookup(field.base->as<ast::IdentExpr>().name);
            if (!base_is_local) {
                if (auto qualified = get_qualified_name(*call.callee)) {
                    std::string resolved_fn_name = resolve_function_name(*qualified);
                    if (functions_.contains(resolved_fn_name)) {
                        return generate_direct_call_named(call, resolved_fn_name);
                    }
                }
                if (auto base_name = get_qualified_name(*field.base)) {
                    if (import_namespace_owners_.contains(*base_name)) {
                        error_unknown_import_member(*base_name, field.field);
                        return false;
                    }
                }
            }
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

        // native-image builtins get first crack at the name
        if (is_native_image_output()) {
            auto dit = dll_builtins_.find(normalized);
            if (dit != dll_builtins_.end()) return dit->second(call);
        }

        // shared builtins
        {
            auto shared = shared_builtins_.find(normalized);
            if (shared != shared_builtins_.end()) return shared->second(call);
        }
        
        // user-defined function - use original name to preserve casing
        return generate_direct_call_named(call, resolve_function_name(raw_fn_name));
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
        emit_.add_smart(x64::Reg::RSP, 32);
        
        // Return 0 (print returns nothing useful)
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    // ========================================================================
    // native-image builtins - relative calls into the embedded startup routines
    // offsets come from pe::PeImageGenerator constexpr chain
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
        emit_.add_smart(x64::Reg::RSP, 32);
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
        emit_.add_smart(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_alloc_console([[maybe_unused]] const ast::CallExpr& call) {
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_startup_call(alloc_console_offset_);
        emit_.add_smart(x64::Reg::RSP, 32);
        emit_.mov_imm32(x64::Reg::RAX, 0);
        
        return true;
    }

    [[nodiscard]] bool generate_dll_exit(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("exit requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::ExitProcess);
        emit_.add_smart(x64::Reg::RSP, 32);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        return true;
    }
    
    [[nodiscard]] bool generate_dll_builtin_print_hex(const ast::CallExpr& call) {
        // prints value as "0x" + 8 hex digits
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
        
        // null-terminate and print
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);  // mov byte [rdx], 0
        
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x4C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);  // lea rcx, [rsp+0x20]
        
        emit_startup_call(print_offset_);
        
        emit_.add_smart(x64::Reg::RSP, 64);
        
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

        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24);
        emit_.buffer().emit8(0x20); emit_.buffer().emit8(0x0A);  // mov byte [rsp+0x20], '\n'
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44); emit_.buffer().emit8(0x24);
        emit_.buffer().emit8(0x21); emit_.buffer().emit8(0x00);  // mov byte [rsp+0x21], 0
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x8D); emit_.buffer().emit8(0x4C);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);  // lea rcx, [rsp+0x20]

        emit_startup_call(print_offset_);

        emit_.add_smart(x64::Reg::RSP, 64);
        
        emit_.pop(x64::Reg::RSI);
        emit_.pop(x64::Reg::RDI);
        
        return true;
    }

    [[nodiscard]] bool generate_dll_crash([[maybe_unused]] const ast::CallExpr& call) {
        // write to null pointer - triggers access violation for crash handler testing
        emit_.buffer().emit8(0x48);  // REX.W
        emit_.buffer().emit8(0x31);  // XOR r/m64, r64
        emit_.buffer().emit8(0xC0);  // rax, rax
        emit_.buffer().emit8(0x48);  // REX.W
        emit_.buffer().emit8(0xC7);  // MOV r/m64, imm32
        emit_.buffer().emit8(0x00);  // [rax]
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
        
        return true;
    }
    
    [[nodiscard]] bool generate_dll_get_tick_count([[maybe_unused]] const ast::CallExpr& call) {
        // get_tick_count() -> int - call Windows GetTickCount64
        emit_.sub_imm(x64::Reg::RSP, 32);
        
        emit_iat_call_raw(pe::iat::GetTickCount64);
        
        emit_.add_smart(x64::Reg::RSP, 32);
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
        emit_.add_smart(x64::Reg::RSP, 16);
        
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
        emit_.add_smart(x64::Reg::RSP, 16);
        
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
        emit_.add_smart(x64::Reg::RSP, 16);
        
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
        emit_.add_smart(x64::Reg::RSP, 16);
        
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
        emit_.add_smart(x64::Reg::RSP, 32);
        
        // set up HeapAlloc args: rcx=heap, rdx=0, r8=size
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // restore size
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
        
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
        emit_.add_smart(x64::Reg::RSP, 32);
        
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // ptr from stack
        
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_smart(x64::Reg::RSP, 32);
        
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
        emit_.add_smart(x64::Reg::RAX, 16);
        emit_.push(x64::Reg::RAX);  // [stack: capacity, total_size]

        // get process heap
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_smart(x64::Reg::RSP, 32);

        // HeapAlloc(heap, 0, total_size)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // total_size  [stack: capacity]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
        // rax = allocation base

        // zero length: [rax+0] = 0
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);

        // set capacity: [rax+8] = capacity
        emit_.pop(x64::Reg::RCX);  // restore capacity  [stack: empty]
        emit_.mov_store(x64::Reg::RAX, 8, x64::Reg::RCX);

        // return ptr past header
        emit_.add_smart(x64::Reg::RAX, 16);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_get(const ast::CallExpr& call) {
        // array_get(arr, index) -> return 0 for null/negative/oob, otherwise load arr[index]
        if (call.args.size() < 2) {
            error("array_get requires arr and index arguments");
            return false;
        }

        // eval arr, stash it
        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);

        // eval index into rax
        if (!generate_expr(*call.args[1])) return false;

        // rdx = index, pop arr into rcx
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RCX);

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);  // default result = 0

        emit_.test(x64::Reg::RCX, x64::Reg::RCX);
        std::size_t null_done = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.cmp_imm(x64::Reg::RDX, 0);
        std::size_t neg_done = emit_.jcc_rel32(x64::Emitter::CC_L);

        // length header lives at arr - 16
        emit_.mov_load(x64::Reg::R8, x64::Reg::RCX, -16);
        emit_.cmp(x64::Reg::RDX, x64::Reg::R8);
        std::size_t oob_done = emit_.jcc_rel32(x64::Emitter::CC_GE);

        emit_.mov(x64::Reg::RAX, x64::Reg::RDX);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03); // shl rax, 3
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);

        emit_.patch_jump(null_done);
        emit_.patch_jump(neg_done);
        emit_.patch_jump(oob_done);

        return true;
    }

    [[nodiscard]] bool generate_dll_array_set(const ast::CallExpr& call) {
        // array_set(arr, index, value) -> ignore null/negative/oob writes, update length on success
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

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);  // default return = 0

        emit_.test(x64::Reg::RDX, x64::Reg::RDX);
        std::size_t null_done = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.cmp_imm(x64::Reg::RCX, 0);
        std::size_t neg_done = emit_.jcc_rel32(x64::Emitter::CC_L);

        // capacity header lives at arr - 8
        emit_.mov_load(x64::Reg::R9, x64::Reg::RDX, -8);
        emit_.cmp(x64::Reg::RCX, x64::Reg::R9);
        std::size_t oob_done = emit_.jcc_rel32(x64::Emitter::CC_GE);

        // store value at arr + index*8
        emit_.mov(x64::Reg::RAX, x64::Reg::RCX);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03); // shl rax, 3
        emit_.add(x64::Reg::RAX, x64::Reg::RDX);  // rax = arr + index*8
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::R8);  // [arr + index*8] = value

        // update length = max(current_length, index + 1)
        emit_.mov(x64::Reg::RAX, x64::Reg::RCX);
        emit_.add_smart(x64::Reg::RAX, 1);  // rax = index + 1
        emit_.mov_load(x64::Reg::R9, x64::Reg::RDX, -16);  // r9 = current length
        emit_.cmp(x64::Reg::R9, x64::Reg::RAX);
        std::size_t skip_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);  // if current >= index+1, skip
        emit_.mov_store(x64::Reg::RDX, -16, x64::Reg::RAX);  // update length

        emit_.patch_jump(skip_patch);
        emit_.patch_jump(null_done);
        emit_.patch_jump(neg_done);
        emit_.patch_jump(oob_done);

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
        emit_.add_smart(x64::Reg::RSP, 32);

        // HeapFree(heap, 0, alloc_ptr)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // alloc base from stack

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapFree);
        emit_.add_smart(x64::Reg::RSP, 32);

        return true;
    }

    [[nodiscard]] bool generate_dll_read_file(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("read_file requires path argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // lpFileName

        // [rsp+00..31] shadow
        // [rsp+32]     arg5 / overlapped
        // [rsp+40]     arg6 / bytesRead
        // [rsp+48]     arg7
        // [rsp+56]     handle
        // [rsp+64]     size
        // [rsp+72]     buffer
        emit_.sub_imm(x64::Reg::RSP, 96);

        emit_.mov_imm64(x64::Reg::RDX, 0x80000000ull);  // GENERIC_READ
        emit_.mov_smart(x64::Reg::R8, 1);               // FILE_SHARE_READ
        emit_.xor_(x64::Reg::R9, x64::Reg::R9);         // security attrs = NULL
        emit_.mov_smart(x64::Reg::RAX, 3);              // OPEN_EXISTING
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_smart(x64::Reg::RAX, 0x80);           // FILE_ATTRIBUTE_NORMAL
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);       // template = NULL
        emit_.mov_store(x64::Reg::RSP, 48, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateFileA);
        emit_.mov_store(x64::Reg::RSP, 56, x64::Reg::RAX);
        emit_.cmp_imm(x64::Reg::RAX, -1);
        std::size_t create_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        emit_iat_call_raw(pe::iat::GetFileSize);
        emit_.mov_store(x64::Reg::RSP, 64, x64::Reg::RAX);
        emit_.cmp_imm(x64::Reg::RAX, -1);
        std::size_t size_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 64);
        emit_.add_smart(x64::Reg::R8, 1);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.mov_store(x64::Reg::RSP, 72, x64::Reg::RAX);
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t alloc_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, 72);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 64);
        emit_.lea(x64::Reg::R9, x64::Reg::RSP, 40);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);  // overlapped = NULL
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);  // bytesRead = 0
        emit_iat_call_raw(pe::iat::ReadFile);
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t read_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 72);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 64);
        emit_.add(x64::Reg::RAX, x64::Reg::RCX);
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x00); emit_.buffer().emit8(0x00);  // mov byte [rax], 0

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 72);
        emit_.add_smart(x64::Reg::RSP, 96);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(read_fail);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 72);
        emit_iat_call_raw(pe::iat::HeapFree);

        emit_.patch_jump(alloc_fail);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_iat_call_raw(pe::iat::CloseHandle);

        emit_.patch_jump(size_fail);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 96);
        std::size_t fail_done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(create_fail);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 96);

        emit_.patch_jump(done);
        emit_.patch_jump(fail_done);
        return true;
    }

    [[nodiscard]] bool generate_dll_write_file(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("write_file requires path and content arguments");
            return false;
        }

        emit_.sub_imm(x64::Reg::RSP, 96);

        if (!generate_expr(*call.args[1])) {
            emit_.add_smart(x64::Reg::RSP, 96);
            return false;
        }
        emit_.mov_store(x64::Reg::RSP, 72, x64::Reg::RAX);  // content

        if (!generate_expr(*call.args[0])) {
            emit_.add_smart(x64::Reg::RSP, 96);
            return false;
        }
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);            // lpFileName

        emit_.mov_imm64(x64::Reg::RDX, 0x40000000ull);      // GENERIC_WRITE
        emit_.xor_(x64::Reg::R8, x64::Reg::R8);             // share mode = 0
        emit_.xor_(x64::Reg::R9, x64::Reg::R9);             // security attrs = NULL
        emit_.mov_smart(x64::Reg::RAX, 2);                  // CREATE_ALWAYS
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_smart(x64::Reg::RAX, 0x80);               // FILE_ATTRIBUTE_NORMAL
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);           // template = NULL
        emit_.mov_store(x64::Reg::RSP, 48, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateFileA);
        emit_.mov_store(x64::Reg::RSP, 56, x64::Reg::RAX);  // handle
        emit_.cmp_imm(x64::Reg::RAX, -1);
        std::size_t create_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 72);   // content
        emit_inline_strlen();
        emit_.mov_store(x64::Reg::RSP, 64, x64::Reg::RAX);  // len

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, 72);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 64);
        emit_.lea(x64::Reg::R9, x64::Reg::RSP, 40);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);  // overlapped = NULL
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);  // bytesWritten = 0
        emit_iat_call_raw(pe::iat::WriteFile);
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t write_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 40);
        emit_.add_smart(x64::Reg::RSP, 96);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(write_fail);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_iat_call_raw(pe::iat::CloseHandle);

        emit_.patch_jump(create_fail);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 96);

        emit_.patch_jump(done);
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

    [[nodiscard]] bool generate_dll_string_get_char(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("string_get_char requires str and index arguments");
            return false;
        }

        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // save index

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // rcx = str ptr
        emit_.pop(x64::Reg::RDX);                 // rdx = index

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);  // default result = 0
        emit_.test(x64::Reg::RCX, x64::Reg::RCX);
        std::size_t null_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.cmp_imm(x64::Reg::RDX, 0);
        std::size_t neg_done = emit_.jcc_rel32(x64::Emitter::CC_L);

        std::size_t loop_top = emit_.buffer().pos();
        emit_.cmp_imm(x64::Reg::RDX, 0);
        std::size_t read_char = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x39); emit_.buffer().emit8(0x00); // cmp byte [rcx], 0
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1); // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xCA); // dec rdx
        emit_.jmp_rel32(static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5));

        emit_.patch_jump(read_char);
        emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x39); emit_.buffer().emit8(0x00); // cmp byte [rcx], 0
        std::size_t done_after_read_cmp = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x01); // movzx rax, byte [rcx]

        emit_.patch_jump(done_after_read_cmp);
        emit_.patch_jump(done);
        emit_.patch_jump(neg_done);
        emit_.patch_jump(null_done);
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
        emit_.add_smart(x64::Reg::R8, 1);                     // r8 = len_a + len_b + 1
        emit_.push(x64::Reg::R8);  // save total_size (need it? no, but keeps stack aligned for reasoning)
        // stack: [str_a, str_b, len_a, len_b, total_size]

        // GetProcessHeap
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_smart(x64::Reg::RSP, 32);
        // rax = heap handle

        // HeapAlloc(heap, 0, total_size)
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // total_size -> r8
        // stack: [str_a, str_b, len_a, len_b]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
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
        emit_.add_smart(x64::Reg::RSP, 32);  // pop len_b, len_a, str_b, str_a
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
        emit_.add_smart(x64::Reg::RSP, 32);

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.mov_imm32(x64::Reg::R8, 24);  // 24 bytes
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
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

    [[nodiscard]] bool generate_dll_make_string(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("make_string requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // source ptr

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_inline_strlen();
        emit_.push(x64::Reg::RAX);  // len

        emit_.mov(x64::Reg::R8, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::R8, 1);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_smart(x64::Reg::RSP, 32);

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RDX, x64::Reg::RDX);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);

        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t alloc_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.push(x64::Reg::RAX);                    // save dst for return
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);      // dst
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 16);  // src
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, 8);    // len
        emit_inline_memcpy();
        emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x02); emit_.buffer().emit8(0x00);  // mov byte [rdx], 0
        emit_.pop(x64::Reg::RAX);                     // return cloned string
        emit_.add_smart(x64::Reg::RSP, 16);           // drop len + src
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(alloc_fail);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 16);           // drop len + src

        emit_.patch_jump(done);
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

    [[nodiscard]] bool generate_dll_string_starts_with(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("string_starts_with requires two string arguments");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        emit_.pop(x64::Reg::RCX);  // str
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);  // prefix

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.test(x64::Reg::RCX, x64::Reg::RCX);
        std::size_t null_fail = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.test(x64::Reg::RDX, x64::Reg::RDX);
        std::size_t prefix_null_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        std::size_t loop_top = emit_.buffer().pos();
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x02);  // movzx r8, byte [rdx]
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x09);  // movzx r9, byte [rcx]

        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x85);
        emit_.buffer().emit8(0xC0);  // test r8, r8
        std::size_t prefix_done = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.buffer().emit8(0x4D); emit_.buffer().emit8(0x39);
        emit_.buffer().emit8(0xC8);  // cmp r8, r9
        std::size_t mismatch = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC2);
        std::int32_t loop_rel = static_cast<std::int32_t>(
            loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        emit_.patch_jump(mismatch);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(prefix_done);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        std::size_t success_done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(null_fail);
        emit_.patch_jump(prefix_null_fail);
        emit_.patch_jump(done);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        emit_.patch_jump(success_done);
        return true;
    }

    [[nodiscard]] bool generate_dll_is_alpha(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("is_alpha requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        emit_.cmp_imm(x64::Reg::RCX, 'a');
        std::size_t check_upper = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, 'z');
        std::size_t lower_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);

        emit_.patch_jump(check_upper);
        emit_.cmp_imm(x64::Reg::RCX, 'A');
        std::size_t check_underscore = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, 'Z');
        std::size_t upper_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);

        emit_.patch_jump(check_underscore);
        emit_.cmp_imm(x64::Reg::RCX, '_');
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        std::size_t after = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(lower_ok);
        emit_.patch_jump(upper_ok);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        emit_.patch_jump(done);
        emit_.patch_jump(after);
        return true;
    }

    [[nodiscard]] bool generate_dll_is_digit(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("is_digit requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.cmp_imm(x64::Reg::RCX, '0');
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, '9');
        std::size_t success = emit_.jcc_rel32(x64::Emitter::CC_LE);
        std::size_t after = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(success);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        emit_.patch_jump(done);
        emit_.patch_jump(after);
        return true;
    }

    [[nodiscard]] bool generate_dll_is_alnum(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("is_alnum requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        emit_.cmp_imm(x64::Reg::RCX, '0');
        std::size_t check_alpha = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, '9');
        std::size_t digit_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);

        emit_.patch_jump(check_alpha);
        emit_.cmp_imm(x64::Reg::RCX, 'a');
        std::size_t check_upper = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, 'z');
        std::size_t lower_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);

        emit_.patch_jump(check_upper);
        emit_.cmp_imm(x64::Reg::RCX, 'A');
        std::size_t check_underscore = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RCX, 'Z');
        std::size_t upper_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);

        emit_.patch_jump(check_underscore);
        emit_.cmp_imm(x64::Reg::RCX, '_');
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        std::size_t after = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(digit_ok);
        emit_.patch_jump(lower_ok);
        emit_.patch_jump(upper_ok);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        emit_.patch_jump(done);
        emit_.patch_jump(after);
        return true;
    }

    [[nodiscard]] bool generate_dll_is_whitespace(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("is_whitespace requires an argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        emit_.cmp_imm(x64::Reg::RCX, ' ');
        std::size_t success_space = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.cmp_imm(x64::Reg::RCX, '\t');
        std::size_t success_tab = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.cmp_imm(x64::Reg::RCX, '\n');
        std::size_t success_newline = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.cmp_imm(x64::Reg::RCX, '\r');
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.patch_jump(success_space);
        emit_.patch_jump(success_tab);
        emit_.patch_jump(success_newline);
        emit_.mov_imm32(x64::Reg::RAX, 1);
        emit_.patch_jump(done);
        return true;
    }

    [[nodiscard]] bool generate_dll_parse_int(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("parse_int requires a string argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);    // result
        emit_.xor_(x64::Reg::R8, x64::Reg::R8);      // negative flag
        emit_.test(x64::Reg::RCX, x64::Reg::RCX);
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x11); // movzx rdx, byte [rcx]
        emit_.cmp_imm(x64::Reg::RDX, '-');
        std::size_t check_plus = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_imm32(x64::Reg::R8, 1);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1); // inc rcx
        std::size_t sign_done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(check_plus);
        emit_.cmp_imm(x64::Reg::RDX, '+');
        std::size_t load_first = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1); // inc rcx
        emit_.patch_jump(sign_done);
        emit_.patch_jump(load_first);

        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x11); // movzx rdx, byte [rcx]
        emit_.cmp_imm(x64::Reg::RDX, '0');
        std::size_t invalid = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RDX, '9');
        std::size_t first_digit_ok = emit_.jcc_rel32(x64::Emitter::CC_LE);
        std::size_t invalid_after_cmp = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(first_digit_ok);
        std::size_t loop_top = emit_.buffer().pos();
        emit_.mov(x64::Reg::R9, x64::Reg::RDX);
        emit_.sub_imm(x64::Reg::R9, '0');
        emit_.imul_imm(x64::Reg::RAX, x64::Reg::RAX, 10);
        emit_.add(x64::Reg::RAX, x64::Reg::R9);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xFF); emit_.buffer().emit8(0xC1); // inc rcx
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xB6); emit_.buffer().emit8(0x11); // movzx rdx, byte [rcx]
        emit_.cmp_imm(x64::Reg::RDX, '0');
        std::size_t finish = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.cmp_imm(x64::Reg::RDX, '9');
        std::size_t next_digit = emit_.jcc_rel32(x64::Emitter::CC_LE);
        std::size_t sign_fix = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(next_digit);
        std::int32_t loop_rel = static_cast<std::int32_t>(
            loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        emit_.patch_jump(invalid);
        emit_.patch_jump(invalid_after_cmp);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t invalid_done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(finish);
        emit_.patch_jump(sign_fix);
        emit_.cmp_imm(x64::Reg::R8, 0);
        std::size_t positive_done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xF7); emit_.buffer().emit8(0xD8); // neg rax
        emit_.patch_jump(positive_done);
        emit_.patch_jump(done);
        emit_.patch_jump(invalid_done);
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
        emit_.add_smart(x64::Reg::R8, 1);            // r8 = len + 1
        emit_.push(x64::Reg::R8);  // save alloc_size
        // stack: [str, start, len, alloc_size]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_smart(x64::Reg::RSP, 32);
        // rax = heap handle

        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);  // hHeap
        emit_.xor_32(x64::Reg::EDX, x64::Reg::EDX);
        emit_.pop(x64::Reg::R8);  // alloc_size
        // stack: [str, start, len]

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);
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
        emit_.add_smart(x64::Reg::RSP, 24);  // pop len, start, str

        return true;
    }

    [[nodiscard]] bool generate_dll_buffer_new(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("buffer_new requires a capacity argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.cmp_imm(x64::Reg::RAX, 0);
        std::size_t invalid = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.push(x64::Reg::RAX);  // capacity
        emit_.add_smart(x64::Reg::RAX, 2);
        emit_.buffer().emit8(0x48); emit_.buffer().emit8(0xC1);
        emit_.buffer().emit8(0xE0); emit_.buffer().emit8(0x03); // shl rax, 3
        emit_.push(x64::Reg::RAX);  // total bytes

        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::GetProcessHeap);
        emit_.add_smart(x64::Reg::RSP, 32);
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.mov_imm32(x64::Reg::RDX, 8); // HEAP_ZERO_MEMORY
        emit_.pop(x64::Reg::R8);
        emit_.sub_imm(x64::Reg::RSP, 32);
        emit_iat_call_raw(pe::iat::HeapAlloc);
        emit_.add_smart(x64::Reg::RSP, 32);

        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t alloc_fail = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.pop(x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RCX);
        emit_.add_smart(x64::Reg::RAX, 8);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(alloc_fail);
        emit_.add_smart(x64::Reg::RSP, 8);
        emit_.patch_jump(invalid);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.patch_jump(done);
        return true;
    }

    [[nodiscard]] bool generate_dll_buffer_push(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("buffer_push requires buffer and byte arguments");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RCX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.test(x64::Reg::RCX, x64::Reg::RCX);
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RCX, 0);
        emit_.mov_load(x64::Reg::R9, x64::Reg::RCX, -8);
        emit_.cmp(x64::Reg::R8, x64::Reg::R9);
        std::size_t full = emit_.jcc_rel32(x64::Emitter::CC_GE);
        emit_.lea_scaled(x64::Reg::RAX, x64::Reg::RCX, x64::Reg::R8, 8);
        emit_.add_smart(x64::Reg::RAX, 8);
        emit_.mov_store(x64::Reg::RAX, 0, x64::Reg::RDX);
        emit_.add_smart(x64::Reg::R8, 1);
        emit_.mov_store(x64::Reg::RCX, 0, x64::Reg::R8);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.patch_jump(done);
        emit_.patch_jump(full);
        return true;
    }

    [[nodiscard]] bool generate_dll_buffer_len(const ast::CallExpr& call) {
        if (call.args.empty()) {
            error("buffer_len requires a buffer argument");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t done = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        emit_.patch_jump(done);
        return true;
    }

    [[nodiscard]] bool generate_dll_write_bytes(const ast::CallExpr& call) {
        if (call.args.size() < 3) {
            error("write_bytes requires path, buffer, and len arguments");
            return false;
        }

        if (!generate_expr(*call.args[0])) return false;
        emit_.push(x64::Reg::RAX);  // path
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);  // buffer
        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);  // len

        constexpr std::int32_t alloc = 88;
        constexpr std::int32_t total_cleanup = alloc + 24;
        emit_.sub_imm(x64::Reg::RSP, alloc);

        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, alloc + 8);   // buffer
        emit_.mov_store(x64::Reg::RSP, 64, x64::Reg::RDX);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, alloc);        // len
        emit_.mov_store(x64::Reg::RSP, 72, x64::Reg::R8);
        emit_.test(x64::Reg::RDX, x64::Reg::RDX);
        std::size_t fail = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_.cmp_imm(x64::Reg::R8, 0);
        std::size_t fail_neg = emit_.jcc_rel32(x64::Emitter::CC_L);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RDX, 0);
        emit_.cmp(x64::Reg::RAX, x64::Reg::R8);
        std::size_t len_ok = emit_.jcc_rel32(x64::Emitter::CC_GE);
        std::size_t fail_short = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(len_ok);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, alloc + 16); // path
        emit_.mov_imm64(x64::Reg::RDX, 0x40000000ull);            // GENERIC_WRITE
        emit_.xor_(x64::Reg::R8, x64::Reg::R8);                   // share mode
        emit_.xor_(x64::Reg::R9, x64::Reg::R9);                   // security attrs
        emit_.mov_imm32(x64::Reg::RAX, 2);                        // CREATE_ALWAYS
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);
        emit_.mov_imm32(x64::Reg::RAX, 0x80);                     // FILE_ATTRIBUTE_NORMAL
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);                 // template
        emit_.mov_store(x64::Reg::RSP, 48, x64::Reg::RAX);
        emit_iat_call_raw(pe::iat::CreateFileA);
        emit_.mov_store(x64::Reg::RSP, 56, x64::Reg::RAX);        // handle
        emit_.cmp_imm(x64::Reg::RAX, -1);
        std::size_t open_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 80, x64::Reg::RAX);        // idx = 0

        std::size_t loop_top = emit_.buffer().pos();
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 80);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 72);
        emit_.cmp(x64::Reg::RAX, x64::Reg::RCX);
        std::size_t loop_done = emit_.jcc_rel32(x64::Emitter::CC_GE);

        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, 64);
        emit_.lea_scaled(x64::Reg::RAX, x64::Reg::RDX, x64::Reg::RAX, 8);
        emit_.add_smart(x64::Reg::RAX, 8);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RAX, 0);
        emit_.mov_store(x64::Reg::RSP, 48, x64::Reg::RAX);        // temp byte cell

        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);         // handle
        emit_.lea(x64::Reg::RDX, x64::Reg::RSP, 48);              // &temp byte
        emit_.mov_imm32(x64::Reg::R8, 1);
        emit_.lea(x64::Reg::R9, x64::Reg::RSP, 40);               // &written
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::RSP, 32, x64::Reg::RAX);        // overlapped = null
        emit_.mov_store(x64::Reg::RSP, 40, x64::Reg::RAX);        // written = 0
        emit_iat_call_raw(pe::iat::WriteFile);
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t write_fail = emit_.jcc_rel32(x64::Emitter::CC_E);

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 80);
        emit_.add_smart(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RSP, 80, x64::Reg::RAX);
        std::int32_t loop_rel = static_cast<std::int32_t>(
            loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);

        emit_.patch_jump(loop_done);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, 72);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(write_fail);
        emit_.patch_jump(open_fail);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, 56);
        emit_.cmp_imm(x64::Reg::RCX, -1);
        std::size_t skip_close = emit_.jcc_rel32(x64::Emitter::CC_E);
        emit_iat_call_raw(pe::iat::CloseHandle);
        emit_.patch_jump(skip_close);

        emit_.patch_jump(fail);
        emit_.patch_jump(fail_neg);
        emit_.patch_jump(fail_short);
        emit_.mov_imm32(x64::Reg::RAX, -1);

        emit_.patch_jump(done);
        emit_.add_smart(x64::Reg::RSP, total_cleanup);
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
        emit_.add_smart(x64::Reg::RSP, 32);

        // clean up our 16-byte string space
        emit_.add_smart(x64::Reg::RSP, 16);

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
        
        emit_.add_smart(x64::Reg::RSP, 48);
        
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
        
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
        
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
        
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
        if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
            error("scan pattern is too large");
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
        
        std::int32_t pattern_len = static_cast<std::int32_t>(bytes.size());
        std::int32_t simd_window = static_cast<std::int32_t>((std::max)(bytes.size(), anchor_idx + 16));

        // R12 = last candidate start that is safe for both the full pattern and the 16-byte anchor load
        emit_.mov(x64::Reg::R12, x64::Reg::RDI);
        emit_.add(x64::Reg::R12, x64::Reg::RSI);
        emit_.sub_imm(x64::Reg::R12, simd_window);
        
        // RDX = absolute end for scalar fallback
        emit_.mov(x64::Reg::RDX, x64::Reg::RDI);
        emit_.add(x64::Reg::RDX, x64::Reg::RSI);
        emit_.sub_imm(x64::Reg::RDX, pattern_len);
        
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
        } else if (anchor_idx < 128) {
            emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x7F);
            emit_.buffer().emit8(static_cast<std::uint8_t>(anchor_idx));
        } else {
            emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xBF);
            emit_.buffer().emit32(static_cast<std::uint32_t>(anchor_idx));
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
                } else if (j < 128) {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0x7F);
                    emit_.buffer().emit8(static_cast<std::uint8_t>(j));
                } else {
                    emit_.buffer().emit8(0x80); emit_.buffer().emit8(0xBF);
                    emit_.buffer().emit32(static_cast<std::uint32_t>(j));
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
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
        
        emit_.add_smart(x64::Reg::RSP, 32);
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
                if (is_native_image_output() && rt_.print_str) {
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
                    } else if (is_native_image_output()) {
                        // native-image output: call print stub
                        emit_.buffer().emit8(0xE8);  // call rel32
                        std::int32_t rel = static_cast<std::int32_t>(0x100) - 
                            static_cast<std::int32_t>(0x240 + emit_.buffer().pos() + 4);
                        emit_.buffer().emit32(static_cast<std::uint32_t>(rel));
                    }
                    emit_.add_smart(x64::Reg::RSP, 32);
                    continue;
                }
            }
            
            // Integer/other types - use print_int
            if (!generate_expr(*arg)) return false;
            
            if (is_native_image_output()) {
                // native-image output: replicate what generate_dll_builtin_print_hex does
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
                
                emit_.add_smart(x64::Reg::RSP, 64);
            } else if (rt_.print_int) {
                emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.print_int));
                emit_.call(x64::Reg::RAX);
                emit_.add_smart(x64::Reg::RSP, 32);
            }
        }
        
        // Add newline if println
        if (add_newline) {
            if (is_native_image_output()) {
                // Allocate newline string on stack
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x04);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x0A);  // mov byte [rsp], '\n'
                emit_.buffer().emit8(0xC6); emit_.buffer().emit8(0x44);
                emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x01);
                emit_.buffer().emit8(0x00);  // mov byte [rsp+1], 0
                emit_.mov(x64::Reg::RCX, x64::Reg::RSP);
                
                emit_startup_call(print_offset_);
                
                emit_.add_smart(x64::Reg::RSP, 32);
            } else if (rt_.print_str) {
                // host fallback has no dedicated newline helper
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

        for (std::size_t i = 0; i < 2; ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }

        constexpr std::int32_t alloc = 32;
        emit_.sub_imm(x64::Reg::RSP, alloc);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, alloc + 8);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, alloc);
        
        if (fn_ptr) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_smart(x64::Reg::RSP, alloc + 16);
        return true;
    }

    // Three-argument builtin (like array_set)
    [[nodiscard]] bool generate_builtin_three_arg(const ast::CallExpr& call, void* fn_ptr) {
        if (call.args.size() < 3) {
            error("builtin requires three arguments");
            return false;
        }

        for (std::size_t i = 0; i < 3; ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }

        constexpr std::int32_t alloc = 40;
        emit_.sub_imm(x64::Reg::RSP, alloc);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, alloc + 16);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, alloc + 8);
        emit_.mov_load(x64::Reg::R8, x64::Reg::RSP, alloc);
        
        if (fn_ptr) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_smart(x64::Reg::RSP, alloc + 24);
        return true;
    }

    // string_get_char(handle, index) - two args
    [[nodiscard]] bool generate_builtin_string_get_char(const ast::CallExpr& call) {
        if (call.args.size() < 2) {
            error("string_get_char requires handle and index arguments");
            return false;
        }

        for (std::size_t i = 0; i < 2; ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }

        constexpr std::int32_t alloc = 32;
        emit_.sub_imm(x64::Reg::RSP, alloc);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RSP, alloc + 8);
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RSP, alloc);
        
        if (rt_.string_get_char) {
            emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(rt_.string_get_char));
            emit_.call(x64::Reg::RAX);
        }
        
        emit_.add_smart(x64::Reg::RSP, alloc + 16);
        // RAX has the character

        return true;
    }

    void emit_cpuid() {
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0xA2);
    }

    void emit_xgetbv() {
        emit_.buffer().emit8(0x0F);
        emit_.buffer().emit8(0x01);
        emit_.buffer().emit8(0xD0);
    }

    void emit_vzeroupper() {
        emit_.buffer().emit8(0xC5);
        emit_.buffer().emit8(0xF8);
        emit_.buffer().emit8(0x77);
    }

    [[nodiscard]] bool generate_builtin_simd_feature(std::int32_t xcr0_mask, std::int32_t leaf7_ebx_mask) {
        emit_.push(x64::Reg::RBX);

        emit_.mov_imm32(x64::Reg::EAX, 1);
        emit_.xor_32(x64::Reg::ECX, x64::Reg::ECX);
        emit_cpuid();

        emit_.mov(x64::Reg::R10, x64::Reg::RCX);
        emit_.and_imm(x64::Reg::R10, 0x18000000);
        emit_.cmp_imm(x64::Reg::R10, 0x18000000);
        std::size_t missing_avx = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.xor_32(x64::Reg::ECX, x64::Reg::ECX);
        emit_xgetbv();
        emit_.mov(x64::Reg::R10, x64::Reg::RAX);
        emit_.and_imm(x64::Reg::R10, xcr0_mask);
        emit_.cmp_imm(x64::Reg::R10, xcr0_mask);
        std::size_t missing_xstate = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.mov_imm32(x64::Reg::EAX, 7);
        emit_.xor_32(x64::Reg::ECX, x64::Reg::ECX);
        emit_cpuid();
        emit_.mov(x64::Reg::R10, x64::Reg::RBX);
        emit_.and_imm(x64::Reg::R10, leaf7_ebx_mask);
        emit_.cmp_imm(x64::Reg::R10, leaf7_ebx_mask);
        std::size_t missing_leaf7 = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.mov_imm32(x64::Reg::EAX, 1);
        std::size_t done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(missing_avx);
        emit_.patch_jump(missing_xstate);
        emit_.patch_jump(missing_leaf7);
        emit_.mov_imm32(x64::Reg::EAX, 0);

        emit_.patch_jump(done);
        emit_.pop(x64::Reg::RBX);
        return true;
    }

    [[nodiscard]] bool generate_simple_pure_expr_into_no_scratch(const ast::Expr& expr, x64::Reg dst) {
        if (auto* lit = std::get_if<ast::LiteralExpr>(&expr.kind)) {
            std::int64_t imm = 0;
            if (try_get_i64_immediate(*lit, imm)) {
                emit_.mov_smart(dst, imm);
                return true;
            }
            if (auto* u = std::get_if<std::uint64_t>(&lit->value)) {
                emit_.mov_imm64(dst, *u);
                return true;
            }
            if (auto* d = std::get_if<double>(&lit->value)) {
                emit_.mov_imm64(dst, std::bit_cast<std::uint64_t>(*d));
                return true;
            }
            return false;
        }

        if (auto* ident = std::get_if<ast::IdentExpr>(&expr.kind)) {
            return generate_bound_ident_into(*ident, dst);
        }

        if (auto* field = std::get_if<ast::FieldExpr>(&expr.kind)) {
            return generate_pure_field_into(*field, dst);
        }

        if (auto* un = std::get_if<ast::UnaryExpr>(&expr.kind)) {
            if (!generate_simple_pure_expr_into_no_scratch(*un->operand, dst)) return false;
            using Op = ast::UnaryExpr::Op;
            switch (un->op) {
                case Op::Neg:
                    emit_.neg(dst);
                    return true;
                case Op::Not:
                    emit_.test(dst, dst);
                    emit_bool_result(x64::Emitter::CC_E, dst);
                    return true;
                case Op::BitNot:
                    emit_.not_(dst);
                    return true;
                default:
                    return false;
            }
        }

        if (auto* bin = std::get_if<ast::BinaryExpr>(&expr.kind)) {
            std::int64_t rhs_imm = 0;
            if (!try_get_i64_immediate(*bin->rhs, rhs_imm) || !supports_binary_rhs_imm(bin->op, rhs_imm)) {
                return false;
            }
            if (!generate_simple_pure_expr_into_no_scratch(*bin->lhs, dst)) return false;
            return emit_binary_into_with_rhs_imm(dst, bin->op, rhs_imm);
        }

        return false;
    }

    [[nodiscard]] bool generate_builtin_simd_has_avx2() {
        return generate_builtin_simd_feature(0x6, 1 << 5);
    }

    [[nodiscard]] bool generate_builtin_simd_has_avx512f() {
        return generate_builtin_simd_feature(0xE6, 1 << 16);
    }

    [[nodiscard]] bool generate_builtin_simd_has_avx512dq() {
        return generate_builtin_simd_feature(0xE6, (1 << 16) | (1 << 17));
    }

    [[nodiscard]] bool generate_builtin_simd_ternary_ptr(const ast::CallExpr& call, std::span<const std::uint8_t> bytes, std::string_view name) {
        if (call.args.size() < 3) {
            error(std::format("{} requires dst, a, and b arguments", name));
            return false;
        }

        if (generate_simple_pure_expr_into_no_scratch(*call.args[2], x64::Reg::R8) &&
            generate_simple_pure_expr_into_no_scratch(*call.args[1], x64::Reg::RDX) &&
            generate_simple_pure_expr_into_no_scratch(*call.args[0], x64::Reg::RCX)) {
            emit_.buffer().emit_bytes(bytes);
            return true;
        }

        if (!generate_expr(*call.args[2])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.pop(x64::Reg::R8);
        emit_.buffer().emit_bytes(bytes);
        return true;
    }

    [[nodiscard]] bool generate_builtin_simd_binary_ptr_i64(const ast::CallExpr& call, std::span<const std::uint8_t> bytes, std::string_view name) {
        if (call.args.size() < 2) {
            error(std::format("{} requires dst and value arguments", name));
            return false;
        }

        if (generate_simple_pure_expr_into_no_scratch(*call.args[1], x64::Reg::RDX) &&
            generate_simple_pure_expr_into_no_scratch(*call.args[0], x64::Reg::RCX)) {
            emit_.buffer().emit_bytes(bytes);
            return true;
        }

        if (!generate_expr(*call.args[1])) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*call.args[0])) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RDX);
        emit_.buffer().emit_bytes(bytes);
        return true;
    }

    [[nodiscard]] bool generate_builtin_simd_i32x8_add(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xFE, 0x6F, 0x02,
            0xC4, 0xC1, 0x7D, 0xFE, 0x08,
            0xC5, 0xFE, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x8_add");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x8_sub(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xFE, 0x6F, 0x02,
            0xC4, 0xC1, 0x7D, 0xFA, 0x08,
            0xC5, 0xFE, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x8_sub");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x8_mul(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xFE, 0x6F, 0x02,
            0xC4, 0xC2, 0x7D, 0x40, 0x08,
            0xC5, 0xFE, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x8_mul");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x4_add(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xFE, 0x6F, 0x02,
            0xC4, 0xC1, 0x7D, 0xD4, 0x08,
            0xC5, 0xFE, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i64x4_add");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x4_sub(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xFE, 0x6F, 0x02,
            0xC4, 0xC1, 0x7D, 0xFB, 0x08,
            0xC5, 0xFE, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i64x4_sub");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x8_splat(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 13> kBytes = {
            0xC5, 0xF9, 0x6E, 0xC2,
            0xC4, 0xE2, 0x7D, 0x58, 0xC0,
            0xC5, 0xFE, 0x7F, 0x01
        };
        return generate_builtin_simd_binary_ptr_i64(call, kBytes, "simd_i32x8_splat");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x4_splat(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 14> kBytes = {
            0xC4, 0xE1, 0xF9, 0x6E, 0xC2,
            0xC4, 0xE2, 0x7D, 0x59, 0xC0,
            0xC5, 0xFE, 0x7F, 0x01
        };
        return generate_builtin_simd_binary_ptr_i64(call, kBytes, "simd_i64x4_splat");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x16_add(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 18> kBytes = {
            0x62, 0xF1, 0x7E, 0x48, 0x6F, 0x02,
            0x62, 0xD1, 0x7D, 0x48, 0xFE, 0x08,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x16_add");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x16_sub(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 18> kBytes = {
            0x62, 0xF1, 0x7E, 0x48, 0x6F, 0x02,
            0x62, 0xD1, 0x7D, 0x48, 0xFA, 0x08,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x16_sub");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x16_mul(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 18> kBytes = {
            0x62, 0xF1, 0x7E, 0x48, 0x6F, 0x02,
            0x62, 0xD2, 0x7D, 0x48, 0x40, 0x08,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i32x16_mul");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x8_add(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 18> kBytes = {
            0x62, 0xF1, 0x7E, 0x48, 0x6F, 0x02,
            0x62, 0xD1, 0xFD, 0x48, 0xD4, 0x08,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i64x8_add");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x8_sub(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 18> kBytes = {
            0x62, 0xF1, 0x7E, 0x48, 0x6F, 0x02,
            0x62, 0xD1, 0xFD, 0x48, 0xFB, 0x08,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x09
        };
        return generate_builtin_simd_ternary_ptr(call, kBytes, "simd_i64x8_sub");
    }

    [[nodiscard]] bool generate_builtin_simd_i32x16_splat(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 12> kBytes = {
            0x62, 0xF2, 0x7D, 0x48, 0x7C, 0xC2,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x01
        };
        return generate_builtin_simd_binary_ptr_i64(call, kBytes, "simd_i32x16_splat");
    }

    [[nodiscard]] bool generate_builtin_simd_i64x8_splat(const ast::CallExpr& call) {
        static constexpr std::array<std::uint8_t, 12> kBytes = {
            0x62, 0xF2, 0xFD, 0x48, 0x7C, 0xC2,
            0x62, 0xF1, 0x7E, 0x48, 0x7F, 0x01
        };
        return generate_builtin_simd_binary_ptr_i64(call, kBytes, "simd_i64x8_splat");
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

    void* get_ffi_fn(const std::string& name) {
        static const std::unordered_map<std::string_view, void*> ffi_table = {
            {"get_module",              reinterpret_cast<void*>(&opus_get_module)},
            {"load_library",            reinterpret_cast<void*>(&opus_load_library)},
            {"get_proc",                reinterpret_cast<void*>(&opus_get_proc)},
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
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_one_arg(const ast::CallExpr& call, const std::string& fn_name) {
        if (is_native_image_output()) {
            if (fn_name == "load_library") {
                if (call.args.empty()) {
                    emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
                } else {
                    if (!generate_expr(*call.args[0])) return false;
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                }
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_iat_call_raw(pe::iat::LoadLibraryA);
                emit_.add_smart(x64::Reg::RSP, 32);
                return true;
            }
            if (fn_name == "get_module") {
                if (call.args.empty()) {
                    emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
                } else {
                    if (!generate_expr(*call.args[0])) return false;
                    emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                }
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_iat_call_raw(pe::iat::GetModuleHandleA);
                emit_.add_smart(x64::Reg::RSP, 32);
                return true;
            }
        }

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
        emit_.add_smart(x64::Reg::RSP, 32);
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_host_fixed_call(
        const ast::CallExpr& call,
        void* fn_ptr,
        std::size_t arg_count,
        const std::string& fn_name
    ) {
        if (!fn_ptr) {
            error(std::format("unknown FFI function: {}", fn_name));
            return false;
        }
        if (call.args.size() < arg_count) {
            error(std::format("{} requires {} arguments", fn_name, arg_count));
            return false;
        }

        for (std::size_t i = 0; i < arg_count; ++i) {
            if (!generate_expr(*call.args[i])) return false;
            emit_.push(x64::Reg::RAX);
        }

        std::size_t reg_args = arg_count < 4 ? arg_count : 4;
        std::size_t stack_args = arg_count > 4 ? arg_count - 4 : 0;
        std::size_t alloc = 32 + stack_args * 8;
        if (stack_args == 0 && (arg_count & 1) != 0) {
            alloc += 8;
        }

        emit_.sub_imm(x64::Reg::RSP, static_cast<std::int32_t>(alloc));

        for (std::size_t i = 0; i < reg_args; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (arg_count - 1 - i) * 8);
            emit_.mov_load(x64::ARG_REGS[i], x64::Reg::RSP, src_off);
        }

        for (std::size_t i = 4; i < arg_count; ++i) {
            std::int32_t src_off = static_cast<std::int32_t>(alloc + (arg_count - 1 - i) * 8);
            std::int32_t dest_off = static_cast<std::int32_t>(32 + (i - 4) * 8);
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RSP, src_off);
            emit_.mov_store(x64::Reg::RSP, dest_off, x64::Reg::RAX);
        }

        emit_.mov_imm64(x64::Reg::RAX, reinterpret_cast<std::uint64_t>(fn_ptr));
        emit_.call(x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, static_cast<std::int32_t>(alloc + arg_count * 8));
        return true;
    }

    [[nodiscard]] bool generate_builtin_ffi_fixed_arg(
        const ast::CallExpr& call,
        const std::string& fn_name,
        std::size_t arg_count
    ) {
        void* fn_ptr = get_ffi_fn(fn_name);
        return generate_builtin_ffi_host_fixed_call(call, fn_ptr, arg_count, fn_name);
    }

    [[nodiscard]] bool generate_builtin_ffi_two_arg(const ast::CallExpr& call, const std::string& fn_name) {
        if (is_native_image_output()) {
            if (fn_name == "get_proc") {
                if (call.args.size() < 2) {
                    error(std::format("{} requires 2 arguments", fn_name));
                    return false;
                }
                if (!generate_expr(*call.args[1])) return false;
                emit_.push(x64::Reg::RAX);
                if (!generate_expr(*call.args[0])) return false;
                emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
                emit_.pop(x64::Reg::RDX);
                emit_.sub_imm(x64::Reg::RSP, 32);
                emit_iat_call_raw(pe::iat::GetProcAddress);
                emit_.add_smart(x64::Reg::RSP, 32);
                return true;
            }
        }

        return generate_builtin_ffi_fixed_arg(call, fn_name, 2);
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
        emit_.mov_load(x64::Reg::R9, x64::Reg::R12, CTX_ARG3);
        emit_.call(x64::Reg::RAX);

        // store result
        emit_.mov_store(x64::Reg::R12, CTX_RESULT, x64::Reg::RAX);

        // return 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.add_smart(x64::Reg::RSP, 0x28);
        emit_.pop(x64::Reg::R12);
        emit_.pop(x64::Reg::RBP);
        emit_.ret();

        return stub_offset;
    }

    // spawn expression codegen
    // allocates Thread_Context and result storage on the heap, then calls CreateThread
    [[nodiscard]] bool generate_spawn(const ast::SpawnExpr& spawn) {
        auto fn_name_opt = resolve_spawn_target_name(spawn);
        if (!fn_name_opt) {
            error("spawn requires a named function call, for example: spawn worker()");
            return false;
        }
        const std::string& fn_name = *fn_name_opt;

        auto fn_it = functions_.find(fn_name);
        if (fn_it == functions_.end()) {
            error_undefined_name("function", fn_name, collect_function_name_candidates());
            return false;
        }

        if (!fn_it->second.return_type.is_integer()) {
            error(std::format(
                "spawned function '{}' must return an integer-compatible value",
                fn_name));
            return false;
        }

        if (spawn.args.size() != fn_it->second.param_types.size()) {
            error(std::format(
                "spawn of '{}' requires {} arguments, got {}",
                fn_name,
                fn_it->second.param_types.size(),
                spawn.args.size()));
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

        if (spawn.args.size() > 4) {
            error("spawn currently supports at most 4 arguments");
            return false;
        }

        std::size_t sid = spawn_counter_++;

        // evaluate args first (before we fill the context, since eval can trash regs)
        std::vector<std::int32_t> arg_temps;
        for (std::size_t i = 0; i < spawn.args.size(); ++i) {
            if (!generate_expr(*spawn.args[i])) return false;
            auto& tmp = current_scope_->define("__spawn_arg_" + std::to_string(i) + "_" + std::to_string(sid),
                Type::make_primitive(PrimitiveType::I64), true);
            emit_.mov_store(x64::Reg::RBP, tmp.stack_offset, x64::Reg::RAX);
            arg_temps.push_back(tmp.stack_offset);
        }

        if (!emit_runtime_alloc_bytes(CTX_SIZE)) return false;
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t ctx_alloc_ok = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t ctx_alloc_fail_exit = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(ctx_alloc_ok);
        auto& ctx_ptr_sym = current_scope_->define("__spawn_ctxptr_" + std::to_string(sid),
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, ctx_ptr_sym.stack_offset, x64::Reg::RAX);
        emit_.mov(x64::Reg::R10, x64::Reg::RAX);

        std::size_t fn_addr_fixup = emit_lea_rip_disp32(x64::Reg::RAX);
        call_fixups_.push_back({fn_addr_fixup, fn_name});
        emit_.mov_store(x64::Reg::R10, CTX_FN_PTR, x64::Reg::RAX);

        // fill args into context slots
        for (std::size_t i = 0; i < arg_temps.size(); ++i) {
            emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, arg_temps[i]);
            emit_.mov_store(x64::Reg::R10, static_cast<std::int32_t>((i + 1) * 8), x64::Reg::RAX);
        }

        // zero result slot
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        emit_.mov_store(x64::Reg::R10, CTX_RESULT, x64::Reg::RAX);

        if (!emit_runtime_alloc_bytes(16)) return false;
        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t result_alloc_ok = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ctx_ptr_sym.stack_offset);
        if (!emit_runtime_free_reg(x64::Reg::RCX)) return false;
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t result_alloc_fail_exit = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(result_alloc_ok);
        auto& result_ptr_sym = current_scope_->define("__spawn_result_" + std::to_string(sid),
            Type::make_primitive(PrimitiveType::I64), true);
        emit_.mov_store(x64::Reg::RBP, result_ptr_sym.stack_offset, x64::Reg::RAX);

        // set up CreateThread args
        // r8 = stub address
        emit_.buffer().emit8(0x4C); emit_.buffer().emit8(0x8D);
        emit_.buffer().emit8(0x05); // lea r8, [rip+disp32]
        std::size_t stub_fixup = emit_.buffer().pos();
        emit_.buffer().emit32(0);
        std::int32_t stub_rel = static_cast<std::int32_t>(stub_offset)
            - static_cast<std::int32_t>(stub_fixup + 4);
        emit_.buffer().patch32(stub_fixup, static_cast<std::uint32_t>(stub_rel));
        // r9 = context ptr
        emit_.mov_load(x64::Reg::R9, x64::Reg::RBP, ctx_ptr_sym.stack_offset);
        if (!emit_thread_spawn_call()) return false;

        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t thread_spawn_ok = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ctx_ptr_sym.stack_offset);
        if (!emit_runtime_free_reg(x64::Reg::RCX)) return false;
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, result_ptr_sym.stack_offset);
        if (!emit_runtime_free_reg(x64::Reg::RCX)) return false;
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t thread_spawn_fail_exit = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(thread_spawn_ok);

        // rax = thread HANDLE
        // pack [handle, ctx_ptr] into the heap-backed result struct
        emit_.mov_load(x64::Reg::R11, x64::Reg::RBP, result_ptr_sym.stack_offset);
        emit_.mov_store(x64::Reg::R11, 0, x64::Reg::RAX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, ctx_ptr_sym.stack_offset);
        emit_.mov_store(x64::Reg::R11, 8, x64::Reg::RCX);

        // return pointer to the spawn result struct
        emit_.mov(x64::Reg::RAX, x64::Reg::R11);

        emit_.patch_jump(ctx_alloc_fail_exit);
        emit_.patch_jump(result_alloc_fail_exit);
        emit_.patch_jump(thread_spawn_fail_exit);

        return true;
    }

    // await expression codegen
    // waits for thread handle, reads result from context, cleans up
    [[nodiscard]] bool generate_await(const ast::AwaitExpr& await_expr) {
        // evaluate handle expression -> rax (pointer to spawn result: [handle, ctx_ptr])
        if (!generate_expr(*await_expr.handle)) return false;

        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t non_null_handle = emit_.jcc_rel32(x64::Emitter::CC_NE);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t null_handle_exit = emit_.jmp_rel32_placeholder();
        emit_.patch_jump(non_null_handle);

        // rax points to heap-backed spawn result struct [handle, ctx_ptr]
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RAX, 0);  // handle
        emit_.mov_load(x64::Reg::RDX, x64::Reg::RAX, 8);  // ctx_ptr
        emit_.push(x64::Reg::RAX); // result ptr
        emit_.push(x64::Reg::RDX); // ctx ptr
        emit_.push(x64::Reg::RCX); // handle

        if (!emit_thread_wait_call()) return false;

        emit_.test(x64::Reg::RAX, x64::Reg::RAX);
        std::size_t wait_failed = emit_.jcc_rel32(x64::Emitter::CC_NE);

        emit_.pop(x64::Reg::RCX); // handle
        if (!emit_close_handle_call()) return false;

        emit_.pop(x64::Reg::RDX); // ctx ptr
        emit_.pop(x64::Reg::R8);  // result ptr
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RDX, CTX_RESULT);
        emit_.push(x64::Reg::RAX);
        emit_.push(x64::Reg::R8); // preserve result ptr across runtime calls
        if (!emit_runtime_free_reg(x64::Reg::RDX)) return false;
        emit_.pop(x64::Reg::R8);
        if (!emit_runtime_free_reg(x64::Reg::R8)) return false;
        emit_.pop(x64::Reg::RAX);

        std::size_t await_done = emit_.jmp_rel32_placeholder();

        emit_.patch_jump(wait_failed);
        emit_.pop(x64::Reg::RCX); // handle
        if (!emit_close_handle_call()) return false;
        emit_.pop(x64::Reg::RDX); // ctx ptr
        emit_.pop(x64::Reg::R8);  // result ptr
        emit_.push(x64::Reg::R8);
        if (!emit_runtime_free_reg(x64::Reg::RDX)) return false;
        emit_.pop(x64::Reg::R8);
        if (!emit_runtime_free_reg(x64::Reg::R8)) return false;
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);

        emit_.patch_jump(await_done);
        emit_.patch_jump(null_handle_exit);

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

        std::size_t scope_mark = scopes_.size();
        push_scope();
        std::size_t frame_patch = emit_.prologue_patchable();

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
        emit_.add_smart(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, loop_var.stack_offset, x64::Reg::RAX);

        std::int32_t loop_rel = static_cast<std::int32_t>(loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(loop_rel);
        emit_.patch_jump(loop_exit);

        // return 0
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        patch_scope_frame(frame_patch, scope_mark);
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
        emit_.add_smart(x64::Reg::RSP, 32);
        // mov eax, [rsp+0x20] ? read dwNumberOfProcessors
        emit_.buffer().emit8(0x8B); emit_.buffer().emit8(0x44);
        emit_.buffer().emit8(0x24); emit_.buffer().emit8(0x20);
        emit_.add_smart(x64::Reg::RSP, 48);

        emit_.mov_store(x64::Reg::RBP, layout.ncores_off, x64::Reg::RAX);

        // cap at PFOR_MAX_THREADS
        emit_.cmp_smart_imm(x64::Reg::RAX, PFOR_MAX_THREADS);
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
        emit_.add_smart(x64::Reg::RAX, 1);
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
        emit_.add_smart(x64::Reg::RSP, 48);

        // store handle: hdl_arr[i] = rax
        emit_.pop(x64::Reg::RDX);
        emit_.mov_load(x64::Reg::RCX, x64::Reg::RBP, layout.idx_off);
        emit_.imul_imm(x64::Reg::RCX, x64::Reg::RCX, 8);
        emit_.lea(x64::Reg::RDX, x64::Reg::RBP, layout.hdl_arr_base);
        emit_.add(x64::Reg::RDX, x64::Reg::RCX);
        emit_.mov_store(x64::Reg::RDX, 0, x64::Reg::RAX);

        // i++
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.add_smart(x64::Reg::RAX, 1);
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
        emit_.add_smart(x64::Reg::RSP, 32);

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
        emit_.add_smart(x64::Reg::RSP, 32);

        emit_.mov_load(x64::Reg::RAX, x64::Reg::RBP, layout.idx_off);
        emit_.add_smart(x64::Reg::RAX, 1);
        emit_.mov_store(x64::Reg::RBP, layout.idx_off, x64::Reg::RAX);

        std::int32_t close_rel = static_cast<std::int32_t>(
            close_loop_top - emit_.buffer().pos() - 5);
        emit_.jmp_rel32(close_rel);
        emit_.patch_jump(close_loop_exit);
    }

    // parallel for codegen ? extracts body into internal function, spawns N threads, waits for all
    [[nodiscard]] bool generate_parallel_for(const ast::ParallelForStmt& pfor) {
        if (!is_native_image_output()) {
            error("parallel for requires native image codegen");
            return false;
        }

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
        std::unordered_set<std::string> seen_captures;
        for (Scope* s = current_scope_; s != nullptr; s = s->parent) {
            for (const auto& [name, sym] : s->symbols) {
                if (name.starts_with("__pfor_")) continue;
                if (name == pfor.name) continue;
                if (!seen_captures.insert(name).second) continue;
                captures.push_back({name, sym.stack_offset});
            }
        }

        auto root_targets_ident = [&](const ast::Expr& expr, const std::string& target, const std::unordered_set<std::string>& shadowed, const auto& self) -> bool {
            return std::visit(overloaded{
                [&](const ast::IdentExpr& e) -> bool {
                    return !shadowed.contains(e.name) && e.name == target;
                },
                [&](const ast::FieldExpr& e) -> bool {
                    return self(*e.base, target, shadowed, self);
                },
                [&](const ast::UnaryExpr& e) -> bool {
                    if (e.op == ast::UnaryExpr::Op::Deref) return false;
                    return self(*e.operand, target, shadowed, self);
                },
                [&](const auto&) -> bool {
                    return false;
                }
            }, expr.kind);
        };

        std::function<bool(const ast::Expr&, const std::string&, std::unordered_set<std::string>&)> expr_mutates_ident;
        std::function<bool(const std::vector<ast::StmtPtr>&, const std::string&, std::unordered_set<std::string>&)> stmts_mutate_ident;
        std::function<bool(const ast::Stmt&, const std::string&, std::unordered_set<std::string>&)> stmt_mutates_ident;

        expr_mutates_ident = [&](const ast::Expr& expr, const std::string& target, std::unordered_set<std::string>& shadowed) -> bool {
            using BinOp = ast::BinaryExpr::Op;
            using UnOp = ast::UnaryExpr::Op;
            return std::visit(overloaded{
                [&](const ast::BinaryExpr& e) -> bool {
                    bool is_assign =
                        e.op == BinOp::Assign ||
                        e.op == BinOp::AddAssign ||
                        e.op == BinOp::SubAssign ||
                        e.op == BinOp::MulAssign ||
                        e.op == BinOp::DivAssign ||
                        e.op == BinOp::ModAssign;
                    if (is_assign && root_targets_ident(*e.lhs, target, shadowed, root_targets_ident)) {
                        return true;
                    }
                    return expr_mutates_ident(*e.lhs, target, shadowed) ||
                           expr_mutates_ident(*e.rhs, target, shadowed);
                },
                [&](const ast::UnaryExpr& e) -> bool {
                    bool is_mutating =
                        e.op == UnOp::PreInc || e.op == UnOp::PreDec ||
                        e.op == UnOp::PostInc || e.op == UnOp::PostDec;
                    if (is_mutating && root_targets_ident(*e.operand, target, shadowed, root_targets_ident)) {
                        return true;
                    }
                    return expr_mutates_ident(*e.operand, target, shadowed);
                },
                [&](const ast::CallExpr& e) -> bool {
                    if (expr_mutates_ident(*e.callee, target, shadowed)) return true;
                    for (const auto& arg : e.args) {
                        if (expr_mutates_ident(*arg, target, shadowed)) return true;
                    }
                    return false;
                },
                [&](const ast::IndexExpr& e) -> bool {
                    return expr_mutates_ident(*e.base, target, shadowed) ||
                           expr_mutates_ident(*e.index, target, shadowed);
                },
                [&](const ast::FieldExpr& e) -> bool {
                    return expr_mutates_ident(*e.base, target, shadowed);
                },
                [&](const ast::CastExpr& e) -> bool {
                    return expr_mutates_ident(*e.expr, target, shadowed);
                },
                [&](const ast::ArrayExpr& e) -> bool {
                    for (const auto& elem : e.elements) {
                        if (expr_mutates_ident(*elem, target, shadowed)) return true;
                    }
                    return false;
                },
                [&](const ast::StructExpr& e) -> bool {
                    for (const auto& [_, value] : e.fields) {
                        if (expr_mutates_ident(*value, target, shadowed)) return true;
                    }
                    return false;
                },
                [&](const ast::IfExpr& e) -> bool {
                    if (expr_mutates_ident(*e.condition, target, shadowed)) return true;
                    auto then_shadowed = shadowed;
                    if (stmts_mutate_ident(e.then_block, target, then_shadowed)) return true;
                    auto else_shadowed = shadowed;
                    return e.else_block && stmts_mutate_ident(*e.else_block, target, else_shadowed);
                },
                [&](const ast::BlockExpr& e) -> bool {
                    auto nested_shadowed = shadowed;
                    if (stmts_mutate_ident(e.block.stmts, target, nested_shadowed)) return true;
                    return e.block.result && expr_mutates_ident(*e.block.result, target, nested_shadowed);
                },
                [&](const ast::SpawnExpr& e) -> bool {
                    if (expr_mutates_ident(*e.callee, target, shadowed)) return true;
                    for (const auto& arg : e.args) {
                        if (expr_mutates_ident(*arg, target, shadowed)) return true;
                    }
                    return false;
                },
                [&](const ast::AwaitExpr& e) -> bool {
                    return expr_mutates_ident(*e.handle, target, shadowed);
                },
                [&](const ast::AtomicLoadExpr& e) -> bool {
                    return expr_mutates_ident(*e.ptr, target, shadowed);
                },
                [&](const ast::AtomicStoreExpr& e) -> bool {
                    return expr_mutates_ident(*e.ptr, target, shadowed) ||
                           expr_mutates_ident(*e.value, target, shadowed);
                },
                [&](const ast::AtomicAddExpr& e) -> bool {
                    return expr_mutates_ident(*e.ptr, target, shadowed) ||
                           expr_mutates_ident(*e.value, target, shadowed);
                },
                [&](const ast::AtomicCompareExchangeExpr& e) -> bool {
                    return expr_mutates_ident(*e.ptr, target, shadowed) ||
                           expr_mutates_ident(*e.expected, target, shadowed) ||
                           expr_mutates_ident(*e.desired, target, shadowed);
                },
                [&](const auto&) -> bool {
                    return false;
                }
            }, expr.kind);
        };

        stmt_mutates_ident = [&](const ast::Stmt& stmt, const std::string& target, std::unordered_set<std::string>& shadowed) -> bool {
            return std::visit(overloaded{
                [&](const ast::LetStmt& s) -> bool {
                    bool mutated = s.init && expr_mutates_ident(*s.init, target, shadowed);
                    shadowed.insert(s.name);
                    return mutated;
                },
                [&](const ast::ExprStmt& s) -> bool {
                    return expr_mutates_ident(*s.expr, target, shadowed);
                },
                [&](const ast::ReturnStmt& s) -> bool {
                    return s.value && expr_mutates_ident(*s.value, target, shadowed);
                },
                [&](const ast::IfStmt& s) -> bool {
                    if (expr_mutates_ident(*s.condition, target, shadowed)) return true;
                    auto then_shadowed = shadowed;
                    if (stmts_mutate_ident(s.then_block, target, then_shadowed)) return true;
                    auto else_shadowed = shadowed;
                    return s.else_block && stmts_mutate_ident(*s.else_block, target, else_shadowed);
                },
                [&](const ast::WhileStmt& s) -> bool {
                    if (expr_mutates_ident(*s.condition, target, shadowed)) return true;
                    auto body_shadowed = shadowed;
                    return stmts_mutate_ident(s.body, target, body_shadowed);
                },
                [&](const ast::ForStmt& s) -> bool {
                    if (expr_mutates_ident(*s.iterable, target, shadowed)) return true;
                    auto body_shadowed = shadowed;
                    body_shadowed.insert(s.name);
                    return stmts_mutate_ident(s.body, target, body_shadowed);
                },
                [&](const ast::LoopStmt& s) -> bool {
                    auto body_shadowed = shadowed;
                    return stmts_mutate_ident(s.body, target, body_shadowed);
                },
                [&](const ast::BlockStmt& s) -> bool {
                    auto block_shadowed = shadowed;
                    return stmts_mutate_ident(s.block.stmts, target, block_shadowed);
                },
                [&](const ast::ParallelForStmt& s) -> bool {
                    if (expr_mutates_ident(*s.start, target, shadowed) ||
                        expr_mutates_ident(*s.end, target, shadowed)) {
                        return true;
                    }
                    auto body_shadowed = shadowed;
                    body_shadowed.insert(s.name);
                    return stmts_mutate_ident(s.body, target, body_shadowed);
                },
                [&](const auto&) -> bool {
                    return false;
                }
            }, stmt.kind);
        };

        stmts_mutate_ident = [&](const std::vector<ast::StmtPtr>& stmts, const std::string& target, std::unordered_set<std::string>& shadowed) -> bool {
            for (const auto& stmt : stmts) {
                if (stmt_mutates_ident(*stmt, target, shadowed)) return true;
            }
            return false;
        };

        for (const auto& cap : captures) {
            std::unordered_set<std::string> shadowed_names;
            shadowed_names.insert(pfor.name);
            if (stmts_mutate_ident(pfor.body, cap.name, shadowed_names)) {
                error(std::format(
                    "parallel for cannot mutate captured variable '{}' (captures are by value; use explicit shared pointers or atomics)",
                    cap.name));
                pop_scope();
                return false;
            }
        }

        for (const auto& [name, gv] : globals_) {
            if (!gv.is_mut) continue;
            std::unordered_set<std::string> shadowed_names;
            shadowed_names.insert(pfor.name);
            if (stmts_mutate_ident(pfor.body, name, shadowed_names)) {
                error(std::format(
                    "parallel for cannot mutate global '{}' directly (use explicit shared pointers and atomics for shared state)",
                    name));
                pop_scope();
                return false;
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
    [[nodiscard]] bool generate_atomic_load(const ast::AtomicLoadExpr& atomic) {
        if (!generate_expr(*atomic.ptr)) return false;
        emit_.mov(x64::Reg::RCX, x64::Reg::RAX);
        emit_.mov_load(x64::Reg::RAX, x64::Reg::RCX, 0);
        return true;
    }

    [[nodiscard]] bool generate_atomic_store(const ast::AtomicStoreExpr& atomic) {
        if (!generate_expr(*atomic.ptr)) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*atomic.value)) return false;
        emit_.pop(x64::Reg::RCX);
        emit_.xchg_mem(x64::Reg::RCX, 0, x64::Reg::RAX);
        emit_.xor_(x64::Reg::RAX, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool generate_atomic_add(const ast::AtomicAddExpr& atomic) {
        if (!generate_expr(*atomic.ptr)) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*atomic.value)) return false;
        emit_.pop(x64::Reg::RCX);
        emit_.lock_xadd(x64::Reg::RCX, 0, x64::Reg::RAX);
        return true;
    }

    [[nodiscard]] bool generate_atomic_compare_exchange(const ast::AtomicCompareExchangeExpr& atomic) {
        if (!generate_expr(*atomic.ptr)) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*atomic.expected)) return false;
        emit_.push(x64::Reg::RAX);
        if (!generate_expr(*atomic.desired)) return false;
        emit_.mov(x64::Reg::RDX, x64::Reg::RAX);
        emit_.pop(x64::Reg::RAX);
        emit_.pop(x64::Reg::RCX);
        emit_.lock_cmpxchg(x64::Reg::RCX, 0, x64::Reg::RDX);
        emit_.setcc(x64::Emitter::CC_E, x64::Reg::RAX);
        emit_.movzx_byte(x64::Reg::RAX, x64::Reg::RAX);
        return true;
    }
};

} // namespace opus
