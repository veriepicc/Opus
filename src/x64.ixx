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

export module sable.x64;

import sable.types;
import std;

export namespace sable::x64 {

// read an environment variable into out; returns length (0 = unset)
inline std::size_t env_string(const char* name, char* out, std::size_t cap) {
#ifdef _WIN32
    return GetEnvironmentVariableA(name, out, static_cast<DWORD>(cap));
#else
    const char* v = std::getenv(name);
    if (!v) return 0;
    std::size_t n = std::strlen(v);
    if (n >= cap) n = cap - 1;
    std::memcpy(out, v, n);
    out[n] = '\0';
    return n;
#endif
}

enum class Reg : std::uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // 32-bit versions (same encoding, different REX)
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
};

enum class XmmReg : std::uint8_t {
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7,
    XMM8 = 8, XMM9 = 9, XMM10 = 10, XMM11 = 11,
    XMM12 = 12, XMM13 = 13, XMM14 = 14, XMM15 = 15,
};

enum class YmmReg : std::uint8_t {
    YMM0 = 0, YMM1 = 1, YMM2 = 2, YMM3 = 3,
    YMM4 = 4, YMM5 = 5, YMM6 = 6, YMM7 = 7,
    YMM8 = 8, YMM9 = 9, YMM10 = 10, YMM11 = 11,
    YMM12 = 12, YMM13 = 13, YMM14 = 14, YMM15 = 15,
};

enum class ZmmReg : std::uint8_t {
    ZMM0 = 0, ZMM1 = 1, ZMM2 = 2, ZMM3 = 3,
    ZMM4 = 4, ZMM5 = 5, ZMM6 = 6, ZMM7 = 7,
    ZMM8 = 8, ZMM9 = 9, ZMM10 = 10, ZMM11 = 11,
    ZMM12 = 12, ZMM13 = 13, ZMM14 = 14, ZMM15 = 15,
    ZMM16 = 16, ZMM17 = 17, ZMM18 = 18, ZMM19 = 19,
    ZMM20 = 20, ZMM21 = 21, ZMM22 = 22, ZMM23 = 23,
    ZMM24 = 24, ZMM25 = 25, ZMM26 = 26, ZMM27 = 27,
    ZMM28 = 28, ZMM29 = 29, ZMM30 = 30, ZMM31 = 31,
};

enum class KMaskReg : std::uint8_t {
    K0 = 0, K1 = 1, K2 = 2, K3 = 3,
    K4 = 4, K5 = 5, K6 = 6, K7 = 7,
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

    void truncate(std::size_t size) {
        assert(size <= code_.size() && "truncate: size out of bounds");
        code_.resize(size);
    }
    
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
        auto delta = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from) - 4;
        assert(delta >= (std::numeric_limits<std::int32_t>::min)() &&
               delta <= (std::numeric_limits<std::int32_t>::max)() &&
               "rel32 displacement out of range");
        return static_cast<std::int32_t>(delta);
    }

private:
    std::vector<std::uint8_t> code_;
};

class Emitter {
public:
    Emitter() = default;
    
    CodeBuffer& buffer() { return buf_; }
    const CodeBuffer& buffer() const { return buf_; }
    int peephole_eliminated() const { return peephole_eliminated_; }
    void set_peephole_eliminated(int count) { peephole_eliminated_ = count; }

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
        assert((static_cast<std::uint8_t>(index) & 7) != 4 &&
               "lea_scaled: RSP/R12 cannot be used as index");
        
        std::uint8_t scale_bits = 0;
        if (scale == 2) scale_bits = 1;
        else if (scale == 4) scale_bits = 2;
        else if (scale == 8) scale_bits = 3;
        
        buf_.emit8(rex_sib(true, dst, index, base));
        buf_.emit8(0x8D);
        
        bool rbp_base = (static_cast<std::uint8_t>(base) & 7) == 5;
        std::uint8_t mod = rbp_base ? 0b01 : 0b00;
        buf_.emit8(modrm(mod, dst, Reg(4)));  // modrm points to SIB
        // SIB byte: scale.index.base
        buf_.emit8((scale_bits << 6) | 
                   ((static_cast<std::uint8_t>(index) & 7) << 3) | 
                   (static_cast<std::uint8_t>(base) & 7));
        if (rbp_base) {
            buf_.emit8(0x00);  // disp8 = 0
        }
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

    // REX placeholder note: Reg::RAX is used as a placeholder register argument
    // in rex() calls where only the REX prefix bits (W/B/X/R) are needed.
    // It does not represent the actual RAX register in these contexts.

    void div(Reg src) {
        buf_.emit8(rex(true, Reg::RAX, src));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(EXT_DIV), src));
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

    // NOTE: INC has P4 (Pentium 4) partial-flag stall history; add_smart prefers LEA/ADD.
    // This direct INC is used when the encoder explicitly requests it.
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
        (void)dst;
        assert(dst == Reg::RAX && "idiv result must be in RAX");
        assert(divisor != 0 && "division by zero");
        
        // power of two → shift
        if (!is_signed && divisor > 0 && is_power_of_two(divisor)) {
            int shift = log2_pow2(static_cast<std::uint64_t>(divisor));
            shr_imm(Reg::RAX, static_cast<std::uint8_t>(shift));
            return;
        }
        
        // fallback to standard idiv
        mov_imm64(Reg::R11, static_cast<std::uint64_t>(divisor));
        if (is_signed) {
            cqo();  // sign-extend RAX into RDX:RAX
        } else {
            xor_32(Reg::RDX, Reg::RDX);  // zero RDX for unsigned
            div(Reg::R11);
            return;
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

    // smart move - keep mov semantics and prefer smaller immediates
    void mov_smart(Reg dst, std::int64_t imm) {
        // 0 → xor dst32, dst32 (3 bytes, zero-extends to 64-bit)
        if (imm == 0) {
            xor_32(dst, dst);
            return;
        }
        
        // -1 → or reg64, -1 (4 bytes vs 10 for mov imm64)
        if (imm == -1) {
            or_imm(dst, -1);
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

    // smart compare
    void cmp_smart(Reg left, Reg right) {
        // compare reg to itself → always equal, use test
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
        // emit REX.B only when base >= 8 (dst is R8-R15)
        // for RAX-RDX (rm 0-3): no REX prefix needed, avoids AH/CH/DH/BH aliasing
        // for R8-R15 (rm 8-15): emit 0x41 (REX.B) to extend register field
        if (static_cast<std::uint8_t>(dst) >= 8) buf_.emit8(0x41);
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
    
    void jmp_rel32_to(std::size_t target) {
        std::int32_t offset = static_cast<std::int32_t>(target - buf_.pos() - 5);
        buf_.emit8(0xE9);
        buf_.emit32(static_cast<std::uint32_t>(offset));
    }
    
    void jcc_rel32_to(std::uint8_t cc, std::size_t target) {
        std::int32_t offset = static_cast<std::int32_t>(target - buf_.pos() - 6);
        buf_.emit8(0x0F);
        buf_.emit8(0x80 + cc);
        buf_.emit32(static_cast<std::uint32_t>(offset));
    }

    void patch_jump(std::size_t jump_pos) {
        std::int32_t rel = static_cast<std::int32_t>(buf_.pos() - jump_pos - 4);
        buf_.patch32(jump_pos, static_cast<std::uint32_t>(rel));
    }
    
    void mov_imm(Reg dst, std::int32_t imm) {
        // mov r32, imm32 — always use 0xB8+rd encoding
        // (no small-imm path: 0xB0 is byte-register, wrong for 32-bit moves)
        if (static_cast<std::uint8_t>(dst) >= 8) buf_.emit8(0x41);
        buf_.emit8(0xB8 + (static_cast<std::uint8_t>(dst) & 7));
        buf_.emit32(static_cast<std::uint32_t>(imm));
    }
    
    void shl(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xD3);
        buf_.emit8(modrm(0b11, Reg(4), dst));
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
        // emit only REX.B when base >= 8 (matches call(Reg) pattern)
        if (static_cast<std::uint8_t>(base) >= 8) buf_.emit8(0x41);
        buf_.emit8(0xFF);
        emit_mem_operand(Reg(EXT_CALL), base, offset);
    }

    void ret() {
        buf_.emit8(0xC3);
    }

    // prefetchnta [base+offset] - non-temporal prefetch hint
    void prefetchnta(Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x0F);
        buf_.emit8(0x18);
        emit_mem_operand(Reg(0), base, offset);
    }
    
    // prefetcht0 [base+offset] - prefetch to all caches
    void prefetcht0(Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x0F);
        buf_.emit8(0x18);
        emit_mem_operand(Reg(1), base, offset);
    }
    
    // prefetcht1 [base+offset] - prefetch to L2+
    void prefetcht1(Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x0F);
        buf_.emit8(0x18);
        emit_mem_operand(Reg(2), base, offset);
    }
    
    // prefetcht2 [base+offset] - prefetch to L3+
    void prefetcht2(Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x0F);
        buf_.emit8(0x18);
        emit_mem_operand(Reg(3), base, offset);
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

    // NOTE: save_regs contract — caller must ensure save_regs matches prologue_patchable
    // (same register list and ordering). Mismatch causes stack imbalance.
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
        buf_.emit8(rex(false, src, dst));
        buf_.emit8(0x31);
        buf_.emit8(modrm(0b11, src, dst));
    }

    // ========================================================================
    // SSE2 SIMD INSTRUCTIONS (basic 128-bit XMM ops)
    // ========================================================================

    // movd xmm, r64 - move 64-bit GP register to XMM (zero-extend)
    void movd_from_gpr(XmmReg dst, Reg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), src));
    }

    // movd r64, xmm - move XMM to 64-bit GP register
    void movd_to_gpr(Reg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x7E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), dst));
    }

    // movq xmm, r64 - move 64-bit GP register to XMM (SSE2)
    void movq_from_gpr(XmmReg dst, Reg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), src));
    }

    // movq r64, xmm - move XMM to 64-bit GP register (SSE2)
    void movq_to_gpr(Reg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x7E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), dst));
    }

    // movq xmm, [mem] - load 64 bits from memory into XMM (SSE2)
    void movq_load(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x6E);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // movq [mem], xmm - store 64 bits from XMM to memory (SSE2)
    void movq_store(Reg base, std::int32_t offset, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0xD6);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src)), base, offset);
    }

    // movhpd xmm, [mem] - load high 64 bits of XMM from memory (keeps low half)
    void movhpd_load(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x16);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // movhpd [mem], xmm - store high 64 bits of XMM to memory
    void movhpd_store(Reg base, std::int32_t offset, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x17);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src)), base, offset);
    }

    // paddq xmm, xmm - packed add quadword (64-bit x2)
    void paddq(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xD4);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // paddq xmm, [mem] - packed add quadword from memory
    void paddq_mem(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0xD4);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // psubq xmm, xmm - packed subtract quadword (64-bit x2)
    void psubq(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xFB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // psubq xmm, [mem] - packed subtract quadword from memory
    void psubq_mem(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0xFB);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // vmovdqu128 xmm, [mem] - load 128 bits (2x i64) into XMM (SSE2)
    void movdqu128_load(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0xF3);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), base));
        buf_.emit8(0x0F);
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // vmovdqu128 [mem], xmm - store 128 bits (2x i64) from XMM (SSE2)
    void movdqu128_store(Reg base, std::int32_t offset, XmmReg src) {
        buf_.emit8(0xF3);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(src)), base));
        buf_.emit8(0x0F);
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src)), base, offset);
    }

    // vmovq xmm, r64 - move 64 bits from GP to XMM
    void vmovq_from_gpr(XmmReg dst, Reg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), src));
        buf_.emit8(0x0F);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), src));
    }

    // vmovq r64, xmm - move 64 bits from XMM to GP
    void vmovq_to_gpr(Reg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(src)), dst));
        buf_.emit8(0x0F);
        buf_.emit8(0x7E);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), dst));
    }

    // paddd xmm, xmm - packed add dword (32-bit x4)
    void paddd(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xFE);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // psubd xmm, xmm - packed subtract dword (32-bit x4)
    void psubd(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // pmulld xmm, xmm - packed multiply low dword (32-bit x4, result is low 32 bits)
    void pmulld(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0x38);
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // psrldq xmm, imm8 - shift right logical double quadword by imm8 bytes (extract high qword)
    void psrldq(XmmReg dst, XmmReg src, std::uint8_t imm8) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(imm8 & 0xFF);
    }

    // punpcklqdq xmm, xmm - unpack low quadwords (interleave low halves)
    void punpcklqdq(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0x6C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // pshufd xmm, xmm, imm8 - packed shuffle dword lanes
    void pshufd(XmmReg dst, XmmReg src, std::uint8_t imm) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(0x0F);
        buf_.emit8(0x70);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(imm);
    }

    // pmuludq xmm, xmm - multiply low dword of each qword lane into qword (SSE2)
    void pmuludq(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xF4);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // psrlq xmm, imm8 - shift qword lanes right logical
    void psrlq(XmmReg dst, std::uint8_t imm) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(2), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(0x0F);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(2), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(imm);
    }

    // psllq xmm, imm8 - shift qword lanes left
    void psllq(XmmReg dst, std::uint8_t imm) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(6), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(0x0F);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(6), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(imm);
    }

    // vpsrlq ymm, ymm, imm8 - shift qword lanes right logical (VEX.256)
    void vpsrlq_imm(std::uint8_t dst, std::uint8_t src, std::uint8_t imm) {
        emit_vex3(0x01, 0x01, false, dst, 2, src);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(2), Reg(dst)));
        buf_.emit8(imm);
    }

    // vpsllq ymm, ymm, imm8 - shift qword lanes left (VEX.256)
    void vpsllq_imm(std::uint8_t dst, std::uint8_t src, std::uint8_t imm) {
        emit_vex3(0x01, 0x01, false, dst, 6, src);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(6), Reg(dst)));
        buf_.emit8(imm);
    }

    // vpmuludq ymm, ymm, ymm - low-dword qword multiply (VEX.256)
    void vpmuludq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        emit_vex3(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0xF4);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // psrldq xmm, imm8 - shift right logical double quadword (for extracting high lanes)
    void psrldq(XmmReg dst, std::uint8_t shift) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x73);
        buf_.emit8(modrm(0b11, Reg(7), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(shift);
    }

    // por xmm, xmm - bitwise OR
    void por(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xEB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // pxor xmm, xmm - bitwise XOR (also used to zero XMM reg)
    void pxor(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // pand xmm, xmm - bitwise AND
    void pand(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0xDB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // pandn xmm, xmm - bitwise AND NOT (dst = ~dst & src)
    void pandn(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0xDF);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // movdqa xmm, xmm - move aligned double quadword (register to register)
    void movdqa(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(rex(false, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(0x0F);
        buf_.emit8(0x6F);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
    }

    // movdqu xmm, [mem] - move unaligned double quadword (load)
    void movdqu_load(XmmReg dst, Reg base, std::int32_t offset = 0) {
        buf_.emit8(0xF3);
        buf_.emit8(0x0F);
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst)), base, offset);
    }

    // movdqu [mem], xmm - move unaligned double quadword (store)
    void movdqu_store(Reg base, std::int32_t offset, XmmReg src) {
        buf_.emit8(0xF3);
        buf_.emit8(0x0F);
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src)), base, offset);
    }

    // pcmpeqd xmm, xmm - packed compare equal dword
    void pcmpeqd(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x76);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
    }

    // pcmpgtd xmm, xmm - packed compare greater than dword (signed)
    void pcmpgtd(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x66);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
    }

    // pcmpeqq xmm, xmm - packed compare equal quadword (SSE4.1)
    void pcmpeqq(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x38);
        buf_.emit8(0x29);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
    }

    // pmovsxbd xmm, xmm - sign-extend packed bytes to dwords
    void pmovsxbd(XmmReg dst, XmmReg src) {
        buf_.emit8(0x66);
        buf_.emit8(0x0F);
        buf_.emit8(0x38);
        buf_.emit8(0x21);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src)), Reg(static_cast<std::uint8_t>(dst))));
    }

    // cvtsi64sd xmm, r64 - convert signed 64-bit int to double (SSE2)
    void cvtsi64sd(XmmReg dst, Reg src) {
        buf_.emit8(0xF2);
        buf_.emit8(0x0F);
        buf_.emit8(0x2A);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), src));
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
    // FIXED: X̄ now properly encodes index register extension bit
    //   X̄ = 0 when index register needs extension (R8-R15)
    //   X̄ = 1 when index register is standard (RAX-R15)
    void emit_vex3(std::uint8_t mmmmm, std::uint8_t pp, bool W,
                   std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm,
                   bool L = true, std::uint8_t index = 4) {
        buf_.emit8(0xC4);  // 3-byte VEX
        // byte 1: R~.X̄.B̄.mmmmm
        std::uint8_t b1 = mmmmm & 0x1F;
        if (!(reg & 0x08)) b1 |= 0x80;   // R~ (inverted)
        if (index < 8) b1 |= 0x40;      // X̄ = 1 when index is RAX-R15 (no extension)
        // X̄ = 0 (bit 6 clear) when index is R8-R15 (needs extension)
        if (!(rm & 0x08)) b1 |= 0x20;   // B̄ (inverted)
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
    // FIXED: X' now properly encodes index register extension bit
    //   X' = 0 when index register needs extension (R8-R15)
    //   X' = 1 when index register is standard (RAX-R15)
    void emit_evex(std::uint8_t mmmmm, std::uint8_t pp, bool W,
                   std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm,
                   std::uint8_t ll = 0b10, std::uint8_t index = 4) {
        buf_.emit8(0x62);

        // byte 1: R' X' B' R 0 0 mm
        std::uint8_t b1 = mmmmm & 0x03;
        if (index < 8) b1 |= 0x40;      // X' = 1 when index is RAX-R15 (no extension)
        // X' = 0 (bit 6 clear) when index is R8-R15 (needs extension)
        if (!(rm & 0x08))  b1 |= 0x20; // B'
        if (!(reg & 0x08)) b1 |= 0x80; // R̄ → bit 7 (inverted bit 3 of reg id = extends 0-7 to 8-15)
        if (!(reg & 0x10)) b1 |= 0x10; // R̄' → bit 4 (inverted bit 4 of reg id = extends 0-15 to 16-31)
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

    // EVEX with mask register support
    void emit_evex_masked(std::uint8_t mmmmm, std::uint8_t pp, bool W,
                          std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm,
                          std::uint8_t k_mask = 0, bool zero_mask = false,
                          std::uint8_t ll = 0b10, std::uint8_t index = 4) {
        buf_.emit8(0x62);

        // byte 1: R' X' B' R 0 0 mm
        std::uint8_t b1 = mmmmm & 0x03;
        if (index < 8) b1 |= 0x40;      // X' = 1 when index is RAX-R15 (no extension)
        // X' = 0 (bit 6 clear) when index is R8-R15 (needs extension)
        if (!(rm & 0x08))  b1 |= 0x20; // B'
        if (!(reg & 0x08)) b1 |= 0x80; // R̄ → bit 7
        if (!(reg & 0x10)) b1 |= 0x10; // R̄' → bit 4
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
        if (zero_mask) b3 |= 0x80; // z = 1 for zero-masking
        if (!(vvvv & 0x10)) b3 |= 0x08; // V'
        b3 |= (k_mask & 0x07); // aaa = mask register (k0-k7)
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

    // vmovntdq [base+offset], zmm - non-temporal streaming store (bypasses cache pollution)
    void vmovntdq_zmm_store(Reg base, std::int32_t offset, std::uint8_t zmm_src) {
        // EVEX.512.66.0F.W0 E7 /r
        emit_evex(0x01, 0x01, false, zmm_src, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0xE7);
        emit_mem_operand(Reg(zmm_src & 7), base, offset);
    }

    // prefetcht0 [base+offset] - pulls cache line into L1 cache
    void prefetch_t0(Reg base, std::int32_t offset) {
        buf_.emit8(rex(false, Reg::RAX, base));
        buf_.emit8(0x0F);
        buf_.emit8(0x18);
        emit_mem_operand(Reg(1), base, offset); // /1 is prefetcht0
    }

    // sfence - store fence for streaming writes
    void sfence() {
        buf_.emit8(0x0F);
        buf_.emit8(0xAE);
        buf_.emit8(0xF8);
    }

    void vmovdqa32_zmm(std::uint8_t dst, std::uint8_t src) {
        // EVEX.512.66.0F.W0 6F /r (register-register form)
        emit_evex(0x01, 0x01, false, dst, 0, src);
        buf_.emit8(0x6F);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src)));
    }

    void vpaddd_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F.W0 FE /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFE);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    void vpsubd_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F.W0 FA /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    void vpmulld_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F38.W0 40 /r
        emit_evex(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    void vpbroadcastd_zmm(std::uint8_t zmm_dst, Reg src) {
        // EVEX.512.66.0F38.W0 7C /r
        emit_evex(0x02, 0x01, false, zmm_dst, 0, static_cast<std::uint8_t>(src));
        buf_.emit8(0x7C);
        buf_.emit8(modrm(0b11, Reg(zmm_dst ), src));
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
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpaddq ymm, ymm, ymm (packed 64-bit integer add)
    void vpaddq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG D4 /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xD4);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpsubd ymm, ymm, ymm (packed 32-bit integer subtract)
    void vpsubd(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG FA /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpsubq ymm, ymm, ymm (packed 64-bit integer subtract)
    void vpsubq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F.WIG FB /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xFB);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpmulld ymm, ymm, ymm (packed 32-bit integer multiply low)
    void vpmulld(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F38.WIG 40 /r
        emit_vex3(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpbroadcastd ymm, r32 (broadcast 32-bit value to all 8 lanes)
    // uses movd to xmm first, then vpbroadcastd
    void vpbroadcastd(std::uint8_t ymm_dst, Reg src) {
        // step 1: vmovd xmm, src (VEX.128.66.0F.W0 6E /r)
        emit_vex3(0x01, 0x01, false, ymm_dst, 0, static_cast<std::uint8_t>(src), false);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(ymm_dst), src));

        // step 2: vpbroadcastd ymm, xmm (VEX.256.66.0F38.W0 58 /r)
        emit_vex3(0x02, 0x01, false, ymm_dst, 0, ymm_dst);
        buf_.emit8(0x58);
        buf_.emit8(modrm(0b11, Reg(ymm_dst), Reg(ymm_dst)));
    }

    // vpbroadcastq ymm, r64 (broadcast 64-bit value to all 4 lanes)
    // uses vmovq to xmm first, then vpbroadcastq
    void vpbroadcastq(std::uint8_t ymm_dst, Reg src) {
        // step 1: vmovq xmm, src (VEX.128.66.0F.W1 6E /r)
        emit_vex3(0x01, 0x01, true, ymm_dst, 0, static_cast<std::uint8_t>(src), false);
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(ymm_dst), src));

        // step 2: vpbroadcastq ymm, xmm (VEX.256.66.0F38.W0 59 /r)
        emit_vex3(0x02, 0x01, false, ymm_dst, 0, ymm_dst);
        buf_.emit8(0x59);
        buf_.emit8(modrm(0b11, Reg(ymm_dst), Reg(ymm_dst)));
    }

    // ========================================================================
    // AVX2 SIMD BINARY OPERATIONS (256-bit YMM)
    // ========================================================================

    // vpaddq ymm, ymm, [base+offset] (packed 64-bit add from memory)
    void vpaddq_mem(std::uint8_t ymm_dst, std::uint8_t ymm_src, Reg base, std::int32_t offset) {
        // VEX.256.66.0F.W1 D4 /r with memory operand
        emit_vex3(0x01, 0x01, true, ymm_dst, ymm_src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xD4);
        emit_mem_operand(Reg(ymm_dst & 7), base, offset);
    }

    // vpsubq ymm, ymm, [base+offset] (packed 64-bit sub from memory)
    void vpsubq_mem(std::uint8_t ymm_dst, std::uint8_t ymm_src, Reg base, std::int32_t offset) {
        // VEX.256.66.0F.W1 FB /r with memory operand
        emit_vex3(0x01, 0x01, true, ymm_dst, ymm_src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xFB);
        emit_mem_operand(Reg(ymm_dst & 7), base, offset);
    }

    // vpor ymm, ymm, ymm (bitwise OR)
    void vpor(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F EB /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xEB);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpand ymm, ymm, ymm (bitwise AND)
    void vpand(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F DB /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xDB);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpxor ymm, ymm, ymm (bitwise XOR)
    void vpxor(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F EF /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpcmpgtq ymm, ymm, ymm (packed compare greater than quadword)
    void vpcmpgtq(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F38.W1 37 /r
        emit_vex3(0x02, 0x01, true, dst, src1, src2);
        buf_.emit8(0x37);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpcmpgtq ymm, ymm, [mem] (packed compare greater than from memory)
    void vpcmpgtq_mem(std::uint8_t dst, std::uint8_t src, Reg base, std::int32_t offset) {
        emit_vex3(0x02, 0x01, true, dst, src, static_cast<std::uint8_t>(base));
        buf_.emit8(0x37);
        emit_mem_operand(Reg(dst & 7), base, offset);
    }

    // vpmadd52 ymm, ymm, ymm (unsigned multiply-add of 64-bit -> 128-bit accumulator)
    // Used in HFT workloads for ultra-low-latency multiply-accumulate
    void vpmadd52(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F38.W0 B5 /r
        emit_vex3(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0xB5);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vpmadd52 with memory operand
    void vpmadd52_mem(std::uint8_t dst, std::uint8_t src, Reg base, std::int32_t offset) {
        emit_vex3(0x02, 0x01, false, dst, src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xB5);
        emit_mem_operand(Reg(dst & 7), base, offset);
    }

    // vextracti128 ymm, [base+offset], imm8 (extract 128 bits from ymm to memory)
    void vextracti128(Reg base, std::int32_t offset, std::uint8_t ymm_src, std::uint8_t imm8) {
        // VEX.256.66.0F3A.W0 39 /r /ib
        emit_vex3(0x03, 0x01, false, 0, ymm_src, static_cast<std::uint8_t>(base));
        buf_.emit8(0x39);
        emit_mem_operand(Reg(ymm_src & 7), base, offset);
        buf_.emit8(imm8 & 0x1);
    }
    
    // vextracti128 xmm, ymm, imm8 (extract 128 bits from ymm to xmm register)
    void vextracti128(XmmReg dst, YmmReg src, std::uint8_t imm8) {
        // VEX.256.66.0F3A.W0 39 /r /ib
        emit_vex3(0x03, 0x01, false, static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src), 0);
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(imm8 & 0x1);
    }

    // vinserti128 ymm, ymm, xmm, imm8 (insert 128 bits into ymm)
    void vinserti128(std::uint8_t ymm_dst, std::uint8_t ymm_src, std::uint8_t xmm_src, std::uint8_t imm8) {
        // VEX.256.66.0F3A.W0 38 /r /ib
        emit_vex3(0x03, 0x01, false, ymm_dst, ymm_src, xmm_src);
        buf_.emit8(0x38);
        buf_.emit8(modrm(0b11, Reg(xmm_src ), Reg(ymm_dst)));
        buf_.emit8(imm8 & 0x1);
    }

    // vpmovmskb r64, ymm (move mask bits to integer register)
    void vpmovmskb(Reg dst, std::uint8_t ymm_src) {
        // VEX.256.66.0F D7 /r
        emit_vex3(0x01, 0x01, false, ymm_src, 0, static_cast<std::uint8_t>(dst));
        buf_.emit8(0xD7);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) ), Reg(ymm_src )));
    }

    // vxorps ymm, ymm, ymm (AVX xor single precision - clears semantics)
    void vxorps(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.0F 57 /r
        emit_vex3(0x01, 0x00, false, dst, src1, src2);
        buf_.emit8(0x57);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // vxorpd ymm, ymm, ymm (AVX xor double precision - clears semantics)
    void vxorpd(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // VEX.256.66.0F 57 /r
        emit_vex3(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0x57);
        buf_.emit8(modrm(0b11, Reg(dst), Reg(src2)));
    }

    // ========================================================================
    // AVX512 SIMD BINARY OPERATIONS (512-bit ZMM)
    // ========================================================================

    // vpaddq zmm, zmm, [base+offset] (packed 64-bit add from memory)
    void vpaddq_zmm_mem(std::uint8_t zmm_dst, std::uint8_t zmm_src, Reg base, std::int32_t offset) {
        // EVEX.512.66.0F.W1 D4 /r
        emit_evex(0x01, 0x01, true, zmm_dst, zmm_src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xD4);
        emit_mem_operand(Reg(zmm_dst & 7), base, offset);
    }

    // vpsubq zmm, zmm, [base+offset] (packed 64-bit sub from memory)
    void vpsubq_zmm_mem(std::uint8_t zmm_dst, std::uint8_t zmm_src, Reg base, std::int32_t offset) {
        // EVEX.512.66.0F.W1 FB /r
        emit_evex(0x01, 0x01, true, zmm_dst, zmm_src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xFB);
        emit_mem_operand(Reg(zmm_dst & 7), base, offset);
    }

    // vpmadd52 zmm, zmm, zmm (unsigned multiply-add 64-bit -> 128-bit)
    void vpmadd52_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F38.W0 B5 /r
        emit_evex(0x02, 0x01, false, dst, src1, src2);
        buf_.emit8(0xB5);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpmadd52 with memory operand
    void vpmadd52_zmm_mem(std::uint8_t dst, std::uint8_t src, Reg base, std::int32_t offset) {
        emit_evex(0x02, 0x01, false, dst, src, static_cast<std::uint8_t>(base));
        buf_.emit8(0xB5);
        emit_mem_operand(Reg(dst & 7), base, offset);
    }

    // vpor zmm, zmm, zmm (bitwise OR)
    void vpor_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F EB /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xEB);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpand zmm, zmm, zmm (bitwise AND)
    void vpand_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F DB /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xDB);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpxor zmm, zmm, zmm (bitwise XOR)
    void vpxor_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F EF /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpcmpgtq zmm, zmm, zmm (packed compare greater than quadword)
    void vpcmpgtq_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F38.W1 37 /r
        emit_evex(0x02, 0x01, true, dst, src1, src2);
        buf_.emit8(0x37);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpmovmskb r64, zmm (move mask bits to integer register)
    void vpmovmskb_zmm(Reg dst, std::uint8_t zmm_src) {
        // EVEX.512.66.0F D7 /r
        emit_evex(0x01, 0x01, false, zmm_src, 0, static_cast<std::uint8_t>(dst));
        buf_.emit8(0xD7);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(zmm_src & 7)));
    }

    // vxorps zmm, zmm, zmm (AVX-512 xor single precision)
    void vxorps_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.0F 57 /r
        emit_evex(0x01, 0x00, false, dst, src1, src2);
        buf_.emit8(0x57);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vxorpd zmm, zmm, zmm (AVX-512 xor double precision)
    void vxorpd_zmm(std::uint8_t dst, std::uint8_t src1, std::uint8_t src2) {
        // EVEX.512.66.0F 57 /r
        emit_evex(0x01, 0x01, false, dst, src1, src2);
        buf_.emit8(0x57);
        buf_.emit8(modrm(0b11, Reg(dst & 7), Reg(src2 & 7)));
    }

    // vpcmpd k, zmm, zmm, imm8 (packed compare dword -> mask)
    void vpcmpd(std::uint8_t k_dst, std::uint8_t src1, std::uint8_t src2, std::uint8_t imm) {
        // EVEX.512.66.0F3A.W0 1F /r ib
        emit_evex(0x03, 0x01, false, src1, src2, k_dst);
        buf_.emit8(0x1F);
        buf_.emit8(modrm(0b11, Reg(k_dst & 7), Reg(src2 & 7)));
        buf_.emit8(imm & 0x7);
    }

    // vpcmpq k, zmm, zmm, imm8 (packed compare qword -> mask)
    void vpcmpq(std::uint8_t k_dst, std::uint8_t src1, std::uint8_t src2, std::uint8_t imm) {
        // EVEX.512.66.0F3A.W1 1F /r ib
        emit_evex(0x03, 0x01, true, src1, src2, k_dst);
        buf_.emit8(0x1F);
        buf_.emit8(modrm(0b11, Reg(k_dst & 7), Reg(src2 & 7)));
        buf_.emit8(imm & 0x7);
    }

    // ========================================================================
    // AVX512 MASK OPERATIONS
    // ========================================================================

    // knotb k, k (bitwise NOT mask)
    void knotb(std::uint8_t k_dst, std::uint8_t k_src) {
        buf_.emit8(0xC4);
        buf_.emit8(0xE1);
        buf_.emit8(0xF8);
        buf_.emit8(0x44);
        buf_.emit8(modrm(0b11, Reg(k_dst & 7), Reg(k_src & 7)));
    }

    // korb k, k, k (bitwise OR masks)
    void korb(std::uint8_t k_dst, std::uint8_t k_src1, std::uint8_t k_src2) {
        buf_.emit8(0xC4);
        buf_.emit8(0xE1);
        buf_.emit8(0xF8);
        buf_.emit8(0x45);
        buf_.emit8(modrm(0b11, Reg(k_src1 & 7), Reg(k_dst & 7)));
        buf_.emit8(modrm(0b11, Reg(k_src2 & 7), Reg(k_dst & 7)));
    }

    // ktestb k, k (set ZF if all bits set, clear CF if any bits set)
    void ktestb(std::uint8_t k1, std::uint8_t k2) {
        buf_.emit8(0xC4);
        buf_.emit8(0xE1);
        buf_.emit8(0xFB);
        buf_.emit8(0x99);
        buf_.emit8(modrm(0b11, Reg(k1 & 7), Reg(k2 & 7)));
    }

    // ktestq k, k (64-bit variant)
    void ktestq(std::uint8_t k1, std::uint8_t k2) {
        buf_.emit8(0xC4);
        buf_.emit8(0xE1);
        buf_.emit8(0xFB);
        buf_.emit8(0x9A);
        buf_.emit8(modrm(0b11, Reg(k1 & 7), Reg(k2 & 7)));
    }

    // ========================================================================
    // AVX512 GATHER/SCATTER
    // ========================================================================

    // vpgatherdd zmm, [base+ymm*4], k (gather dword with mask)
    void vpgatherdd_masked(ZmmReg dst, Reg base, YmmReg index, KMaskReg mask) {
        // EVEX.512.66.0F38.W0 90 /r
        emit_evex_masked(0x02, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(base), static_cast<std::uint8_t>(mask));
        buf_.emit8(0x90);
        emit_mem_operand_scaled_index(Reg(static_cast<std::uint8_t>(dst) & 7), base,
                                       static_cast<Reg>(static_cast<std::uint8_t>(index) & 7), 4);
    }

    // vpgatherdq zmm, [base+xmm*8], k (gather qword with mask)
    void vpgatherdq_masked(ZmmReg dst, Reg base, XmmReg index, KMaskReg mask) {
        emit_evex_masked(0x02, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(base), static_cast<std::uint8_t>(mask));
        buf_.emit8(0x90);
        emit_mem_operand_scaled_index(Reg(static_cast<std::uint8_t>(dst) & 7), base,
                                       static_cast<Reg>(static_cast<std::uint8_t>(index) & 7), 8);
    }

    // vpscatterdq [base+ymm*8], zmm, k (scatter qword with mask)
    void vpscatterdq_masked(Reg base, ZmmReg src, YmmReg index, KMaskReg mask) {
        emit_evex_masked(0x02, 0x01, false,
            static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(src),
            static_cast<std::uint8_t>(base), static_cast<std::uint8_t>(mask));
        buf_.emit8(0xA0);
        emit_mem_operand_scaled_index(Reg(static_cast<std::uint8_t>(index) & 7), base,
                                       static_cast<Reg>(static_cast<std::uint8_t>(src) & 7), 8);
    }

    // ========================================================================
    // AVX512 REDUCTION OPERATIONS
    // ========================================================================

    // vextracti32x8 ymm, zmm, imm8 (extract 256 bits from zmm)
    void vextracti32x8(YmmReg dst, ZmmReg src, std::uint8_t imm) {
        emit_evex(0x03, 0x01, false, static_cast<std::uint8_t>(src),
                  0, static_cast<std::uint8_t>(dst));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst)), Reg(static_cast<std::uint8_t>(src))));
        buf_.emit8(imm & 0x1);
    }

    // vinserti32x8 zmm, zmm, ymm, imm8 (insert 256 bits into zmm)
    void vinserti32x8(ZmmReg dst, ZmmReg src1, YmmReg src2, std::uint8_t imm) {
        emit_evex(0x03, 0x01, false, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x38);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src2)), Reg(static_cast<std::uint8_t>(dst))));
        buf_.emit8(imm & 0x1);
    }

    // vextracti64x4 ymm, zmm, imm8 (extract 256 bits from zmm upper/lower half)
    void vextracti64x4(YmmReg dst, ZmmReg src, std::uint8_t imm) {
        emit_evex(0x03, 0x01, false, static_cast<std::uint8_t>(src),
                  0, static_cast<std::uint8_t>(dst));
        buf_.emit8(0x3A);
        buf_.emit8(0x83);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src) & 7)));
        buf_.emit8(imm & 0x1);
    }

    // vinserti64x4 zmm, zmm, ymm, imm8
    void vinserti64x4(ZmmReg dst, ZmmReg src1, YmmReg src2, std::uint8_t imm) {
        emit_evex(0x03, 0x01, false, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3A);
        buf_.emit8(0x83);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src2) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
        buf_.emit8(imm & 0x1);
    }

    // ========================================================================
    // AVX512 CONVERSION OPERATIONS
    // ========================================================================

    // vpmovdb [mem], zmm, k (compress dword to memory with mask)
    void vpmovdb_masked(Reg base, ZmmReg src, KMaskReg mask, std::int32_t offset = 0) {
        emit_evex_masked(0x02, 0x01, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(mask),
            static_cast<std::uint8_t>(base));
        buf_.emit8(0x21);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vpmovqb [mem], zmm, k (compress qword to memory with mask)
    void vpmovqb_masked(Reg base, ZmmReg src, KMaskReg mask, std::int32_t offset = 0) {
        emit_evex_masked(0x02, 0x01, true, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(mask),
            static_cast<std::uint8_t>(base));
        buf_.emit8(0x23);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vmovdqu32 zmm, zmm (512-bit move)
    void vmovdqu32(ZmmReg dst, ZmmReg src) {
        emit_evex(0x01, 0x02, false, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(src), 0b10);
        buf_.emit8(0x6F);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src) & 7)));
    }

    // vpxord zmm, zmm, zmm (zero zmm register)
    void vpxord(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2), 0b10);
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpxord zmm, zmm/zero (faster zero using k0 mask)
    void vpxord_zero(ZmmReg dst) {
        emit_evex(0x01, 0x01, false, 0, 0, static_cast<std::uint8_t>(dst), 0b10);
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
    }

    // vpcmpeqd k, zmm, zmm (packed compare equal dword -> mask)
    void vpcmpeqd_mask(KMaskReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2), 0b10);
        buf_.emit8(0x76);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpcmpeqq k, zmm, zmm (packed compare equal qword -> mask)
    void vpcmpeqq_mask(KMaskReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2), 0b10);
        buf_.emit8(0x29);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpcmpgtq k, zmm, zmm (packed compare greater than qword -> mask)
    void vpcmpgtq_mask(KMaskReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, true, static_cast<std::uint8_t>(dst),
                  static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2), 0b10);
        buf_.emit8(0x37);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // ========================================================================
    // AVX512 BROADCAST OPERATIONS
    // ========================================================================

    // vbroadcasti32x8 zmm, [mem] (broadcast 256-bit to 512)
    void vbroadcasti32x8(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base), 0b10);
        buf_.emit8(0x5A);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }

    // vbroadcasti64x4 zmm, [mem] (broadcast 256-bit qword to 512)
    void vbroadcasti64x4(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x02, 0x01, true, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base), 0b10);
        buf_.emit8(0x5A);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }

    // kmovb k, r64 (move from mask register to GP)
    void kmovb_to_gpr(Reg dst, std::uint8_t k_src) {
        // VEX.L0.66.0F.W0 90 /r (k -> r32)
        buf_.emit8(0xC4);
        buf_.emit8(0xE1); // R~X~B~ = 111 (no ext regs)
        buf_.emit8(0xF8); // W.vvvv.L.pp = 0.1111.0.10 (W=0, vvvv=1111, L=0, pp=10)
        buf_.emit8(0x90);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(k_src & 7)));
    }

    // kmovb r64, k (move from GP to mask register)
    void kmovb_from_gpr(std::uint8_t k_dst, Reg src) {
        buf_.emit8(0xC4);
        buf_.emit8(0xE1);
        buf_.emit8(0xF8);
        buf_.emit8(0x91);
        buf_.emit8(modrm(0b11, Reg(k_dst & 7), Reg(static_cast<std::uint8_t>(src) & 7)));
    }

    // ========================================================================
    // AVX512 ZMM REGISTER OPERATIONS
    // ========================================================================

    // vpor zmm, zmm, zmm (bitwise OR with ZmmReg enums)
    void vpor_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xEB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpand zmm, zmm, zmm (bitwise AND)
    void vpand_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xDB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpxor zmm, zmm, zmm (bitwise XOR)
    void vpxor_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xEF);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpaddd zmm, zmm, zmm (packed 32-bit add with ZmmReg enums)
    void vpaddd_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xFE);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpsubd zmm, zmm, zmm (packed 32-bit subtract)
    void vpsubd_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xFA);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpaddq zmm, zmm, zmm (packed 64-bit add)
    void vpaddq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, true,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xD4);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpsubq zmm, zmm, zmm (packed 64-bit subtract)
    void vpsubq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, true,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0xFB);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmulld zmm, zmm, zmm (packed 32-bit multiply low)
    void vpmulld_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false,
            static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(src1),
            static_cast<std::uint8_t>(src2));
        buf_.emit8(0x40);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpbroadcastd zmm, r32 (broadcast 32-bit to all lanes)
    void vpbroadcastd_zmm(ZmmReg dst, Reg src) {
        emit_evex(0x02, 0x01, false,
            static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        buf_.emit8(0x7C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), src));
    }

    // vpbroadcastq zmm, r64 (broadcast 64-bit to all lanes)
    void vpbroadcastq_zmm(ZmmReg dst, Reg src) {
        emit_evex(0x02, 0x01, true,
            static_cast<std::uint8_t>(dst), 0, static_cast<std::uint8_t>(src));
        buf_.emit8(0x7C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), src));
    }

    // ========================================================================
    // AVX512 SATURATED ARITHMETIC (AVX512BW)
    // ========================================================================

    // vpmaxsb zmm, zmm, zmm (saturated max signed byte)
    void vpmaxsb_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxsd zmm, zmm, zmm (saturated max signed dword)
    void vpmaxsd_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3D);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxsq zmm, zmm, zmm (saturated max signed qword)
    void vpmaxsq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, true, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3D);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminsb zmm, zmm, zmm (saturated min signed byte)
    void vpminsb_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x38);
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminsd zmm, zmm, zmm (saturated min signed dword)
    void vpminsd_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminsq zmm, zmm, zmm (saturated min signed qword)
    void vpminsq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, true, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxub zmm, zmm, zmm (saturated max unsigned byte)
    void vpmaxub_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0xDE);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxud zmm, zmm, zmm (saturated max unsigned dword)
    void vpmaxud_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3F);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxuq zmm, zmm, zmm (saturated max unsigned qword)
    void vpmaxuq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, true, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3F);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminub zmm, zmm, zmm (saturated min unsigned byte)
    void vpminub_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0xDA);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminud zmm, zmm, zmm (saturated min unsigned dword)
    void vpminud_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3B);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminuq zmm, zmm, zmm (saturated min unsigned qword)
    void vpminuq_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, true, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3B);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpavgb zmm, zmm, zmm (unsigned average byte - useful for blending)
    void vpavgb_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0xE0);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpavgw zmm, zmm, zmm (unsigned average word)
    void vpavgw_zmm(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0xE3);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vmovdqu64 zmm, [base+offset] (512-bit unaligned load)
    void vmovdqu64_load(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x01, 0x02, true, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }

    // vmovdqu64 [base+offset], zmm (512-bit unaligned store)
    void vmovdqu64_store(Reg base, std::int32_t offset, ZmmReg src) {
        emit_evex(0x01, 0x02, true, static_cast<std::uint8_t>(src), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vmovdqu32 zmm, [base+offset] (512-bit load)
    void vmovdqu32_load(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x01, 0x02, false, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base));
buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }
    
    // vmovdqu32 zmm{k}, [base+offset] (512-bit masked load)
    void vmovdqu32_zmm_load_masked(std::uint8_t k_mask, Reg base, std::int32_t offset = 0) {
        emit_evex_masked(0x01, 0x02, false, 0, k_mask, static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(k_mask & 7), base, offset);
    }
    
    // vmovdqu32 [base+offset], zmm{k} (512-bit masked store)
    void vmovdqu32_zmm_store_masked(Reg base, std::int32_t offset, std::uint8_t k_mask, std::uint8_t zmm_src) {
        emit_evex_masked(0x01, 0x02, false, zmm_src, k_mask, static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(zmm_src & 7), base, offset);
    }
    
    // vmovdqu8 zmm, [base+offset] (512-bit unaligned load of bytes)
    void vmovdqu8_load(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }

    // vmovdqu8 [base+offset], zmm (512-bit unaligned store of bytes)
    void vmovdqu8_store(Reg base, std::int32_t offset, ZmmReg src) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(src), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vmovdqu16 zmm, [base+offset] (512-bit unaligned load of words)
    void vmovdqu16_load(ZmmReg dst, Reg base, std::int32_t offset = 0) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(dst), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(dst) & 7), base, offset);
    }

    // vmovdqu16 [base+offset], zmm (512-bit unaligned store of words)
    void vmovdqu16_store(Reg base, std::int32_t offset, ZmmReg src) {
        emit_evex(0x01, 0x01, false, static_cast<std::uint8_t>(src), 0,
                  static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vpavgb zmm, zmm, zmm (unsigned average byte with AVX512BW)
    void vpavgb_bw(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x01, 0x03, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0xE0);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vphaddubd zmm, zmm, zmm (horizontal add of unsigned bytes to dwords)
    void vphaddubd(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x03, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x2C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vphaddd zmm, zmm, zmm (horizontal add of packed dwords)
    void vphaddd(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x02);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vphaddq zmm, zmm, zmm (horizontal add of packed qwords)
    void vphaddq(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x01, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x05);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmaxsb_bw zmm, zmm, zmm (saturated max signed byte with AVX512BW)
    void vpmaxsb_bw(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x03, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x3C);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpminsb_bw zmm, zmm, zmm (saturated min signed byte with AVX512BW)
    void vpminsb_bw(ZmmReg dst, ZmmReg src1, ZmmReg src2) {
        emit_evex(0x02, 0x03, false, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(src1), static_cast<std::uint8_t>(src2));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(dst) & 7), Reg(static_cast<std::uint8_t>(src2) & 7)));
    }

    // vpmovdb zmm, r32 (compress dword to GP register with zero-extension)
    void vpmovdb_from_zmm(ZmmReg src, Reg dst) {
        emit_evex(0x02, 0x01, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
        buf_.emit8(0x31);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
    }

    // vpmovdw zmm, r32 (compress word to GP lower dword)
    void vpmovdw_from_zmm(ZmmReg src, Reg dst) {
        emit_evex(0x02, 0x01, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
        buf_.emit8(0x33);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
    }

    // vpmovqb zmm, r32 (compress qword to GP register)
    void vpmovqb_from_zmm(ZmmReg src, Reg dst) {
        emit_evex(0x02, 0x01, true, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
        buf_.emit8(0x35);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
    }

    // vpmovqw zmm, r32 (compress word to GP lower dword)
    void vpmovqw_from_zmm(ZmmReg src, Reg dst) {
        emit_evex(0x02, 0x01, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(dst));
        buf_.emit8(0x34);
        buf_.emit8(modrm(0b11, Reg(static_cast<std::uint8_t>(src) & 7), Reg(static_cast<std::uint8_t>(dst) & 7)));
    }

    // vpmovswb [base], zmm, k (store with mask, compressed signed word)
    void vpmovswb_masked(Reg base, ZmmReg src, KMaskReg mask, std::int32_t offset = 0) {
        emit_evex_masked(0x02, 0x03, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(mask),
            static_cast<std::uint8_t>(base));
        buf_.emit8(0x2B);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // vpmovuswb [base], zmm, k (store with mask, compressed unsigned word)
    void vpmovuswb_masked(Reg base, ZmmReg src, KMaskReg mask, std::int32_t offset = 0) {
        emit_evex_masked(0x02, 0x03, false, 0,
            static_cast<std::uint8_t>(src), static_cast<std::uint8_t>(mask),
            static_cast<std::uint8_t>(base));
        buf_.emit8(0x2D);
        emit_mem_operand(Reg(static_cast<std::uint8_t>(src) & 7), base, offset);
    }

    // shared memory operand encoding: modrm + optional sib + displacement
    // handles rsp needing sib byte, rbp needing explicit disp, disp8 vs disp32
    void emit_mem_operand(Reg reg_field, Reg base, std::int32_t offset) {
        std::uint8_t base_low = static_cast<std::uint8_t>(base) & 7;
        if (offset == 0 && base_low != 5) {
            buf_.emit8(modrm(0b00, reg_field, base));
            if (base_low == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, reg_field, base));
            if (base_low == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, reg_field, base));
            if (base_low == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    // memory operand with scaled index for gather/scatter
    void emit_mem_operand_scaled_index(Reg reg_field, Reg base, Reg index, std::uint8_t scale) {
        buf_.emit8(modrm(0b00, reg_field, Reg(4)));  // modrm points to SIB
        std::uint8_t scale_bits = 0;
        if (scale == 2) scale_bits = 1;
        else if (scale == 4) scale_bits = 2;
        else if (scale == 8) scale_bits = 3;
        buf_.emit8((scale_bits << 6) |
                   ((static_cast<std::uint8_t>(index) & 7) << 3) |
                   (static_cast<std::uint8_t>(base) & 7));
    }

private:
    CodeBuffer buf_;
    int peephole_eliminated_ = 0;
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

} // namespace sable::x64
