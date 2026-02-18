// x64 emitter

module;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

constexpr std::string_view reg_name(Reg r) {
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

// Windows x64 calling convention
constexpr Reg ARG_REGS[] = { Reg::RCX, Reg::RDX, Reg::R8, Reg::R9 };
constexpr Reg CALLER_SAVED[] = { Reg::RAX, Reg::RCX, Reg::RDX, Reg::R8, Reg::R9, Reg::R10, Reg::R11 };
constexpr Reg CALLEE_SAVED[] = { Reg::RBX, Reg::RSI, Reg::RDI, Reg::R12, Reg::R13, Reg::R14, Reg::R15 };

// x64 CODE BUFFER

class CodeBuffer {
public:
    CodeBuffer(std::size_t initial_capacity = 4096) {
        code_.reserve(initial_capacity);
    }

    // Raw byte emission
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

    std::size_t size() const { return code_.size(); }
    std::size_t pos() const { return code_.size(); }
    
    std::uint8_t* data() { return code_.data(); }
    const std::uint8_t* data() const { return code_.data(); }
    
    std::span<std::uint8_t> span() { return std::span<std::uint8_t>(code_); }
    std::span<const std::uint8_t> span() const { return std::span<const std::uint8_t>(code_); }

    void patch32(std::size_t offset, std::uint32_t value) {
        code_[offset]     = static_cast<std::uint8_t>(value);
        code_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        code_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
        code_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    }

    std::size_t create_label() { return pos(); }
    
    std::int32_t rel32(std::size_t from, std::size_t to) {
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
    static constexpr std::uint8_t REX_R = 0x44;  // Extension of ModRM reg
    static constexpr std::uint8_t REX_X = 0x42;  // Extension of SIB index
    static constexpr std::uint8_t REX_B = 0x41;  // Extension of ModRM r/m or SIB base

    std::uint8_t rex(bool w, Reg reg, Reg rm) {
        std::uint8_t r = 0x40;
        if (w) r |= 0x08;
        if (static_cast<std::uint8_t>(reg) >= 8) r |= 0x04;
        if (static_cast<std::uint8_t>(rm) >= 8) r |= 0x01;
        return r;
    }

    std::uint8_t modrm(std::uint8_t mod, Reg reg, Reg rm) {
        return (mod << 6) | ((static_cast<std::uint8_t>(reg) & 7) << 3) | (static_cast<std::uint8_t>(rm) & 7);
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
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, dst, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);  // SIB for RSP
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, dst, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, dst, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    void mov_store(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x89);
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    void lea(Reg dst, Reg base, std::int32_t offset) {
        buf_.emit8(rex(true, dst, base));
        buf_.emit8(0x8D);
        if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, dst, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, dst, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    // ARITHMETIC

    void add(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x01);
        buf_.emit8(modrm(0b11, src, dst));
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

    void sub(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x29);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void sub_imm(Reg dst, std::int32_t imm) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        if (imm >= -128 && imm <= 127) {
            buf_.emit8(0x83);
            buf_.emit8(modrm(0b11, Reg(5), dst));  // /5 = sub
            buf_.emit8(static_cast<std::uint8_t>(imm));
        } else {
            buf_.emit8(0x81);
            buf_.emit8(modrm(0b11, Reg(5), dst));
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

    // idiv reg (RDX:RAX / reg -> RAX, RDX)
    void idiv(Reg src) {
        buf_.emit8(rex(true, Reg::RAX, src));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(7), src));  // /7 = idiv
    }

    // cqo (sign-extend RAX into RDX:RAX)
    void cqo() {
        buf_.emit8(0x48);
        buf_.emit8(0x99);
    }

    void neg(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(3), dst));  // /3 = neg
    }

    // BITWISE

    void and_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x21);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void or_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x09);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void xor_(Reg dst, Reg src) {
        buf_.emit8(rex(true, src, dst));
        buf_.emit8(0x31);
        buf_.emit8(modrm(0b11, src, dst));
    }

    void not_(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xF7);
        buf_.emit8(modrm(0b11, Reg(2), dst));  // /2 = not
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

    // sar reg, cl (arithmetic shift right)
    void sar_cl(Reg dst) {
        buf_.emit8(rex(true, Reg::RAX, dst));
        buf_.emit8(0xD3);
        buf_.emit8(modrm(0b11, Reg(7), dst));  // /7 = sar
    }

    // COMPARISON

    void cmp(Reg left, Reg right) {
        buf_.emit8(rex(true, right, left));
        buf_.emit8(0x39);
        buf_.emit8(modrm(0b11, right, left));
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

    void test(Reg left, Reg right) {
        buf_.emit8(rex(true, right, left));
        buf_.emit8(0x85);
        buf_.emit8(modrm(0b11, right, left));
    }

    // setcc reg (set byte based on condition)
    void setcc(std::uint8_t cc, Reg dst) {
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

    std::size_t jmp_rel32_placeholder() {
        buf_.emit8(0xE9);
        std::size_t pos = buf_.pos();
        buf_.emit32(0);  // Placeholder
        return pos;
    }

    // jcc rel32 (conditional jump)
    std::size_t jcc_rel32(std::uint8_t cc) {
        buf_.emit8(0x0F);
        buf_.emit8(0x80 + cc);
        std::size_t pos = buf_.pos();
        buf_.emit32(0);  // Placeholder
        return pos;
    }

    // Patch a jump to target current position
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
        buf_.emit8(modrm(0b11, Reg(2), target));  // /2 = call
    }

    // ret
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

    // Standard Windows x64 prologue
    void prologue(std::size_t local_size, std::span<const Reg> save_regs = {}) {
        push(Reg::RBP);
        mov(Reg::RBP, Reg::RSP);
        
        // Save callee-saved registers
        for (Reg r : save_regs) {
            push(r);
        }
        
        // Allocate locals (16-byte aligned)
        std::size_t total = local_size + save_regs.size() * 8;
        total = (total + 15) & ~15;  // Align to 16
        if (total > 0) {
            sub_imm(Reg::RSP, static_cast<std::int32_t>(total));
        }
    }

    // Standard epilogue
    void epilogue(std::span<const Reg> save_regs = {}) {
        // Restore callee-saved registers (reverse order)
        for (auto it = save_regs.rbegin(); it != save_regs.rend(); ++it) {
            pop(*it);
        }
        
        mov(Reg::RSP, Reg::RBP);
        pop(Reg::RBP);
        ret();
    }

    // MISC

    // nop
    void nop() { buf_.emit8(0x90); }

    // int3 (breakpoint)
    void int3() { buf_.emit8(0xCC); }

    // movzx (zero-extend byte to 64-bit)
    void movzx_byte(Reg dst, Reg src) {
        buf_.emit8(rex(true, dst, src));
        buf_.emit8(0x0F);
        buf_.emit8(0xB6);
        buf_.emit8(modrm(0b11, dst, src));
    }

    // xor 32-bit (clears upper 32 bits too, useful for zeroing)
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
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    // lock cmpxchg [base+offset], src
    // compares rax with memory, if equal stores src, else loads memory into rax
    void lock_cmpxchg(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(0xF0);  // LOCK prefix
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x0F);
        buf_.emit8(0xB1);
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    // xchg [base+offset], src (implicitly locked on x86)
    void xchg_mem(Reg base, std::int32_t offset, Reg src) {
        buf_.emit8(rex(true, src, base));
        buf_.emit8(0x87);
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, src, base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
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
                   std::uint8_t reg, std::uint8_t vvvv, std::uint8_t rm) {
        buf_.emit8(0xC4);  // 3-byte VEX
        // byte 1: R~.X~.B~.mmmmm
        std::uint8_t b1 = mmmmm & 0x1F;
        if (!(reg & 0x08)) b1 |= 0x80;   // R~ (inverted)
        b1 |= 0x40;                        // X~ = 1 (no SIB index extension)
        if (!(rm & 0x08)) b1 |= 0x20;     // B~ (inverted)
        buf_.emit8(b1);
        // byte 2: W.vvvv~.L.pp
        std::uint8_t b2 = pp & 0x03;
        b2 |= 0x04;  // L=1 for 256-bit
        b2 |= ((~vvvv & 0x0F) << 3);  // vvvv inverted
        if (W) b2 |= 0x80;
        buf_.emit8(b2);
    }

    // vmovdqu ymm, [base+offset] (256-bit unaligned load)
    void vmovdqu_load(std::uint8_t ymm_dst, Reg base, std::int32_t offset) {
        // VEX.256.F3.0F.WIG 6F /r
        emit_vex3(0x01, 0x02, false, ymm_dst, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x6F);
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, Reg(ymm_dst & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, Reg(ymm_dst & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, Reg(ymm_dst & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
    }

    // vmovdqu [base+offset], ymm (256-bit unaligned store)
    void vmovdqu_store(Reg base, std::int32_t offset, std::uint8_t ymm_src) {
        // VEX.256.F3.0F.WIG 7F /r
        emit_vex3(0x01, 0x02, false, ymm_src, 0, static_cast<std::uint8_t>(base));
        buf_.emit8(0x7F);
        if (offset == 0 && (static_cast<std::uint8_t>(base) & 7) != 5) {
            buf_.emit8(modrm(0b00, Reg(ymm_src & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
        } else if (offset >= -128 && offset <= 127) {
            buf_.emit8(modrm(0b01, Reg(ymm_src & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit8(static_cast<std::uint8_t>(offset));
        } else {
            buf_.emit8(modrm(0b10, Reg(ymm_src & 7), base));
            if ((static_cast<std::uint8_t>(base) & 7) == 4) buf_.emit8(0x24);
            buf_.emit32(static_cast<std::uint32_t>(offset));
        }
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
        // step 1: vmovd xmm0, src (VEX.128.66.0F.W0 6E /r)
        buf_.emit8(0xC4);
        std::uint8_t b1 = 0x01;  // mmmmm = 0F
        b1 |= 0x40;  // X~ = 1
        if (!(ymm_dst & 0x08)) b1 |= 0x80;  // R~
        if (!(static_cast<std::uint8_t>(src) & 0x08)) b1 |= 0x20;  // B~
        buf_.emit8(b1);
        buf_.emit8(0x79);  // W=0, vvvv=1111, L=0 (128-bit), pp=01 (66)
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
        buf_.emit8(0xC4);
        std::uint8_t b1 = 0x01;
        b1 |= 0x40;
        if (!(ymm_dst & 0x08)) b1 |= 0x80;
        if (!(static_cast<std::uint8_t>(src) & 0x08)) b1 |= 0x20;
        buf_.emit8(b1);
        buf_.emit8(0xF9);  // W=1, vvvv=1111, L=0, pp=01
        buf_.emit8(0x6E);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), src));

        // step 2: vpbroadcastq ymm, xmm (VEX.256.66.0F38.W0 59 /r)
        emit_vex3(0x02, 0x01, false, ymm_dst, 0, ymm_dst);
        buf_.emit8(0x59);
        buf_.emit8(modrm(0b11, Reg(ymm_dst & 7), Reg(ymm_dst & 7)));
    }

private:
    CodeBuffer buf_;
};

// EXECUTABLE MEMORY (Windows)

#ifdef _WIN32

class ExecutableMemory {
public:
    ExecutableMemory(std::size_t size) {
        ptr_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, 
                           PAGE_EXECUTE_READWRITE);
        size_ = size;
    }

    ~ExecutableMemory() {
        if (ptr_) {
            VirtualFree(ptr_, 0, MEM_RELEASE);
        }
    }

    // Non-copyable
    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    // Movable
    ExecutableMemory(ExecutableMemory&& other) noexcept 
        : ptr_(other.ptr_), size_(other.size_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    ExecutableMemory& operator=(ExecutableMemory&& other) noexcept {
        if (this != &other) {
            if (ptr_) VirtualFree(ptr_, 0, MEM_RELEASE);
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void* ptr() { return ptr_; }
    std::size_t size() const { return size_; }

    void copy_from(const CodeBuffer& buf) {
        if (buf.size() <= size_) {
            std::memcpy(ptr_, buf.data(), buf.size());
        }
    }

    // Execute as a function returning std::int64_t
    template<typename... Args>
    std::int64_t call(Args... args) {
        using FnPtr = std::int64_t(*)(Args...);
        return reinterpret_cast<FnPtr>(ptr_)(args...);
    }

private:
    void* ptr_ = nullptr;
    std::size_t size_ = 0;
};

#endif // _WIN32

} // namespace opus::x64

