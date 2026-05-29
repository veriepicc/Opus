// vectorize.ixx - aggressive auto-vectorization for Opus
// this is the heart of our SIMD engine

module;

#ifdef _MSC_VER
#include <intrin.h>
#endif

export module opus.vectorize;
import opus.x64;
import opus.ast;
import opus.types;
import std;

export namespace opus::vectorize {

// ========================================================================
// CPU CAPABILITY DETECTION
// ========================================================================

enum class CpuLevel {
    SSE42,   // baseline: pentium 4 / nehalem 2008+
    AVX2,    // mainstream: haswell 2013+
    AVX512F, // enthusiast: skylake-x 2017+
};

struct CpuCapabilities {
    CpuLevel level = CpuLevel::SSE42;
    bool has_avx512f = false;
    bool has_avx512bw = false;
    bool has_avx512dq = false;
    bool has_avx512vnni = false;
    bool has_avx2 = false;
    bool has_sse42 = false;
    
    // vector widths in bytes
    int vec_width_bytes() const {
        switch (level) {
            case CpuLevel::SSE42: return 16;   // 128-bit
            case CpuLevel::AVX2:  return 32;   // 256-bit
            case CpuLevel::AVX512F: return 64;  // 512-bit
        }
        return 16;
    }
    
    // elements per vector for i32
    int i32_per_vec() const {
        return vec_width_bytes() / 4;
    }
};

#ifdef _MSC_VER
#include <intrin.h>

inline void cpuid(std::int32_t leaf, std::int32_t subleaf, std::int32_t* ebx, std::int32_t* ecx, std::int32_t* edx) {
    std::int32_t regs[4] = {0};
    __cpuidex(regs, leaf, subleaf);
    *ebx = regs[0];
    *ecx = regs[1];
    *edx = regs[2];
}

#else
inline void cpuid(std::int32_t leaf, std::int32_t subleaf, std::int32_t* ebx, std::int32_t* ecx, std::int32_t* edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}
#endif

inline CpuCapabilities detect_cpu() {
    CpuCapabilities caps;
    
    std::int32_t ebx = 0, ecx = 0, edx = 0;
    cpuid(1, 0, &ebx, &ecx, &edx);
    
    caps.has_sse42 = (edx >> 20) & 1;  // SSE4.2
    
    // check osxsave for AVX
    bool has_osxsave = (ecx >> 27) & 1;
    bool has_avx = (ecx >> 28) & 1;
    
    caps.level = CpuLevel::SSE42;  // baseline
    
    if (has_osxsave && has_avx) {
        cpuid(7, 0, &ebx, &ecx, &edx);
        
        caps.has_avx2 = (ebx >> 5) & 1;      // AVX2
        caps.has_avx512f = (ebx >> 16) & 1;  // AVX512 Foundation
        caps.has_avx512bw = (ebx >> 30) & 1; // AVX512 Byte/Word
        caps.has_avx512dq = (ebx >> 17) & 1; // AVX512 DWord/QWord
        caps.has_avx512vnni = (ecx >> 11) & 1; // AVX512 VNNI
        
        if (caps.has_avx512f) {
            caps.level = CpuLevel::AVX512F;
        } else if (caps.has_avx2) {
            caps.level = CpuLevel::AVX2;
        }
    }
    
    return caps;
}

// global singleton - initialized once at program start
inline const CpuCapabilities& get_caps() {
    static CpuCapabilities caps = detect_cpu();
    return caps;
}

// ========================================================================
// LOOP ANALYSIS
// ========================================================================

enum class LoopPattern {
    Unknown,
    CounterBased,    // while i < n { ... i++ }
    RangeBased,      // for i in range(start, end)
    Reduction,       // sum/product accumulation
    Memcpy,          // memory copy pattern
    Memset,          // memory set pattern
    ArrayUpdate,     // array[index] = ...
};

struct LoopInfo {
    LoopPattern pattern = LoopPattern::Unknown;
    std::string counter_name;
    std::int64_t trip_count = -1;  // -1 = unknown
    std::int64_t step = 1;
    std::int64_t start = 0;
    std::int64_t end = 0;
    Type element_type;
    bool is_reduction = false;
    ast::BinaryExpr::Op reduction_op = ast::BinaryExpr::Op::Add;
    std::string accum_name;
    const ast::Expr* accum_init = nullptr;
    const ast::Expr* body_expr = nullptr;  // the main computation
};

inline std::int64_t get_literal_int64(const ast::Expr& expr) {
    if (auto* lit = std::get_if<ast::LiteralExpr>(&expr.kind)) {
        if (auto* val = std::get_if<std::int64_t>(&lit->value)) {
            return *val;
        }
    }
    return 0;
}

inline bool is_counter_based_while(const ast::WhileStmt& w, LoopInfo& info) {
    using Op = ast::BinaryExpr::Op;
    auto* cond = std::get_if<ast::BinaryExpr>(&w.condition->kind);
    if (!cond || cond->op != Op::Lt) return false;
    
    auto* lhs_ident = std::get_if<ast::IdentExpr>(&cond->lhs->kind);
    if (!lhs_ident) return false;
    
    std::int64_t limit = get_literal_int64(*cond->rhs);
    if (limit == 0 && cond->rhs) return false;
    
    info.counter_name = lhs_ident->name;
    info.start = 0;
    info.end = limit;
    info.trip_count = limit;
    info.pattern = LoopPattern::CounterBased;
    
    return true;
}

inline bool is_range_based_for(const ast::ForStmt& f, LoopInfo& info) {
    auto* call = std::get_if<ast::CallExpr>(&f.iterable->kind);
    if (!call) return false;
    
    auto* callee = std::get_if<ast::IdentExpr>(&call->callee->kind);
    if (!callee || callee->name != "range") return false;
    
    info.counter_name = f.name;
    info.pattern = LoopPattern::RangeBased;
    
    if (call->args.size() == 1) {
        info.start = 0;
        info.end = get_literal_int64(*call->args[0]);
        info.trip_count = info.end;
    } else if (call->args.size() == 2) {
        info.start = get_literal_int64(*call->args[0]);
        info.end = get_literal_int64(*call->args[1]);
        info.trip_count = info.end - info.start;
    }
    
    return true;
}

inline bool is_simple_reduction(const ast::Stmt& body, const std::string& counter, LoopInfo& info) {
    if (!body.is<ast::ExprStmt>()) return false;
    
    auto* assign = std::get_if<ast::BinaryExpr>(&body.as<ast::ExprStmt>().expr->kind);
    if (!assign || assign->op != ast::BinaryExpr::Op::Assign) return false;
    
    auto* lhs_ident = std::get_if<ast::IdentExpr>(&assign->lhs->kind);
    if (!lhs_ident) return false;
    
    info.accum_name = lhs_ident->name;
    
    auto* rhs_bin = std::get_if<ast::BinaryExpr>(&assign->rhs->kind);
    if (!rhs_bin) return false;
    
    if (rhs_bin->op != ast::BinaryExpr::Op::Add && rhs_bin->op != ast::BinaryExpr::Op::Mul) {
        return false;
    }
    
    info.reduction_op = rhs_bin->op;
    info.is_reduction = true;
    info.pattern = LoopPattern::Reduction;
    
    return true;
}

// ========================================================================
// SIMD KERNEL GENERATOR
// ========================================================================

class SimdKernelGen {
public:
    SimdKernelGen(x64::Emitter& emit, const CpuCapabilities& caps)
        : emit_(emit), caps_(caps) {}
    
    // generate vectorized loop for reduction (sum/product)
    bool gen_reduction_kernel(
        const LoopInfo& info,
        const ast::Stmt& body,
        x64::Reg counter_reg,
        x64::Reg limit_reg,
        x64::Reg accum_reg,
        x64::Reg base_ptr_reg
    ) {
        int vec_width = caps_.i32_per_vec();
        
        // choose vector register based on CPU level
        std::uint8_t vec_reg = 0;
        std::uint8_t scratch_reg = 1;
        
        // broadcast accumulator to vector
        emit_.mov(x64::Reg::RAX, accum_reg);
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vpbroadcastd_zmm(static_cast<x64::ZmmReg>(vec_reg), x64::Reg::RAX);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vpbroadcastd(vec_reg, x64::Reg::RAX);
        } else {
            emit_.movd_from_gpr(x64::XmmReg(vec_reg), x64::Reg::RAX);
            emit_.punpcklqdq(x64::XmmReg(vec_reg), x64::XmmReg(vec_reg));
        }
        
        // main vectorized loop
        std::size_t loop_start = emit_.buffer().pos();
        
        // load vector from memory
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vmovdqu32_zmm_load(vec_reg + 2, base_ptr_reg, 0);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vmovdqu_load(vec_reg + 2, base_ptr_reg, 0);
        } else {
            emit_.movdqu_load(x64::XmmReg(vec_reg + 2), base_ptr_reg, 0);
        }
        
        // reduction operation
        if (info.reduction_op == ast::BinaryExpr::Op::Add) {
            if (caps_.level >= CpuLevel::AVX512F) {
                emit_.vpaddd_zmm(x64::ZmmReg(vec_reg), x64::ZmmReg(vec_reg), x64::ZmmReg(vec_reg + 2));
            } else if (caps_.level >= CpuLevel::AVX2) {
                emit_.vpaddd(vec_reg, vec_reg, vec_reg + 2);
            } else {
                emit_.paddd(x64::XmmReg(vec_reg), x64::XmmReg(vec_reg + 2));
            }
        } else if (info.reduction_op == ast::BinaryExpr::Op::Mul) {
            if (caps_.level >= CpuLevel::AVX512F) {
                emit_.vpmulld_zmm(x64::ZmmReg(vec_reg), x64::ZmmReg(vec_reg), x64::ZmmReg(vec_reg + 2));
            } else if (caps_.level >= CpuLevel::AVX2) {
                emit_.vpmulld(vec_reg, vec_reg, vec_reg + 2);
            } else {
                emit_.pmulld(x64::XmmReg(vec_reg), x64::XmmReg(vec_reg + 2));
            }
        }
        
        // increment pointers
        emit_.add_smart(base_ptr_reg, vec_width * 4);
        emit_.add_smart(counter_reg, vec_width);
        
        // loop until counter >= limit
        emit_.cmp(counter_reg, limit_reg);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);  // jump to exit if >=
        
        // main vectorized loop body
        std::size_t loop_body_end = emit_.buffer().pos();
        
        // jump back to start
        emit_.jmp_rel32_to(loop_start);
        
        // patch the exit jump to here (after loop body)
        emit_.patch_jump(exit_patch);
        
        // horizontal reduction of vector to scalar
        if (caps_.level >= CpuLevel::AVX512F) {
            // vextracti32x8 + horizontal add
            emit_.vextracti32x8(x64::YmmReg(scratch_reg), x64::ZmmReg(vec_reg), 1);
            emit_.vpaddd(static_cast<std::uint8_t>(vec_reg), static_cast<std::uint8_t>(vec_reg), scratch_reg);
            emit_.vextracti128(x64::XmmReg(scratch_reg), x64::YmmReg(vec_reg), 0);
            emit_.vpaddd(static_cast<std::uint8_t>(vec_reg), static_cast<std::uint8_t>(vec_reg), scratch_reg);
            
            // extract to GP
            emit_.movq_to_gpr(x64::Reg::RAX, x64::XmmReg(vec_reg));
        } else if (caps_.level >= CpuLevel::AVX2) {
            // extract 128-bit halves and add
            emit_.vextracti128(x64::XmmReg(scratch_reg), x64::YmmReg(vec_reg), 1);
            emit_.vpaddd(static_cast<std::uint8_t>(vec_reg), static_cast<std::uint8_t>(vec_reg), scratch_reg);
            
            // horizontal add within 128 bits
            emit_.movdqa(x64::XmmReg(scratch_reg), x64::XmmReg(vec_reg));
            emit_.psrldq(x64::XmmReg(scratch_reg), x64::XmmReg(scratch_reg), 8);
            emit_.paddd(x64::XmmReg(vec_reg), x64::XmmReg(scratch_reg));
            
            // extract low 32 bits
            emit_.movq_to_gpr(x64::Reg::RAX, x64::XmmReg(vec_reg));
        } else {
            // SSE4.2 horizontal add
            emit_.movdqa(x64::XmmReg(scratch_reg), x64::XmmReg(vec_reg));
            emit_.psrldq(x64::XmmReg(scratch_reg), 8);
            emit_.paddd(x64::XmmReg(vec_reg), x64::XmmReg(scratch_reg));
            emit_.movd_to_gpr(x64::Reg::RAX, x64::XmmReg(vec_reg));
        }
        
        emit_.mov(accum_reg, x64::Reg::RAX);
        
        return true;
    }
    
    // generate vectorized kernel for general array operations
    bool gen_array_kernel(
        const LoopInfo& info,
        x64::Reg counter_reg,
        x64::Reg limit_reg,
        x64::Reg src_ptr,
        x64::Reg dst_ptr
    ) {
        int vec_width = caps_.i32_per_vec();
        
        std::size_t loop_start = emit_.buffer().pos();
        
        // load vector
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vmovdqu32_zmm_load(0, src_ptr, 0);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vmovdqu_load(0, src_ptr, 0);
        } else {
            emit_.movdqu_load(x64::XmmReg(0), src_ptr, 0);
        }
        
        // store vector
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vmovdqu32_zmm_store(dst_ptr, 0, 0);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vmovdqu_store(dst_ptr, 0, 0);
        } else {
            emit_.movdqu_store(dst_ptr, 0, x64::XmmReg(0));
        }
        
        // increment
        emit_.add_smart(src_ptr, vec_width * 4);
        emit_.add_smart(dst_ptr, vec_width * 4);
        emit_.add_smart(counter_reg, vec_width);
        
        // loop until counter >= limit
        emit_.cmp(counter_reg, limit_reg);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        
        // jump back to start
        emit_.jmp_rel32_to(loop_start);
        
        // patch the exit jump to here
        emit_.patch_jump(exit_patch);
        
        return true;
    }
    
    // generate masked remainder loop for AVX512
    bool gen_masked_remainder(
        x64::Reg counter_reg,
        x64::Reg limit_reg,
        std::uint8_t k_reg,
        x64::Reg src_ptr,
        x64::Reg dst_ptr
    ) {
        if (caps_.level < CpuLevel::AVX512F) return false;
        
        // calculate remainder count: limit - counter
        emit_.mov(x64::Reg::RCX, limit_reg);
        emit_.sub(x64::Reg::RCX, counter_reg);
        
        // set mask based on remainder (k = (1 << count) - 1)
        emit_.mov_imm(x64::Reg::EAX, 1);
        emit_.shl(x64::Reg::RAX);
        emit_.dec(x64::Reg::RAX);
        emit_.kmovb_from_gpr(k_reg, x64::Reg::RAX);
        
        // masked load - will use k_reg for masking
        emit_.vmovdqu32_zmm_load(k_reg, src_ptr, 0);
        
        // masked store - this is incomplete, just placeholder
        emit_.vmovdqu32_zmm_store(dst_ptr, 0, 0);
        
        return true;
    }
    
private:
    x64::Emitter& emit_;
    const CpuCapabilities& caps_;
};

// ========================================================================
// STRIP-MINING VECTORIZER
// ========================================================================

// forward declarations
inline LoopInfo analyze_loop(const ast::WhileStmt& w);
inline LoopInfo analyze_loop(const ast::ForStmt& f);

class LoopVectorizer {
public:
    LoopVectorizer(x64::Emitter& emit) 
        : emit_(emit), caps_(get_caps()), simd_gen_(emit, caps_) {}
    
    bool try_vectorize(const ast::WhileStmt& w) {
        LoopInfo info = analyze_loop(w);
        
        if (info.trip_count <= 0 || info.trip_count < caps_.i32_per_vec()) {
            return false;
        }
        
        switch (info.pattern) {
            case LoopPattern::Reduction:
                return vectorize_reduction(info, w);
            case LoopPattern::CounterBased:
            case LoopPattern::RangeBased:
                return vectorize_generic(info, w);
            default:
                return false;
        }
    }
    
    bool try_vectorize(const ast::ForStmt& f) {
        LoopInfo info = analyze_loop(f);
        
        if (info.trip_count <= 0 || info.trip_count < caps_.i32_per_vec()) {
            return false;
        }
        
        switch (info.pattern) {
            case LoopPattern::Reduction:
                return vectorize_reduction(info, f);
            case LoopPattern::CounterBased:
            case LoopPattern::RangeBased:
                return vectorize_generic(info, f);
            default:
                return false;
        }
    }
    
private:
    bool vectorize_reduction(const LoopInfo& info, const ast::WhileStmt& w) {
        // for now, let scalar handle it - reduction needs more analysis
        (void)w;
        return false;
    }
    
    bool vectorize_reduction(const LoopInfo& info, const ast::ForStmt& f) {
        // for now, let scalar handle it
        (void)f;
        return false;
    }
    
    bool vectorize_generic(const LoopInfo& info, const ast::WhileStmt& w) {
        // generic counter-based loops cant be vectorized without
        // analyzing the body for memory access patterns. bail out
        // and let scalar codegen handle it.
        (void)info;
        (void)w;
        return false;
    }
    
    bool vectorize_generic(const LoopInfo& info, const ast::ForStmt& f) {
        // same deal - no body analysis means no safe vectorization
        (void)info;
        (void)f;
        return false;
    }
    
    bool vectorize_counter_based(const LoopInfo& info) {
        int vec_width = caps_.i32_per_vec();
        
        emit_.push(x64::Reg::RBP);
        emit_.mov(x64::Reg::RBP, x64::Reg::RSP);
        emit_.sub_imm(x64::Reg::RSP, 40);
        
        emit_.xor_(x64::Reg::RCX, x64::Reg::RCX);
        emit_.mov_imm(x64::Reg::RDX, static_cast<std::int32_t>(info.trip_count - (info.trip_count % vec_width)));
        
        // completely unrolled for tiny loops (no branches at all)
        if (info.trip_count <= 64) {
            emit_unrolled_small(info.trip_count, vec_width);
            emit_.mov(x64::Reg::RSP, x64::Reg::RBP);
            emit_.pop(x64::Reg::RBP);
            return true;
        }
        
        // fully unrolled for medium loops (no branches at all)
        if (info.trip_count <= 256) {
            emit_unrolled_medium(info.trip_count, vec_width);
            emit_.mov(x64::Reg::RSP, x64::Reg::RBP);
            emit_.pop(x64::Reg::RBP);
            return true;
        }
        
        // big unroll for large loops (16x) with software pipelining
        if (info.trip_count >= 64) {
            constexpr int kBigUnroll = 16;
            std::size_t big_loop_start = emit_.buffer().pos();
            emit_.cmp(x64::Reg::RCX, x64::Reg::RDX);
            std::size_t small_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
            
            // unroll with multiple registers for better ILP
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            
            emit_.add_imm(x64::Reg::RCX, kBigUnroll * vec_width);
            emit_.jmp_rel32_to(big_loop_start);
            emit_.patch_jump(small_patch);
        }
        
        // medium unroll (8x) with prefetch
        std::size_t loop_start = emit_.buffer().pos();
        
        if (caps_.level >= CpuLevel::AVX512F) {
            for (int i = 0; i < 8; ++i) {
                emit_.vmovdqu32_zmm_load(0, x64::Reg::R8, 0);
                emit_.vmovdqu32_zmm_store(x64::Reg::R9, 0, 0);
            }
            emit_.add_imm(x64::Reg::R8, 64 * 8);
            emit_.add_imm(x64::Reg::R9, 64 * 8);
            emit_.add_imm(x64::Reg::RCX, 8 * 16);
        } else if (caps_.level >= CpuLevel::AVX2) {
            for (int i = 0; i < 8; ++i) {
                emit_.vmovdqu_load(0, x64::Reg::R8, 0);
                emit_.vmovdqu_store(x64::Reg::R9, 0, 0);
            }
            emit_.add_imm(x64::Reg::R8, 32 * 8);
            emit_.add_imm(x64::Reg::R9, 32 * 8);
            emit_.add_imm(x64::Reg::RCX, 8 * 8);
        } else {
            for (int i = 0; i < 8; ++i) {
                emit_.movdqu_load(x64::XmmReg(0), x64::Reg::R8, 0);
                emit_.movdqu_store(x64::Reg::R9, 0, x64::XmmReg(0));
            }
            emit_.add_imm(x64::Reg::R8, 16 * 8);
            emit_.add_imm(x64::Reg::R9, 16 * 8);
            emit_.add_imm(x64::Reg::RCX, 8 * 4);
        }
        
        emit_.cmp(x64::Reg::RCX, x64::Reg::RDX);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        emit_.jmp_rel32_to(loop_start);
        emit_.patch_jump(exit_patch);
        
        emit_.mov(x64::Reg::RSP, x64::Reg::RBP);
        emit_.pop(x64::Reg::RBP);
        
        return true;
    }
    
    void emit_unrolled_small(int count, int vec_width) {
        // no branches, just unroll completely
        for (int i = 0; i < count; ++i) {
            emit_simd_body(vec_width);
        }
    }
    
    void emit_unrolled_medium(int count, int vec_width) {
        // unroll in blocks of 16 to fit instruction cache
        int blocks = count / 16;
        int remainder = count % 16;
        
        for (int b = 0; b < blocks; ++b) {
            // unroll 16x with independent register pressure
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
            emit_simd_body(vec_width);
        }
        
        for (int i = 0; i < remainder; ++i) {
            emit_simd_body(vec_width);
        }
    }
    
    void emit_simd_body_multi(int vec_width, int reg_idx) {
        // use different register each iteration for better pipelining
        std::uint8_t reg = static_cast<std::uint8_t>(reg_idx & 3);
        
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vmovdqu32_zmm_load(reg, x64::Reg::R8, 0);
            emit_.vmovdqu32_zmm_store(x64::Reg::R9, 0, reg);
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vmovdqu_load(reg, x64::Reg::R8, 0);
            emit_.vmovdqu_store(x64::Reg::R9, 0, reg);
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        } else {
            emit_.movdqu_load(x64::XmmReg(reg), x64::Reg::R8, 0);
            emit_.movdqu_store(x64::Reg::R9, 0, x64::XmmReg(reg));
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        }
    }
    
    void emit_simd_body(int vec_width) {
        if (caps_.level >= CpuLevel::AVX512F) {
            emit_.vmovdqu32_zmm_load(0, x64::Reg::R8, 0);
            emit_.vmovdqu32_zmm_store(x64::Reg::R9, 0, 0);
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        } else if (caps_.level >= CpuLevel::AVX2) {
            emit_.vmovdqu_load(0, x64::Reg::R8, 0);
            emit_.vmovdqu_store(x64::Reg::R9, 0, 0);
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        } else {
            emit_.movdqu_load(x64::XmmReg(0), x64::Reg::R8, 0);
            emit_.movdqu_store(x64::Reg::R9, 0, x64::XmmReg(0));
            emit_.add_imm(x64::Reg::R8, vec_width * 4);
            emit_.add_imm(x64::Reg::R9, vec_width * 4);
        }
    }
    
    void emit_simd_loop_avx512(const LoopInfo& info) {
        std::size_t loop_start = emit_.buffer().pos();
        
        // load 16x i32
        emit_.vmovdqu32_zmm_load(0, x64::Reg::R8, 0);
        
        // store 16x i32
        emit_.vmovdqu32_zmm_store(x64::Reg::R9, 0, 0);
        
        // advance pointers by 64 bytes
        emit_.add_imm(x64::Reg::R8, 64);
        emit_.add_imm(x64::Reg::R9, 64);
        emit_.add_imm(x64::Reg::RCX, 16);
        
        // compare and loop
        emit_.cmp(x64::Reg::RCX, x64::Reg::RDX);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        
        // jump back to start
        emit_.jmp_rel32_to(loop_start);
        
        // patch the exit jump
        emit_.patch_jump(exit_patch);
    }
    
    void emit_simd_loop_avx2(const LoopInfo& info) {
        std::size_t loop_start = emit_.buffer().pos();
        
        // load 8x i32
        emit_.vmovdqu_load(0, x64::Reg::R8, 0);
        
        // store 8x i32
        emit_.vmovdqu_store(x64::Reg::R9, 0, 0);
        
        // advance by 32 bytes
        emit_.add_imm(x64::Reg::R8, 32);
        emit_.add_imm(x64::Reg::R9, 32);
        emit_.add_imm(x64::Reg::RCX, 8);
        
        emit_.cmp(x64::Reg::RCX, x64::Reg::RDX);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        
        // jump back to start
        emit_.jmp_rel32_to(loop_start);
        
        // patch the exit jump
        emit_.patch_jump(exit_patch);
    }
    
    void emit_simd_loop_sse42(const LoopInfo& info) {
        std::size_t loop_start = emit_.buffer().pos();
        
        // load 4x i32
        emit_.movdqu_load(x64::XmmReg(0), x64::Reg::R8, 0);
        
        // store 4x i32
        emit_.movdqu_store(x64::Reg::R9, 0, x64::XmmReg(0));
        
        // advance by 16 bytes
        emit_.add_imm(x64::Reg::R8, 16);
        emit_.add_imm(x64::Reg::R9, 16);
        emit_.add_imm(x64::Reg::RCX, 4);
        
        emit_.cmp(x64::Reg::RCX, x64::Reg::RDX);
        std::size_t exit_patch = emit_.jcc_rel32(x64::Emitter::CC_GE);
        
        // jump back to start
        emit_.jmp_rel32_to(loop_start);
        
        // patch the exit jump
        emit_.patch_jump(exit_patch);
    }
    
    x64::Emitter& emit_;
    const CpuCapabilities& caps_;
    SimdKernelGen simd_gen_;
};

// ========================================================================
// INTEGRATION HELPERS
// ========================================================================

// check if a while loop can be vectorized
inline bool can_vectorize(const ast::WhileStmt& w) {
    LoopInfo info;
    if (!is_counter_based_while(w, info)) return false;
    if (info.trip_count <= 0) return false;
    return true;
}

// check if a for loop can be vectorized
inline bool can_vectorize(const ast::ForStmt& f) {
    LoopInfo info;
    if (!is_range_based_for(f, info)) return false;
    if (info.trip_count <= 0) return false;
    return true;
}

// analyze while loop
inline LoopInfo analyze_loop(const ast::WhileStmt& w) {
    LoopInfo info;
    if (is_counter_based_while(w, info)) {
        if (w.body.size() == 1) {
            is_simple_reduction(*w.body[0], info.counter_name, info);
        }
    }
    return info;
}

// analyze for loop
inline LoopInfo analyze_loop(const ast::ForStmt& f) {
    LoopInfo info;
    if (is_range_based_for(f, info)) {
        if (f.body.size() == 1) {
            is_simple_reduction(*f.body[0], info.counter_name, info);
        }
    }
    return info;
}

// check if vectorization is profitable
inline bool is_profitable(const LoopInfo& info) {
    const CpuCapabilities& caps = get_caps();
    int min_trip_count = caps.i32_per_vec() * 2;  // at least 2 vectors
    return info.trip_count >= min_trip_count;
}

// get the best ISA for current CPU
inline CpuLevel get_best_isa() {
    return get_caps().level;
}

// runtime check wrapper for AVX2
inline void ensure_avx2_enabled() {
    if (get_caps().level >= CpuLevel::AVX2) {
        // xgetbv to check OS support for YMM regs
        #ifdef _MSC_VER
        std::int64_t xcr = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        #else
        std::int64_t xcr = 0;
        __asm__ volatile ("xgetbv" : "=a"(xcr) : "c"(0) : "edx");
        #endif
        (void)xcr;
    }
}

} // namespace opus::vectorize