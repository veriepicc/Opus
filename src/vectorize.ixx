export module sable.vectorize;
import sable.x64;
import std;

export namespace sable::vectorize {

// kernel abi: (void* const* buffers, uint64_t count)
// map: writes buffers[output]
// sum: returns accumulated bits in rax
enum class Element { i32, i64, f32, f64 };
enum class Op { load, constant, add, sub, mul, bit_and, bit_or, bit_xor };
enum class Result { map, sum };
enum class Width { avx2, avx512 };
using Value = std::uint16_t;

struct Node {
    Op op = Op::constant;
    Value a = 0, b = 0;
    std::uint64_t bits = 0;
};

struct Loop {
    Element element = Element::i64;
    Result result = Result::map;
    std::uint16_t buffers = 0, output = 0;
    std::vector<Node> nodes;
    Value root = 0;
    bool reassociate_fp = false;
};

// target cpu capabilities
struct Target {
    bool avx2 = false, avx512f = false, avx512dq = false;
    Width preferred = Width::avx2;
    unsigned unroll = 2;
};

struct Report {
    bool emitted = false;
    std::string reason;
    Width width = Width::avx2;
    unsigned lanes = 0, unroll = 0, peak_registers = 0;
    std::size_t code_bytes = 0, live_nodes = 0;
};

namespace detail {
using R = x64::Reg;
using E = x64::Emitter;
inline bool floating(Element t) { return t == Element::f32 || t == Element::f64; }
inline unsigned bytes(Element t) { return t == Element::i32 || t == Element::f32 ? 4 : 8; }
inline bool binary(Op op) { return op >= Op::add; }

struct Step {
    Node node;
    unsigned dst = 0, lhs = 0, rhs = 0;
    Value original = 0;
};
struct Plan {
    std::vector<Step> steps;
    std::vector<unsigned> inputs;
    std::vector<Step> constants;
    unsigned root_reg = 0, peak = 0;
};

inline std::optional<Plan> plan(const Loop& loop, unsigned registers, std::string& reason) {
    auto reject = [&](std::string text) -> std::optional<Plan> {
        reason = std::move(text); return std::nullopt;
    };
    if (loop.nodes.empty() || loop.nodes.size() > 128 || loop.root >= loop.nodes.size())
        return reject("loop expression must contain 1..128 topologically ordered nodes");
    if (loop.buffers > 32 || (loop.result == Result::map && loop.output >= loop.buffers))
        return reject("invalid buffer table");
    if (loop.element != Element::i32 && loop.element != Element::i64 &&
        loop.element != Element::f32 && loop.element != Element::f64)
        return reject("unsupported element type");
    if (loop.result != Result::map && loop.result != Result::sum)
        return reject("unsupported result operation");
    if (loop.result == Result::sum && floating(loop.element) && !loop.reassociate_fp)
        return reject("floating reduction requires explicit reassociation permission");

    std::vector<bool> live(loop.nodes.size());
    live[loop.root] = true;
    for (std::size_t i = loop.nodes.size(); i-- > 0;) {
        if (!live[i]) continue;
        const auto& n = loop.nodes[i];
        if (n.op < Op::load || n.op > Op::bit_xor) return reject("unsupported operation");
        if (n.op == Op::load && n.bits >= loop.buffers) return reject("invalid input buffer");
        if (binary(n.op)) {
            if (n.a >= i || n.b >= i) return reject("cyclic or forward expression reference");
            if (floating(loop.element) && n.op >= Op::bit_and)
                return reject("floating bit operations require an explicit bitcast");
            live[n.a] = live[n.b] = true;
        }
    }

    std::vector<unsigned> uses(loop.nodes.size()), assigned(loop.nodes.size());
    for (std::size_t i = 0; i < loop.nodes.size(); ++i)
        if (live[i] && binary(loop.nodes[i].op)) {
            ++uses[loop.nodes[i].a]; ++uses[loop.nodes[i].b];
        }
    ++uses[loop.root];
    std::array<bool, 6> busy{};
    Plan p;
    unsigned const_alloc = 8;
    for (std::size_t i = 0; i < loop.nodes.size(); ++i) {
        if (!live[i]) continue;
        const auto& n = loop.nodes[i];
        Step s{n, 0, 0, 0, static_cast<Value>(i)};
        if (n.op == Op::constant && const_alloc < 16) {
            assigned[i] = s.dst = const_alloc++;
            p.constants.push_back(s);
            p.steps.push_back(s);
            continue;
        }
        if (binary(n.op)) {
            s.lhs = assigned[n.a]; s.rhs = assigned[n.b];
            if (--uses[n.a] == 0 && assigned[n.a] < 6) busy[s.lhs] = false;
            if (--uses[n.b] == 0 && assigned[n.b] < 6) busy[s.rhs] = false;
        }
        unsigned r = 0;
        while (r < registers && busy[r]) ++r;
        if (r == registers) return reject("vector register budget exceeded");
        busy[r] = true; assigned[i] = s.dst = r;
        p.peak = std::max(p.peak, static_cast<unsigned>(std::count(busy.begin(), busy.end(), true)));
        if (n.op == Op::load && std::find(p.inputs.begin(), p.inputs.end(), n.bits) == p.inputs.end())
            p.inputs.push_back(static_cast<unsigned>(n.bits));
        p.steps.push_back(s);
    }
    p.root_reg = assigned[loop.root];
    return p;
}

inline void memory32(E& e, bool store, R reg, R base, std::int32_t offset = 0) {
    auto& b = e.buffer();
    b.emit8(e.rex(false, reg, base)); b.emit8(store ? 0x89 : 0x8b);
    e.emit_mem_operand(reg, base, offset);
}

inline void scalar_binary(E& e, Element type, Op op) {
    if (floating(type)) {
        auto& b = e.buffer();
        b.emit8(0x66); b.emit8(0x48); b.emit8(0x0f); b.emit8(0x6e); b.emit8(0xc0);
        b.emit8(0x66); b.emit8(0x49); b.emit8(0x0f); b.emit8(0x6e); b.emit8(0xca);
        b.emit8(type == Element::f32 ? 0xf3 : 0xf2);
        b.emit8(0x0f); b.emit8(op == Op::add ? 0x58 : op == Op::sub ? 0x5c : 0x59);
        b.emit8(0xc1);
        b.emit8(0x66); b.emit8(0x48); b.emit8(0x0f); b.emit8(0x7e); b.emit8(0xc0);
    } else {
        switch (op) {
        case Op::add: e.add(R::RAX, R::R10); break;
        case Op::sub: e.sub(R::RAX, R::R10); break;
        case Op::mul: e.imul(R::RAX, R::R10); break;
        case Op::bit_and: e.and_(R::RAX, R::R10); break;
        case Op::bit_or: e.or_(R::RAX, R::R10); break;
        case Op::bit_xor: e.xor_(R::RAX, R::R10); break;
        default: break;
        }
    }
}

inline void vector_binary(E& e, Element type, Width width, Op op,
                          unsigned dst, unsigned lhs, unsigned rhs) {
    unsigned map = 1, pp = 1, opcode = 0;
    bool w = false;
    if (floating(type)) {
        pp = type == Element::f32 ? 0 : 1;
        w = width == Width::avx512 && type == Element::f64;
        opcode = op == Op::add ? 0x58 : op == Op::sub ? 0x5c : 0x59;
    } else {
        bool q = type == Element::i64;
        switch (op) {
        case Op::add: opcode = q ? 0xd4 : 0xfe; w = q && width == Width::avx512; break;
        case Op::sub: opcode = q ? 0xfb : 0xfa; w = q && width == Width::avx512; break;
        case Op::mul: map = 2; opcode = 0x40; w = q; break;
        case Op::bit_and: opcode = 0xdb; break;
        case Op::bit_or: opcode = 0xeb; break;
        case Op::bit_xor: opcode = 0xef; break;
        default: break;
        }
    }
    if (width == Width::avx512) e.emit_evex(map, pp, w, dst, lhs, rhs);
    else e.emit_vex3(map, pp, w, dst, lhs, rhs);
    e.buffer().emit8(static_cast<std::uint8_t>(opcode));
    e.buffer().emit8(e.modrm(3, R(dst), R(rhs)));
}

class Kernel {
    E& e;
    const Loop& loop;
    const Plan& p;
    Width width;
    unsigned size, lanes, vector_bytes;
    std::int32_t frame;

    void address(unsigned buffer) {
        e.mov_load(R::R11, R::RCX, static_cast<std::int32_t>(buffer * 8));
        e.add(R::R11, R::R8);
    }
    void load_vector(unsigned reg) {
        e.prefetch_t0(R::R11, 256);
        if (width == Width::avx512) e.vmovdqu32_zmm_load(reg, R::R11);
        else e.vmovdqu_load(reg, R::R11, 0);
    }
    void store_vector(unsigned reg, R base, std::int32_t offset = 0) {
        if (width == Width::avx512) e.vmovdqu32_zmm_store(base, offset, reg);
        else e.vmovdqu_store(base, offset, reg);
    }
    void broadcast_constant(unsigned reg, std::uint64_t bits) {
        e.mov_imm64(R::RAX, bits);
        if (width == Width::avx512) {
            if (size == 4) e.vpbroadcastd_zmm(reg, R::RAX);
            else e.vpbroadcastq_zmm(x64::ZmmReg(reg), R::RAX);
        } else {
            if (size == 4) e.vpbroadcastd(reg, R::RAX);
            else e.vpbroadcastq(reg, R::RAX);
        }
    }
    void vector_body(unsigned accumulator) {
        for (const auto& s : p.steps) {
            const auto& n = s.node;
            if (n.op == Op::load) {
                address(static_cast<unsigned>(n.bits)); load_vector(s.dst);
            } else if (n.op == Op::constant) {
            } else vector_binary(e, loop.element, width, n.op, s.dst, s.lhs, s.rhs);
        }
        if (loop.result == Result::map) {
            address(loop.output); store_vector(p.root_reg, R::R11);
        } else vector_binary(e, loop.element, width, Op::add, accumulator, accumulator, p.root_reg);
        e.add_imm(R::R8, vector_bytes);
        e.sub_imm(R::RDX, lanes);
    }
    void scalar_body() {
        for (const auto& s : p.steps) {
            const auto& n = s.node;
            if (n.op == Op::load) {
                address(static_cast<unsigned>(n.bits));
                if (size == 4) memory32(e, false, R::RAX, R::R11);
                else e.mov_load(R::RAX, R::R11);
            } else if (n.op == Op::constant) e.mov_imm64(R::RAX, n.bits);
            else {
                e.mov_load(R::RAX, R::RSP, n.a * 8);
                e.mov_load(R::R10, R::RSP, n.b * 8);
                scalar_binary(e, loop.element, n.op);
            }
            e.mov_store(R::RSP, s.original * 8, R::RAX);
        }
        e.mov_load(R::RAX, R::RSP, loop.root * 8);
        if (loop.result == Result::map) {
            address(loop.output);
            if (size == 4) memory32(e, true, R::RAX, R::R11);
            else e.mov_store(R::R11, 0, R::RAX);
        } else {
            e.mov(R::R10, R::R9); scalar_binary(e, loop.element, Op::add);
            e.mov(R::R9, R::RAX);
        }
        e.add_imm(R::R8, size); e.sub_imm(R::RDX, 1);
    }

public:
    Kernel(E& emitter, const Loop& l, const Plan& plan, Width w)
        : e(emitter), loop(l), p(plan), width(w), size(bytes(l.element)),
          lanes((w == Width::avx512 ? 64 : 32) / size), vector_bytes(lanes * size),
          frame(static_cast<std::int32_t>((l.nodes.size() * 8 + 128 + 15) & ~std::size_t(15))) {}

    void emit(unsigned unroll) {
        e.sub_imm(R::RSP, frame);
        e.xor_(R::R8, R::R8); e.xor_(R::R9, R::R9);
        e.test(R::RDX, R::RDX);
        auto empty = e.jcc_rel32(E::CC_E);
        std::vector<std::size_t> fallback;
        if (loop.result == Result::map && !p.inputs.empty()) {
            e.mov(R::RAX, R::RDX); e.shr_imm(R::RAX, size == 4 ? 62 : 61);
            e.test(R::RAX, R::RAX); fallback.push_back(e.jcc_rel32(E::CC_NE));
            e.mov(R::R10, R::RDX); e.imul_imm(R::R10, R::R10, size);
            for (auto input : p.inputs) {
                if (input == loop.output) continue;
                e.mov_load(R::RAX, R::RCX, loop.output * 8);
                e.mov_load(R::R11, R::RCX, input * 8);
                e.cmp(R::RAX, R::R11);
                auto identical = e.jcc_rel32(E::CC_E);
                auto below = e.jcc_rel32(E::CC_B);
                e.sub(R::RAX, R::R11); e.cmp(R::RAX, R::R10);
                fallback.push_back(e.jcc_rel32(E::CC_B));
                auto next = e.jmp_rel32_placeholder();
                e.patch_jump(below);
                e.sub(R::R11, R::RAX); e.cmp(R::R11, R::R10);
                fallback.push_back(e.jcc_rel32(E::CC_B));
                e.patch_jump(next); e.patch_jump(identical);
            }
        }
        if (loop.result == Result::sum) {
            vector_binary(e, Element::i32, width, Op::bit_xor, 4, 4, 4);
            vector_binary(e, Element::i32, width, Op::bit_xor, 5, 5, 5);
        }
        for (const auto& c : p.constants) {
            broadcast_constant(c.dst, c.node.bits);
        }
        if (unroll > 1) {
            e.cmp_imm(R::RDX, lanes * unroll);
            auto short_loop = e.jcc_rel32(E::CC_B);
            auto top = e.buffer().pos();
            for (unsigned u = 0; u < unroll; ++u) vector_body(4 + u % 2);
            e.cmp_imm(R::RDX, lanes * unroll); e.jcc_rel32_to(E::CC_AE, top);
            e.patch_jump(short_loop);
        }
        e.cmp_imm(R::RDX, lanes);
        auto no_vector = e.jcc_rel32(E::CC_B);
        auto top = e.buffer().pos();
        vector_body(4);
        e.cmp_imm(R::RDX, lanes); e.jcc_rel32_to(E::CC_AE, top);
        e.patch_jump(no_vector);
        if (loop.result == Result::sum) {
            vector_binary(e, loop.element, width, Op::add, 4, 4, 5);
            e.lea(R::R11, R::RSP, frame - 128); store_vector(4, R::R11);
            for (unsigned lane = 0; lane < lanes; ++lane) {
                if (size == 4) memory32(e, false, R::RAX, R::RSP, frame - 128 + lane * size);
                else e.mov_load(R::RAX, R::RSP, frame - 128 + lane * size);
                e.mov(R::R10, R::R9); scalar_binary(e, loop.element, Op::add);
                e.mov(R::R9, R::RAX);
            }
        }
        for (auto patch : fallback) e.patch_jump(patch);
        e.test(R::RDX, R::RDX);
        auto no_tail = e.jcc_rel32(E::CC_E);
        auto tail = e.buffer().pos();
        scalar_body(); e.test(R::RDX, R::RDX); e.jcc_rel32_to(E::CC_NE, tail);
        e.patch_jump(no_tail); e.patch_jump(empty);
        e.mov(R::RAX, R::R9);
        if (size == 4) {
            e.buffer().emit8(0x89); e.buffer().emit8(0xc0);
        }
        if (loop.result == Result::map) {
            e.sfence();
        }
        e.buffer().emit8(0xc5); e.buffer().emit8(0xf8); e.buffer().emit8(0x77);
        e.add_imm(R::RSP, frame); e.ret();
    }
};

} // namespace detail

// emits vector loop kernel directly into output emitter
[[nodiscard]] inline Report compile(const Loop& loop, const Target& target, x64::Emitter& output) {
    Report report;
    if (!target.avx2) { report.reason = "avx2 execution support required"; return report; }
    report.width = target.preferred == Width::avx512 && target.avx512f ? Width::avx512 : Width::avx2;
    auto p = detail::plan(loop, loop.result == Result::sum ? 4 : 6, report.reason);
    if (!p) return report;
    for (const auto& s : p->steps)
        if (loop.element == Element::i64 && s.node.op == Op::mul &&
            (report.width != Width::avx512 || !target.avx512dq)) {
            report.reason = "packed i64 multiply requires avx512dq"; return report;
        }
    report.lanes = (report.width == Width::avx512 ? 64 : 32) / detail::bytes(loop.element);
    report.unroll = p->steps.size() > 24 ? 1 : std::clamp(target.unroll, 1u, 4u);
    report.peak_registers = p->peak + (loop.result == Result::sum ? 2 : 0);
    report.live_nodes = p->steps.size();
    x64::Emitter temporary;
    detail::Kernel(temporary, loop, *p, report.width).emit(report.unroll);
    report.code_bytes = temporary.buffer().pos();
    output.buffer().emit_bytes(temporary.buffer().span());
    report.emitted = true;
    return report;
}

} // namespace sable::vectorize
