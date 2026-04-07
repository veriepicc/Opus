// x64 emitter

module;

#include <cassert>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

export module opus.x64;

import opus.types;
import std;

export namespace opus::x64 {

enum class Reg : std::uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // 32-bit versions (same encoding, different REX)
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
};

[[nodiscard]] constexpr std::string_view reg_name(Reg r) {
    switch (r) {
        case Reg::RAX: return "rax"; case Reg::RCX: return "rcx";
        case Reg::RDX: return "rdx"; case Reg::RBX: return "rbx";
        case Reg::RSP: return "rsp"; case Reg::RBP: return "rbp";
        case Reg::RSI: return "rsi"; case Reg::RDI: return "rdi";
        case Reg::R8:  return "r8";  case Reg::R9:  return "r9";
        case Reg::R10: return "r10"; case Reg::R11: return "r11";
        case Reg::R12: return "r12"; case Reg::R13: return "r13";
        case Reg::R14: return "r14"; case Reg::R15: return "r15";
        default: return "???";
    }
}

// windows x64 calling convention
constexpr std::array<Reg, 4> ARG_REGS = { Reg::RCX, Reg::RDX, Reg::R8, Reg::R9 };
constexpr std::array<Reg, 7> CALLER_SAVED = { Reg::RAX, Reg::RCX, Reg::RDX, Reg::R8, Reg::R9, Reg::R10, Reg::R11 };
constexpr std::array<Reg, 7> CALLEE_SAVED = { Reg::RBX, Reg::RSI, Reg::RDI, Reg::R12, Reg::R13, Reg::R14, Reg::R15 };

// x64 CODE BUFFER

class CodeBuffer {
public:
    CodeBuffer(std::size_t initial_capacity = 4096) {
        code_.reserve(initial_capacity);
    }

    void emit8(std::uint8_t b) { code_.push_back(b); }
    
    void emit16(std::uint16_t w) {
        emit8(static_cast<std::uint8_t>(w));
        emit8(static_cast<std::uint8_t>(w >> 8));
    }
    
    void emit32(std::uint32_t d) {
        emit8(static_cast<std::uint8_t>(d));
        emit8(static_cast<std::uint8_t>(d >> 8));
        emit8(static_cast<std::uint8_t>(d >> 16));
        emit8(static_cast<std::uint8_t>(d >> 24));
    }
    
    void emit64(std::uint64_t q) {
        emit32(static_cast<std::uint32_t>(q));
        emit32(static_cast<std::uint32_t>(q >> 32));
    }

    void emit_bytes(std::span<const std::uint8_t> bytes) {
        code_.insert(code_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::size_t pos() const { return code_.size(); }
    
    std::uint8_t* data() { return code_.data(); }
    const std::uint8_t* data() const { return code_.data(); }
    
    std::span<std::uint8_t> span() { return std::span<std::uint8_t>(code_); }
    std::span<const std::uint8_t> span() const { return std::span<const std::uint8_t>(code_); }

    void patch32(std::size_t offset, std::uint32_t value) {
        assert(offset + 3 < code_.size() && "patch32: offset out of bounds");
        code_[offset]     = static_cast<std::uint8_t>(value);
        code_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        code_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
        code_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    }

    [[nodiscard]] std::size_t create_label() { return pos(); }
    
    [[nodiscard]] std::int32_t rel32(std::size_t from, std::size_t to) {
        return static_cast<std::int32_t>(to - from - 4);
    }

private:
    std::vector<std::uint8_t> code_;
};

class Emitter {
public:
    Emitter() = default;
    
    CodeBuffer& buffer() { return buf_; }
    const CodeBuffer& buffer() const { return buf_; }

    static constexpr std::uint8_t REX_W = 0x48;  // 64-bit operand

    // modrm opcode extension fields (/0 through /7)
    static constexpr std::uint8_t EXT_ADD = 0, EXT_OR = 1, EXT_CALL = 2, EXT_NOT = 2;
    static constexpr std::uint8_t EXT_NEG = 3, EXT_MUL = 4, EXT_SUB = 5;
    static constexpr std::uint8_t EXT_DIV = 6, EXT_IDIV = 7;

    [[nodiscard]] constexpr std::uint8_t rex(bool w, Reg reg, Reg rm) {
        std::uint8_t r = 0x40;
        if (w) r |= 0x08;
        if (static_cast<std::uint8_t>(reg) >= 8) r |= 0x04;
        if (static_cast<std::uint8_t>(rm) >= 8) r |= 0x01;
        return r;
    }

    [[nodiscard]] constexpr std::uint8_t rex_sib(bool w, Reg reg, Reg index, Reg base) {
        std::uint8_t r = 0x40;
        if (w) r |= 0x08;
        if (static_cast<std::uint8_t>(reg) >= 8) r |= 0x04;
        if (static_cast<std::uint8_t>(index) >= 8) r |= 0x02;
        if (static_cast<std::uint8_t>(base) >= 8) r |= 0x01;
        return r;
    }

    [[nodiscard]] constexpr std::uint8_t modrm(std::uint8_t mod, Reg reg, Reg rm) {
        return (mod << 6) | ((static_cast<std::uint8_t>(reg) & 7) << 3) | (static_cast<std::uint8_t>(rm) & 7);
    }

    // power of two detection helpers
    [[nodiscard]] static constexpr bool is_power_of_two(std::int64_t n) {
        return n > 1 && (n & (n - 1)) == 0;
    }

    [[nodiscard]] static inline int log2_pow2(std::uint64_t n) {
        // count trailing zeros = log2 for power of two
        #ifdef _MSC_VER
            unsigned long index;
            _BitScanForward64(&index, n);
            return static_cast<int>(index);
        #else
            return __builtin_ctzll(n);
        #endif
    }


    void mov_imm64(Reg dst, std::uint64_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xB8 + (static_cast<std::uint8_t>(dst) & 7));
        buf_.emit64(imm);
    }

    void mov_imm32(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xC7);
        buf_.emit8(modrm(0b11, Reg::RAX, dst));
        buf_.emit32(static_cast<std::uint32_t>(imm));
    }

    void mov(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x89);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void mov_load(Reg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(rex(true, dst, base));
        buf_.emit8(0x8B);
        emit_mem_operand(dst, base, offset);
    }

    void mov_store(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x89);
        emit_mem_operand(src, base, offset);
    }

    void lea(Reg dst, Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, dst, base));
        buf_.emit8(0x8D);
        emit_mem_operand(dst, base, offset);
    }

    // lea dst, [base + index*scale]
    void lea_scaled(Reg dst, Reg base, Reg index, std::uint8_t scale) {
        assert((scale == 1 || scale == 2 || scale == 4 || scale == 8) && 
               "lea scale must be 1, 2, 4, or 8");
        
        std::uint8_t scale_bits = 0;
        if (scale == 2) scale_bits = 1;
        else if (scale == 4) scale_bits = 2;
        else if (scale == 8) scale_bits = 3;
        
        buf_.emit8(rex_sib(true, dst, index, base));
        buf_.emit8(0x8D);
        buf_.emit8(modrm(0b00, dst, Reg(4)));  // modrm points to SIB
        // SIB byte: scale.index.base
        buf_.emit8((scale_bits << 6) | 
                   ((static_cast<std::uint8_t>(index) & 7) << 3) | 
                   (static_cast<std::uint8_t>(base) & 7));
    }

    // ARITHMETIC

    void add(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x01);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void add_mem(Reg dst, Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, dst, base));
        buf_.emit8(0x03);
        emit_mem_operand(dst, base, offset);
    }

    void add_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg::RAX, dst));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg::RAX, dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void add_mem_imm(Reg base, std::int32_t offset, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, base));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            emit_mem_operand(Reg(EXT_ADD), base, offset);
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            emit_mem_operand(Reg(EXT_ADD), base, offset);
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void sub(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x29);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void sub_mem(Reg dst, Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, dst, base));
        buf_.emit8(0x2B);
        emit_mem_operand(dst, base, offset);
    }

    void sub_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(EXT_SUB), dst));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(EXT_SUB), dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void sub_mem_imm(Reg base, std::int32_t offset, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, base));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            emit_mem_operand(Reg(EXT_SUB), base, offset);
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            emit_mem_operand(Reg(EXT_SUB), base, offset);
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void imul(Reg dst, Reg src) {
        buf_.emit8(rex(true, dst, src));
        buf_.emit8(0x0F);
        buf_.emit8(0xAF);
        buf_.emit8(modrm(0b11, dst, src));
    }

    void imul_imm(Reg dst, Reg src, std::int32_t imm) {
        buf_.emit8(rex(true, dst, src));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x6B);
            buf_.emit8(modrm(0b11, dst, src));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x69);
            buf_.emit8(modrm(0b11, dst, src));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    // idiv reg (RDX:RAX / reg -> quotient in RAX, remainder in RDX)
    void idiv(Reg src) {
        buf_.emit8(rex(true, Reg::RAX, src));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(EXT_IDIV), src));
    }

    // cqo - sign-extend RAX into RDX:RAX
    void cqo() {
        buf_.emit8(0x48);
        buf_.emit8(0x99);
    }

    void neg(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(EXT_NEG), dst));
    }

    void inc(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xFF);
        buf_.emit8(modrm(0b11, Reg(0), dst));  // /0 = inc
    }

    void inc_mem(Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, Reg::RAX, base));
        buf_.emit8(0xFF);
        emit_mem_operand(Reg(0), base, offset);
    }

    void dec(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xFF);
        buf_.emit8(modrm(0b11, Reg(1), dst));  // /1 = dec
    }

    void dec_mem(Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, Reg::RAX, base));
        buf_.emit8(0xFF);
        emit_mem_operand(Reg(1), base, offset);
    }

    // BITWISE

    void and_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x21);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void and_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(4), dst));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(4), dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void or_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x09);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void or_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(EXT_OR), dst));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(EXT_OR), dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void xor_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x31);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void xor_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(6), dst));
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(6), dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void not_(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(EXT_NOT), dst));
    }

    void shl_cl(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xD3);
        buf_.emit8(modrm(0b11, Reg(4), dst));  // /4 = shl
    }

    void shr_cl(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xD3);
        buf_.emit8(modrm(0b11, Reg(5), dst));  // /5 = shr
    }

    // sar reg, cl - arithmetic shift right
    void sar_cl(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xD3);
        buf_.emit8(modrm(0b11, Reg(7), dst));  // /7 = sar
    }

    // shift immediate variants
    void shl_imm(Reg dst, std::uint8_t amount) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xC1);
        buf_.emit8(modrm(0b11, Reg(4), dst));  // /4 = shl
        buf_.emit8(amount);
    }

    void shr_imm(Reg dst, std::uint8_t amount) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xC1);
        buf_.emit8(modrm(0b11, Reg(5), dst));  // /5 = shr
        buf_.emit8(amount);
    }

    void sar_imm(Reg dst, std::uint8_t amount) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xC1);
        buf_.emit8(modrm(0b11, Reg(7), dst));  // /7 = sar
        buf_.emit8(amount);
    }

    // SMART INSTRUCTION HELPERS

    // smart multiply - uses shift for power of 2, lea for 3/5/9, imm8 for small values
    void imul_smart(Reg dst, Reg src, std::int64_t imm) {
        // power of two → shift
        if (is_power_of_two(imm)) {
            if (dst != src) mov(dst, src);
            shl_imm(dst, static_cast<std::uint8_t>(log2_pow2(static_cast<std::uint64_t>(imm))));
            return;
        }
        
        // multiply by 3, 5, or 9 → lea trick
        if (imm == 3) {
            lea_scaled(dst, src, src, 2);  // [src + src*2]
            return;
        }
        if (imm == 5) {
            lea_scaled(dst, src, src, 4);  // [src + src*4]
            return;
        }
        if (imm == 9) {
            lea_scaled(dst, src, src, 8);  // [src + src*8]
            return;
        }
        
        // small immediate → use imm8 encoding
        if (imm >= -128 && imm <= 127) {
            imul_imm(dst, src, static_cast<std::int32_t>(imm));
            return;
        }
        
        // fallback to standard imul
        if (dst != src) mov(dst, src);
        mov_imm64(Reg::R11, static_cast<std::uint64_t>(imm));
        imul(dst, Reg::R11);
    }

    // smart divide - uses shift for power of 2, idiv otherwise
    void idiv_smart(Reg dst, std::int64_t divisor, bool is_signed) {
        assert(dst == Reg::RAX && "idiv result must be in RAX");
        
        // power of two → shift
        if (is_power_of_two(divisor)) {
            int shift = log2_pow2(static_cast<std::uint64_t>(divisor));
            if (is_signed) {
                sar_imm(Reg::RAX, static_cast<std::uint8_t>(shift));
            } else {
                shr_imm(Reg::RAX, static_cast<std::uint8_t>(shift));
            }
            return;
        }
        
        // fallback to standard idiv
        mov_imm64(Reg::R11, static_cast<std::uint64_t>(divisor));
        if (is_signed) {
            cqo();  // sign-extend RAX into RDX:RAX
        } else {
            xor_(Reg::RDX, Reg::RDX);  // zero RDX for unsigned
        }
        idiv(Reg::R11);
    }

    // smart add - eliminates add 0, uses inc/dec for ±1
    void add_smart(Reg dst, std::int64_t imm) {
        // add 0 → nop (emit nothing)
        if (imm == 0) {
            return;
        }
        
        // add 1 → inc
        if (imm == 1) {
            inc(dst);
            return;
        }
        
        // add -1 → dec
        if (imm == -1) {
            dec(dst);
            return;
        }
        
        // fits in imm32 → use existing add_imm
        if (imm >= (std::numeric_limits<std::int32_t>::min)() && 
            imm <= (std::numeric_limits<std::int32_t>::max)()) {
            add_imm(dst, static_cast<std::int32_t>(imm));
            return;
        }
        
        // large immediate → load to temp reg and add
        mov_imm64(Reg::R11, static_cast<std::uint64_t>(imm));
        add(dst, Reg::R11);
    }

    // smart move - uses xor for 0, or for -1, shorter encodings when possible
    void mov_smart(Reg dst, std::int64_t imm) {
        // move 0 → xor reg, reg (3 bytes vs 10 for mov imm64)
        if (imm == 0) {
            xor_32(dst, dst);  // 32-bit xor zero-extends to 64
            return;
        }
        
        // move -1 → or reg, -1 (4 bytes vs 10)
        if (imm == -1) {
            buf_.emit8(rex(true, Reg::RAX, dst));
            buf_.emit8(0x83);  // or with imm8
            buf_.emit8(modrm(0b11, Reg(EXT_OR), dst));
            buf_.emit8(0xFF);  // -1 as imm8
            return;
        }
        
        // fits in imm32 → use mov_imm32 (7 bytes vs 10)
        if (imm >= (std::numeric_limits<std::int32_t>::min)() && 
            imm <= (std::numeric_limits<std::int32_t>::max)()) {
            mov_imm32(dst, static_cast<std::int32_t>(imm));
            return;
        }
        
        // full 64-bit immediate
        mov_imm64(dst, static_cast<std::uint64_t>(imm));
    }

    // COMPARISON

    void cmp(Reg left, Reg right) {
        buf_.emit8(rex(true, right, left));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, right, left));
    }

    void cmp_mem(Reg left, Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, left, base));
        buf_.emit8(0x3B);
        emit_mem_operand(left, base, offset);
    }

    void cmp_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(7), dst));  // /7 = cmp
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(7), dst));
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void cmp_mem_imm(Reg base, std::int32_t offset, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, base));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            emit_mem_operand(Reg(7), base, offset);
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            emit_mem_operand(Reg(7), base, offset);
            buf_.emit32(static_cast<std::uint32_t>(imm));
        }
    }

    void test(Reg left, Reg right) {
        buf_.emit8(rex(true, right, left));
        buf_.emit8(0x85);
        buf_.emit8(modrm(0b11, right, left));
    }

    // smart compare - uses test for comparing to zero
    void cmp_smart(Reg left, Reg right) {
        // compare reg to itself → always equal, use test
        if (left == right) {
            test(left, left);
            return;
        }
        
        // standard comparison
        cmp(left, right);
    }

    // smart compare with immediate - uses test for zero
    void cmp_smart_imm(Reg left, std::int64_t imm) {
        // compare to 0 → test reg, reg
        if (imm == 0) {
            test(left, left);
            return;
        }
        
        // fits in imm32 → use existing cmp_imm
        if (imm >= (std::numeric_limits<std::int32_t>::min)() && 
            imm <= (std::numeric_limits<std::int32_t>::max)()) {
            cmp_imm(left, static_cast<std::int32_t>(imm));
            return;
        }
        
        // large immediate → load to temp and compare
        mov_imm64(Reg::R11, static_cast<std::uint64_t>(imm));
        cmp(left, Reg::R11);
    }

    // setcc - set byte based on condition code
    void setcc(std::uint8_t cc, Reg dst) {
        // always emit REX to avoid high-byte register ambiguity (AH/CH/DH/BH)
        buf_.emit8(0x40 | (static_cast<std::uint8_t>(dst) >= 8 ? 0x01 : 0x00));
        buf_.emit8(0x0F);
        buf_.emit8(0x90 + cc);
        buf_.emit8(modrm(0b11, Reg::RAX, dst));
    }

    // Condition codes for setcc/jcc
    static constexpr std::uint8_t CC_O  = 0x0;  // Overflow
    static constexpr std::uint8_t CC_NO = 0x1;  // No overflow
    static constexpr std::uint8_t CC_B  = 0x2;  // Below (unsigned <)
    static constexpr std::uint8_t CC_AE = 0x3;  // Above or equal (unsigned >=)
    static constexpr std::uint8_t CC_E  = 0x4;  // Equal
    static constexpr std::uint8_t CC_NE = 0x5;  // Not equal
    static constexpr std::uint8_t CC_BE = 0x6;  // Below or equal (unsigned <=)
    static constexpr std::uint8_t CC_A  = 0x7;  // Above (unsigned >)
    static constexpr std::uint8_t CC_S  = 0x8;  // Sign (negative)
    static constexpr std::uint8_t CC_NS = 0x9;  // No sign (positive)
    static constexpr std::uint8_t CC_L  = 0xC;  // Less (signed <)
    static constexpr std::uint8_t CC_GE = 0xD;  // Greater or equal (signed >=)
    static constexpr std::uint8_t CC_LE = 0xE;  // Less or equal (signed <=)
    static constexpr std::uint8_t CC_G  = 0xF;  // Greater (signed >)

    // CONTROL FLOW

    void jmp_rel32(std::int32_t offset) {
        buf_.emit8(0xE9);
        buf_.emit32(static_cast<std::uint32_t>(offset));
    }

    [[nodiscard]] std::size_t jmp_rel32_placeholder() {
        buf_.emit8(0xE9);
        std::size_t pos = buf_.pos();
        buf_.emit32(0);
        return pos;
    }

    [[nodiscard]] std::size_t jcc_rel32(std::uint8_t cc) {
        buf_.emit8(0x0F);
        buf_.emit8(0x80 + cc);
        std::size_t pos = buf_.pos();
        buf_.emit32(0);
        return pos;
    }

    void patch_jump(std::size_t jump_pos) {
        std::int32_t rel = static_cast<std::int32_t>(buf_.pos() - jump_pos - 4);
        buf_.patch32(jump_pos, static_cast<std::uint32_t>(rel));
    }

    void call_rel32(std::int32_t offset) {
        buf_.emit8(0xE8);
        buf_.emit32(static_cast<std::uint32_t>(offset));
    }

    void call(Reg target) {
        if (static_cast<std::uint8_t>(target) >= 8) buf_.emit8(0x41);
        buf_.emit8(0xFF);
        buf_.emit8(modrm(0b11, Reg(EXT_CALL), target));
    }

    void call_mem(Reg base, std::int32_t offset = 0) {
        buf_.emit8(rex(true, Reg(EXT_CALL), base));
        buf_.emit8(0xFF);
        emit_mem_operand(Reg(EXT_CALL), base, offset);
    }

    void ret() {
        buf_.emit8(0xC3);
    }

    // STACK

    void push(Reg src) {
        if (static_cast<std::uint8_t>(src) >= 8) buf_.emit8(0x41);
        buf_.emit8(0x50 + (static_cast<std::uint8_t>(src) & 7));
    }

    void pop(Reg dst) {
        if (static_cast<std::uint8_t>(dst) >= 8) buf_.emit8(0x41);
        buf_.emit8(0x58 + (static_cast<std::uint8_t>(dst) & 7));
    }

    // FUNCTION PROLOGUE/EPILOGUE

    [[nodiscard]] static std::size_t aligned_stack_allocation(std::size_t local_size, std::size_t save_reg_count = 0) {
        std::size_t total_pushes = 1 + save_reg_count;
        bool already_aligned = (total_pushes % 2) == 1;
        std::size_t total = (local_size + 15) & ~static_cast<std::size_t>(15);
        if (!already_aligned) {
            total += 8;
        }
        return total;
    }

    void prologue(std::size_t local_size, std::span<const Reg> save_regs = {}) {
        push(Reg::RBP);
        mov(Reg::RBP, Reg::RSP);
        
        for (Reg r : save_regs) {
            push(r);
        }
        
        // after call: rsp is 8-aligned (return addr)
        // each push subtracts 8, so after (1 + save_regs) pushes:
        //   odd total pushes -> rsp is 16-aligned
        //   even total pushes -> rsp is 8-aligned
        std::size_t total_pushes = 1 + save_regs.size();
        bool already_aligned = (total_pushes % 2) == 1;
        std::size_t total = local_size;
        total = (total + 15) & ~15;
        if (!already_aligned) total += 8;
        if (total > 0) {
            sub_imm(Reg::RSP, static_cast<std::int32_t>(total));
        }
    }

    [[nodiscard]] std::size_t prologue_patchable(std::span<const Reg> save_regs = {}) {
        push(Reg::RBP);
        mov(Reg::RBP, Reg::RSP);
        for (Reg r : save_regs) {
            push(r);
        }
        buf_.emit8(rex(true, Reg::RAX, Reg::RSP));
        buf_.emit8(0x81);
        buf_.emit8(modrm(0b11, Reg(EXT_SUB), Reg::RSP));
        std::size_t patch_site = buf_.pos();
        buf_.emit32(0);
        return patch_site;
    }

    void epilogue(std::span<const Reg> save_regs = {}) {
        mov(Reg::RSP, Reg::RBP);
        if (!save_regs.empty()) {
            sub_imm(Reg::RSP, static_cast<std::int32_t>(save_regs.size() * 8));
        }
        for (auto it = save_regs.rbegin(); it != save_regs.rend(); ++it) {
            pop(*it);
        }

        pop(Reg::RBP);
        ret();
    }

    // MISC

    void nop() { buf_.emit8(0x90); }
    void int3() { buf_.emit8(0xCC); }

    void movzx_byte(Reg dst, Reg src) {
        buf_.emit8(rex(true, dst, src));
        buf_.emit8(0x0F);
        buf_.emit8(0xB6);
        buf_.emit8(modrm(0b11, dst, src));
    }

    // 32-bit xor zero-extends to 64, good for zeroing regs
    void xor_32(Reg dst, Reg src) {
        // no REX.W so its 32-bit, which zero-extends to 64
        std::uint8_t r = 0x40;
        if (static_cast<std::uint8_t>(src) >= 8) r |= 0x04;
        if (static_cast<std::uint8_t>(dst) >= 8) r |= 0x01;
        if (r != 0x40) buf_.emit8(r);  // only emit rex if needed
        buf_.emit8(0x31);
        buf_.emit8(modrm(0b11, src, dst));
    }

    // ========================================================================
    // ATOMIC OPERATIONS (LOCK prefix)
    // ========================================================================

    // lock xadd [base+offset], src
    // atomically adds src to memory, returns old value in src
    void lock_xadd(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(0xF0);  // LOCK prefix
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x0F);
        buf_.emit8(0xC1);
        emit_mem_operand(src, base, offset);
    }

    // lock cmpxchg [base+offset], src
    // compares rax with memory, if equal stores src, else loads memory into rax
    void lock_cmpxchg(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(0xF0);  // LOCK prefix
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x0F);
        buf_.emit8(0xB1);
        emit_mem_operand(src, base, offset);
    }

    // xchg [base+offset], src (implicitly locked on x86)
    void xchg_mem(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x87);
        emit_mem_operand(src, base, offset);
    }

    // ========================================================================
    // AVX2 SIMD INSTRUCTIONS (VEX-encoded, 256-bit YMM)
    // ========================================================================

    // 3-byte VEX prefix for ymm operations
    // pp: 01=66, 10=F3, 11=F2
    // mmmmm: 00001=0F, 00010=0F38, 00011=0F3A
    // W: 0 or 1
    // vvvv: source register (inverted, 1s complement)
    // L: 1 for 256-bit
    void emit_vex3(std::uint8_t mmmmm, std::uint8_t pp, bool W,
                   std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm,
                   bool L = true) {
        buf_.emit8(0xC4);  // 3-byte VEX
        // byte 1: R~.X~.B~.mmmmm
        std::uint8_t b1 = mmmmm & 0x1F;
        if (!(reg & 0x08)) b1 |= 0x80;   // R~ (inverted)
        b1 |= 0x40;                        // X~ = 1 (no SIB index extension)
        if (!(rm & 0x08)) b1 |= 0x20;     // B~ (inverted)
        buf_.emit8(b1);
        // byte 2: W.vvvv~.L.pp
        std::uint8_t b2 = pp & 0x03;
        if (L) b2 |= 0x04;
        b2 |= ((~vvvv & 0x0F) << 3);  // vvvv inverted
        if (W) b2 |= 0x80;
        buf_.emit8(b2);
    }

    // 4-byte EVEX prefix for zmm operations.
    // This is enough for the subset we currently use:
    // - unmasked ops
    // - no broadcast/rounding
    // - 512-bit vectors
    // - up to 5-bit dst/vvvv register ids
    void emit_evex(std::uint8_t mmmmm, std::uint8_t pp, bool W,
                   std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm,
                   std::uint8_t ll = 0b10) {
        buf_.emit8(0x62);

        // byte 1: R' X' B' R 0 0 mm
        std::uint8_t b1 = mmmmm & 0x03;
        b1 |= 0x40; // X' = 1 (no SIB index extension)
        if (!(rm & 0x08))  b1 |= 0x20; // B'
        if (!(reg & 0x08)) b1 |= 0x10; // R
        if (!(reg & 0x10)) b1 |= 0x80; // R'
        buf_.emit8(b1);

        // byte 2: W vvvv 1 pp
        std::uint8_t b2 = pp & 0x03;
        b2 |= 0x04; // fixed 1 bit
        b2 |= ((~vvvv & 0x0F) << 3);
        if (W) b2 |= 0x80;
        buf_.emit8(b2);

        // byte 3: z L'L b V' aaa
        std::uint8_t b3 = 0;
        b3 |= (ll & 0x03) << 5; // 10 = 512-bit zmm
        if (!(vvvv & 0x10)) b3 |= 0x08; // V'
        buf_.emit8(b3);
    }

    // ========================================================================
    // AVX512 SIMD INSTRUCTIONS (EVEX-encoded, 512-bit ZMM)
    // ========================================================================

    void vmovdqu32_zmm_load(std::uint8_t zmm_dst, Reg base, std::int32_t offset = 0) {
        // EVEX.512.F3.0F.W0 6F /r
        emit_evex(0x01, 0x02, false, zmm_dst, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(zmm_dst & 7), base, offset);
    }

    void vmovdqu32_zmm_store(Reg base, std::int32_t offset, std::uint8_t zmm_src) {
        // EVEX.512.F3.0F.W0 7F /r
        emit_evex(0x01, 0x02, false, zmm_src, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(zmm_src & 7), base, offset);
    }

    void vmovdqa32_zmm(std::uint8_t dst, std::uint8_t src) {
        // EVEX.512.66.0F.W0 6F /r (register-register form)
        emit_evex(0x01, 0x01, false, dst, 0, src);
        buf_.emit8(0x6F);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src & 7)));
    }

    void vpaddd_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F.W0 FE /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFE);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    void vpsubd_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F.W0 FA /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    void vpmulld_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F38.W0 40 /r
        emit_evex(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    void vpbroadcastd_zmm(std::uint8_t zmm_dst, Reg src) {
        // EVEX.512.66.0F38.W0 7C /r
        emit_evex(0x02, 0x01, false, zmm_dst, 0, static_cast<std::uint8_t>(src));
        buf_.emit8(0x7C);
        buf_.emit8(modrm(0b11, Reg(zmm_dst & 7), src));
    }

    // vmovdqu ymm, [base+offset] (256-bit unaligned load)
    void vmovdqu_load(std::uint8_t ymm_dst, Reg base, std::int32_t offset) {
        // VEX.256.F3.0F.WIG 6F /r
        emit_vex3(0x01, 0x02, false, ymm_dst, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(ymm_dst & 7), base, offset);
    }

    // vmovdqu [base+offset], ymm (256-bit unaligned store)
    void vmovdqu_store(Reg base, std::int32_t offset, std::uint8_t ymm_src) {
        // VEX.256.F3.0F.WIG 7F /r
        emit_vex3(0x01, 0x02, false, ymm_src, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(ymm_src & 7), base, offset);
    }

    // vpaddd ymm, ymm, ymm (packed 32-bit integer add)
    void vpaddd(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG FE /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFE);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpaddq ymm, ymm, ymm (packed 64-bit integer add)
    void vpaddq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG D4 /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xD4);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpsubd ymm, ymm, ymm (packed 32-bit integer subtract)
    void vpsubd(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG FA /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpsubq ymm, ymm, ymm (packed 64-bit integer subtract)
    void vpsubq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG FB /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFB);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpmulld ymm, ymm, ymm (packed 32-bit integer multiply low)
    void vpmulld(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F38.WIG 40 /r
        emit_vex3(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpbroadcastd ymm, r32 (broadcast 32-bit value to all 8 lanes)
    // uses movd to xmm first, then vpbroadcastd
    void vpbroadcastd(std::uint8_t ymm_dst, Reg src) {
        // step 1: vmovd xmm, src (VEX.128.66.0F.W0 6E /r)
        emit_vex3(0x01, 0x01, false, ymm_dst, 0, static_cast<std::uint8_t>(src), false);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), src));

        // step 2: vpbroadcastd ymm, xmm (VEX.256.66.0F38.W0 58 /r)
        emit_vex3(0x02, 0x01, false, ymm_dst, 0, ymm_dst);
        buf_.emit8(0x58);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), Reg(ymm_dst & 7)));
    }

    // vpbroadcastq ymm, r64 (broadcast 64-bit value to all 4 lanes)
    // uses vmovq to xmm first, then vpbroadcastq
    void vpbroadcastq(std::uint8_t ymm_dst, Reg src) {
        // step 1: vmovq xmm, src (VEX.128.66.0F.W1 6E /r)
        emit_vex3(0x01, 0x01, true, ymm_dst, 0, static_cast<std::uint8_t>(src), false);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), src));

        // step 2: vpbroadcastq ymm, xmm (VEX.256.66.0F38.W0 59 /r)
        emit_vex3(0x02, 0x01, false, ymm_dst, 0, ymm_dst);
        buf_.emit8(0x59);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), Reg(ymm_dst & 7)));
    }

    // shared memory operand encoding: modrm + optional sib + displacement
    // handles rsp needing sib byte, rbp needing explicit disp, disp8 vs disp32
    void emit_mem_operand(Reg reg_field, Reg base, std::int32_t offset) {
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, reg_field, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, reg_field, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, reg_field, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

private:
    CodeBuffer buf_;
};

// EXECUTABLE MEMORY (Windows)

#ifdef _WIN32

class ExecutableMemory {
public:
    ExecutableMemory(std::size_t size) {
        // allocate as rw, flip to rx after code is written
        ptr_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, 
                           PAGE_READWRITE);
        size_ = size;
    }

    ~ExecutableMemory() {
        if (ptr_) {
            VirtualFree(ptr_, 0, MEM_RELEASE);
        }
    }

    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    ExecutableMemory(ExecutableMemory&& other) noexcept 
        : ptr_(other.ptr_), size_(other.size_), finalized_(other.finalized_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
        other.finalized_ = false;
    }

    ExecutableMemory& operator=(ExecutableMemory&& other) noexcept {
        if (this != &other) {
            if (ptr_) VirtualFree(ptr_, 0, MEM_RELEASE);
            ptr_ = other.ptr_;
            size_ = other.size_;
            finalized_ = other.finalized_;
            other.ptr_ = nullptr;
            other.size_ = 0;
            other.finalized_ = false;
        }
        return *this;
    }

    void* ptr() { return ptr_; }
    std::size_t size() const { return size_; }

    void copy_from(const CodeBuffer& buf) {
        if (!ptr_)
            throw std::logic_error("cannot copy into null executable memory");
        if (buf.pos() > size_)
            throw std::logic_error("code buffer too large for executable memory");
        std::memcpy(ptr_, buf.data(), buf.pos());
        finalized_ = false;
    }

    // flip from rw to rx so we can actually execute the code
    void finalize() {
        if (!ptr_)
            throw std::logic_error("cannot finalize null executable memory");
        DWORD old_protect{};
        if (!VirtualProtect(ptr_, size_, PAGE_EXECUTE_READ, &old_protect))
            throw std::logic_error("VirtualProtect failed during finalize");
#ifdef _WIN32
        FlushInstructionCache(GetCurrentProcess(), ptr_, size_);
#endif
        finalized_ = true;
    }

    template<typename... Args>
    std::int64_t call(Args... args) {
        if (!ptr_)
            throw std::logic_error("cannot call null executable memory");
        if (!finalized_)
            throw std::logic_error("must call finalize() before executing code");
        using FnPtr = std::int64_t(*)(Args...);
        return reinterpret_cast<FnPtr>(ptr_)(args...);
    }

private:
    void* ptr_ = nullptr;
    std::size_t size_ = 0;
    bool finalized_ = false;
};

#endif // _WIN32

} // namespace opus::x64
