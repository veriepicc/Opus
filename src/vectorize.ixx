export module sable.vectorize;
import sable.x64;
import std;

export namespace sable::vectorize {

// kernel abi: (void* const* buffers, uint64_t count, uint64_t start_index)
// map: writes buffers[output]
// sum: returns accumulated bits in rax
enum class Element { i32, i64, f32, f64 };
enum class Op { load, constant, index, shl_imm, add, sub, mul, bit_and, bit_or, bit_xor };
enum class Result { map, sum };
enum class Width { avx2, avx512, sse2 };
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
    bool avx2 = false, avx512f = false, avx512dq = false, sse2 = true;
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
inline bool commutative(Op op) {
    return op == Op::add || op == Op::mul || op == Op::bit_and || op == Op::bit_or || op == Op::bit_xor;
}

struct Step {
    Node node;
    unsigned dst = 0, lhs = 0, rhs = 0;
    Value original = 0;
};
struct Plan {
    Loop loop;
    std::vector<Step> steps;
    std::vector<unsigned> inputs;
    std::vector<unsigned> active_buffers;
    std::vector<Step> constants;
    bool uses_index = false;
    unsigned root_reg = 0, peak = 0;
};

inline std::uint64_t mask_bits(Element t, std::uint64_t v) {
    return bytes(t) == 4 ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)) : v;
}

inline std::int64_t signed_val(Element t, std::uint64_t v) {
    if (bytes(t) == 4) {
        return static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(v)));
    }
    return static_cast<std::int64_t>(v);
}

inline std::optional<Loop> optimize_dag(const Loop& src, std::string& reason) {
    auto reject = [&](std::string text) -> std::optional<Loop> {
        reason = std::move(text);
        return std::nullopt;
    };
    if (src.nodes.empty() || src.nodes.size() > 256 || src.root >= src.nodes.size())
        return reject("loop expression must contain 1..256 topologically ordered nodes");
    if (src.buffers > 32 || (src.result == Result::map && src.output >= src.buffers))
        return reject("invalid buffer table");
    if (src.element != Element::i32 && src.element != Element::i64 &&
        src.element != Element::f32 && src.element != Element::f64)
        return reject("unsupported element type");
    if (src.result != Result::map && src.result != Result::sum)
        return reject("unsupported result operation");
    if (src.result == Result::sum && floating(src.element) && !src.reassociate_fp)
        return reject("floating reduction requires explicit reassociation permission");

    std::vector<bool> live(src.nodes.size());
    live[src.root] = true;
    for (std::size_t i = src.nodes.size(); i-- > 0;) {
        if (!live[i]) continue;
        const auto& n = src.nodes[i];
        if (n.op < Op::load || n.op > Op::bit_xor) return reject("unsupported operation");
        if (n.op == Op::load && n.bits >= src.buffers) return reject("invalid input buffer");
        if (n.op == Op::shl_imm) {
            if (n.a >= i || floating(src.element)) return reject("invalid shift node");
            live[n.a] = true;
        } else if (binary(n.op)) {
            if (n.a >= i || n.b >= i) return reject("cyclic or forward expression reference");
            if (floating(src.element) && n.op >= Op::bit_and)
                return reject("floating bit operations require an explicit bitcast");
            live[n.a] = live[n.b] = true;
        }
    }

    Loop out;
    out.element = src.element;
    out.result = src.result;
    out.buffers = src.buffers;
    out.output = src.output;
    out.reassociate_fp = src.reassociate_fp;

    auto emit_unique = [&](Node n) -> Value {
        if (commutative(n.op) && n.a > n.b) {
            std::swap(n.a, n.b);
        }
        for (std::size_t i = 0; i < out.nodes.size(); ++i) {
            const auto& e = out.nodes[i];
            if (e.op == n.op && e.a == n.a && e.b == n.b && e.bits == n.bits) {
                return static_cast<Value>(i);
            }
        }
        out.nodes.push_back(n);
        return static_cast<Value>(out.nodes.size() - 1);
    };

    auto is_const = [&](Value v, std::uint64_t& bits) -> bool {
        if (out.nodes[v].op == Op::constant) {
            bits = out.nodes[v].bits;
            return true;
        }
        return false;
    };

    std::vector<Value> remap(src.nodes.size(), 0);
    for (std::size_t i = 0; i < src.nodes.size(); ++i) {
        if (!live[i]) continue;
        const auto& n = src.nodes[i];
        if (n.op == Op::load || n.op == Op::constant || n.op == Op::index) {
            Node c = n;
            if (c.op == Op::constant && !floating(out.element)) {
                c.bits = mask_bits(out.element, c.bits);
            }
            remap[i] = emit_unique(c);
            continue;
        }
        if (n.op == Op::shl_imm) {
            Value a = remap[n.a];
            std::uint64_t ca = 0;
            unsigned max_shift = bytes(out.element) * 8;
            if (n.bits == 0) {
                remap[i] = a;
            } else if (n.bits >= max_shift) {
                remap[i] = emit_unique(Node{Op::constant, 0, 0, 0});
            } else if (is_const(a, ca)) {
                remap[i] = emit_unique(Node{Op::constant, 0, 0, mask_bits(out.element, ca << n.bits)});
            } else {
                remap[i] = emit_unique(Node{Op::shl_imm, a, 0, n.bits});
            }
            continue;
        }

        Value a = remap[n.a];
        Value b = remap[n.b];
        Op op = n.op;

        if (!floating(out.element)) {
            std::uint64_t ca = 0, cb = 0;
            bool ha = is_const(a, ca);
            bool hb = is_const(b, cb);
            if (ha && hb) {
                std::uint64_t res = 0;
                switch (op) {
                case Op::add: res = ca + cb; break;
                case Op::sub: res = ca - cb; break;
                case Op::mul: res = ca * cb; break;
                case Op::bit_and: res = ca & cb; break;
                case Op::bit_or: res = ca | cb; break;
                case Op::bit_xor: res = ca ^ cb; break;
                default: break;
                }
                remap[i] = emit_unique(Node{Op::constant, 0, 0, mask_bits(out.element, res)});
                continue;
            }
            if (commutative(op) && ha && !hb) {
                std::swap(a, b);
                std::swap(ca, cb);
                std::swap(ha, hb);
            }
            if (hb) {
                std::int64_t sb = signed_val(out.element, cb);
                if ((op == Op::add || op == Op::sub || op == Op::bit_or || op == Op::bit_xor) && cb == 0) {
                    remap[i] = a; continue;
                }
                if (op == Op::mul && sb == 0) {
                    remap[i] = emit_unique(Node{Op::constant, 0, 0, 0}); continue;
                }
                if (op == Op::mul && sb == 1) {
                    remap[i] = a; continue;
                }
                if (op == Op::bit_and && cb == 0) {
                    remap[i] = emit_unique(Node{Op::constant, 0, 0, 0}); continue;
                }
                if (op == Op::bit_and && cb == mask_bits(out.element, ~0ULL)) {
                    remap[i] = a; continue;
                }
                if (op == Op::mul && sb > 1) {
                    std::uint64_t u = static_cast<std::uint64_t>(sb);
                    unsigned max_k = bytes(out.element) * 8 - 1;
                    if ((u & (u - 1)) == 0) {
                        unsigned k = static_cast<unsigned>(std::countr_zero(u));
                        if (k <= max_k) {
                            remap[i] = emit_unique(Node{Op::shl_imm, a, 0, k});
                            continue;
                        }
                    } else if (((u - 1) & (u - 2)) == 0) {
                        unsigned k = static_cast<unsigned>(std::countr_zero(u - 1));
                        if (k >= 1 && k <= max_k) {
                            Value sh = emit_unique(Node{Op::shl_imm, a, 0, k});
                            remap[i] = emit_unique(Node{Op::add, sh, a, 0});
                            continue;
                        }
                    } else if (((u + 1) & u) == 0) {
                        unsigned k = static_cast<unsigned>(std::countr_zero(u + 1));
                        if (k >= 2 && k <= max_k) {
                            Value sh = emit_unique(Node{Op::shl_imm, a, 0, k});
                            remap[i] = emit_unique(Node{Op::sub, sh, a, 0});
                            continue;
                        }
                    }
                }
            }
            if (a == b) {
                if (op == Op::sub || op == Op::bit_xor) {
                    remap[i] = emit_unique(Node{Op::constant, 0, 0, 0}); continue;
                }
                if (op == Op::bit_and || op == Op::bit_or) {
                    remap[i] = a; continue;
                }
            }
            auto split_affine = [&](Value v, Value& base, std::uint64_t& off) {
                const auto& vn = out.nodes[v];
                std::uint64_t c = 0;
                if (vn.op == Op::add) {
                    if (is_const(vn.b, c)) { base = vn.a; off = c; return; }
                    if (is_const(vn.a, c)) { base = vn.b; off = c; return; }
                }
                if (vn.op == Op::sub && is_const(vn.b, c)) {
                    base = vn.a; off = mask_bits(out.element, 0ULL - c); return;
                }
                base = v; off = 0;
            };
            if (op == Op::sub) {
                Value ba = 0, bb = 0;
                std::uint64_t oa = 0, ob = 0;
                split_affine(a, ba, oa);
                split_affine(b, bb, ob);
                if (ba == bb) {
                    remap[i] = emit_unique(Node{Op::constant, 0, 0, mask_bits(out.element, oa - ob)});
                    continue;
                }
            }
            if ((op == Op::add || op == Op::sub) && hb) {
                Value ba = 0;
                std::uint64_t oa = 0;
                split_affine(a, ba, oa);
                if (ba != a) {
                    std::uint64_t combined = mask_bits(out.element, op == Op::add ? (oa + cb) : (oa - cb));
                    if (combined == 0) {
                        remap[i] = ba;
                    } else {
                        Value cnode = emit_unique(Node{Op::constant, 0, 0, combined});
                        remap[i] = emit_unique(Node{Op::add, ba, cnode, 0});
                    }
                    continue;
                }
            }
        }
        remap[i] = emit_unique(Node{op, a, b, 0});
    }

    Value opt_root = remap[src.root];

    // Prune nodes that became dead after folding/simplification
    std::vector<bool> final_live(out.nodes.size());
    final_live[opt_root] = true;
    for (std::size_t i = out.nodes.size(); i-- > 0;) {
        if (!final_live[i]) continue;
        const auto& n = out.nodes[i];
        if (n.op == Op::shl_imm) final_live[n.a] = true;
        else if (binary(n.op)) final_live[n.a] = final_live[n.b] = true;
    }

    Loop pruned;
    pruned.element = out.element;
    pruned.result = out.result;
    pruned.buffers = out.buffers;
    pruned.output = out.output;
    pruned.reassociate_fp = out.reassociate_fp;
    std::vector<Value> compact(out.nodes.size(), 0);
    for (std::size_t i = 0; i < out.nodes.size(); ++i) {
        if (!final_live[i]) continue;
        Node n = out.nodes[i];
        if (n.op == Op::shl_imm) n.a = compact[n.a];
        else if (binary(n.op)) { n.a = compact[n.a]; n.b = compact[n.b]; }
        compact[i] = static_cast<Value>(pruned.nodes.size());
        pruned.nodes.push_back(n);
    }
    pruned.root = compact[opt_root];
    if (pruned.nodes.size() > 128)
        return reject("optimized expression exceeded 128 nodes");
    return pruned;
}

inline std::optional<Plan> plan(const Loop& raw_loop, unsigned registers, std::string& reason) {
    auto opt = optimize_dag(raw_loop, reason);
    if (!opt) return std::nullopt;
    const Loop& loop = *opt;

    bool uses_index = false;
    for (const auto& n : loop.nodes) {
        if (n.op == Op::index) { uses_index = true; break; }
    }
    // Regs 13, 14, 15 may be used by index vector state when uses_index is true
    unsigned const_limit = uses_index ? 13 : 16;

    std::vector<unsigned> uses(loop.nodes.size()), assigned(loop.nodes.size());
    for (std::size_t i = 0; i < loop.nodes.size(); ++i) {
        const auto& n = loop.nodes[i];
        if (n.op == Op::shl_imm) {
            ++uses[n.a];
        } else if (binary(n.op)) {
            ++uses[n.a]; ++uses[n.b];
        }
    }
    ++uses[loop.root];
    std::array<bool, 6> busy{};
    Plan p;
    p.loop = loop;
    p.uses_index = uses_index;
    unsigned const_alloc = 8;
    for (std::size_t i = 0; i < loop.nodes.size(); ++i) {
        const auto& n = loop.nodes[i];
        Step s{n, 0, 0, 0, static_cast<Value>(i)};
        if (n.op == Op::constant && const_alloc < const_limit) {
            assigned[i] = s.dst = const_alloc++;
            p.constants.push_back(s);
            p.steps.push_back(s);
            continue;
        }
        if (n.op == Op::index) {
            assigned[i] = s.dst = 14;
            p.steps.push_back(s);
            continue;
        }
        if (n.op == Op::shl_imm) {
            s.lhs = assigned[n.a];
            if (--uses[n.a] == 0 && assigned[n.a] < 6) busy[s.lhs] = false;
        } else if (binary(n.op)) {
            s.lhs = assigned[n.a]; s.rhs = assigned[n.b];
            if (--uses[n.a] == 0 && assigned[n.a] < 6) busy[s.lhs] = false;
            if (--uses[n.b] == 0 && assigned[n.b] < 6) busy[s.rhs] = false;
        }
        unsigned r = 0;
        while (r < registers && busy[r]) ++r;
        if (r == registers) {
            reason = "vector register budget exceeded";
            return std::nullopt;
        }
        busy[r] = true; assigned[i] = s.dst = r;
        p.peak = std::max(p.peak, static_cast<unsigned>(std::count(busy.begin(), busy.end(), true)));
        if (n.op == Op::load) {
            auto b = static_cast<unsigned>(n.bits);
            if (std::find(p.inputs.begin(), p.inputs.end(), b) == p.inputs.end())
                p.inputs.push_back(b);
            if (std::find(p.active_buffers.begin(), p.active_buffers.end(), b) == p.active_buffers.end())
                p.active_buffers.push_back(b);
        }
        p.steps.push_back(s);
    }
    if (loop.result == Result::map) {
        if (std::find(p.active_buffers.begin(), p.active_buffers.end(), loop.output) == p.active_buffers.end())
            p.active_buffers.push_back(loop.output);
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

// packed 64-bit multiply via pmuludq partial products (wrapping semantics, high half dropped)
// uses vector scratch 6 and 7, which the register allocator never assigns to data lanes
inline void i64_mul_expand(E& e, Width width, unsigned dst, unsigned lhs, unsigned rhs) {
    if (width == Width::avx512) {
        e.emit_evex(0x02, 0x01, true, dst, lhs, rhs);
        e.buffer().emit8(0x40);
        e.buffer().emit8(e.modrm(3, R(dst), R(rhs)));
        return;
    }
    if (width == Width::avx2) {
        auto r = [&](unsigned g) { return static_cast<std::uint8_t>(g); };
        e.vpsrlq_imm(6, r(lhs), 32);
        e.vpsrlq_imm(7, r(rhs), 32);
        e.vpmuludq(6, 6, r(rhs));
        e.vpmuludq(7, 7, r(lhs));
        e.vpaddq(6, 6, 7);
        e.vpsllq_imm(6, 6, 32);
        e.vpmuludq(r(dst), r(lhs), r(rhs));
        e.vpaddq(r(dst), r(dst), 6);
        return;
    }
    auto xr = [&](unsigned g) { return static_cast<x64::XmmReg>(g & 15); };
    e.movdqa(xr(7), xr(lhs));
    e.psrlq(xr(7), 32);
    e.pmuludq(xr(7), xr(rhs));
    e.psllq(xr(7), 32);
    e.movdqa(xr(6), xr(rhs));
    e.psrlq(xr(6), 32);
    e.pmuludq(xr(6), xr(lhs));
    e.psllq(xr(6), 32);
    e.paddq(xr(7), xr(6));
    e.movdqa(xr(6), xr(lhs));
    e.pmuludq(xr(6), xr(rhs));
    e.movdqa(xr(dst), xr(6));
    e.paddq(xr(dst), xr(7));
}

inline void vector_shl_imm(E& e, Element type, Width width,
                           unsigned dst, unsigned lhs, std::uint8_t shift) {
    if (width == Width::avx512) {
        if (type == Element::i32) e.vpslld_zmm_imm(static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(lhs), shift);
        else e.vpsllq_zmm_imm(static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(lhs), shift);
        return;
    }
    if (width == Width::avx2) {
        if (type == Element::i32) e.vpslld_imm(static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(lhs), shift);
        else e.vpsllq_imm(static_cast<std::uint8_t>(dst), static_cast<std::uint8_t>(lhs), shift);
        return;
    }
    auto d = static_cast<x64::XmmReg>(dst & 15);
    auto l = static_cast<x64::XmmReg>(lhs & 15);
    if (d != l) e.movdqa(d, l);
    if (type == Element::i32) e.pslld(d, shift);
    else e.psllq(d, shift);
}

inline void vector_binary(E& e, Element type, Width width, Op op,
                          unsigned dst, unsigned lhs, unsigned rhs) {
    if (width == Width::sse2) {
        if (type == Element::i64 && op == Op::mul) {
            i64_mul_expand(e, width, dst, lhs, rhs);
            return;
        }
        auto d = static_cast<x64::XmmReg>(dst & 15);
        auto l = static_cast<x64::XmmReg>(lhs & 15);
        auto r = static_cast<x64::XmmReg>(rhs & 15);
        if (d != l) {
            if (d == r && commutative(op)) {
                r = l;
            } else if (d == r) {
                auto tmp = static_cast<x64::XmmReg>(7);
                e.movdqa(tmp, r);
                e.movdqa(d, l);
                r = tmp;
            } else {
                e.movdqa(d, l);
            }
        }
        switch (op) {
        case Op::add: if (type == Element::i32) e.paddd(d, r); else e.paddq(d, r); break;
        case Op::sub: if (type == Element::i32) e.psubd(d, r); else e.psubq(d, r); break;
        case Op::mul: if (type != Element::i32) return; e.pmulld(d, r); break;
        case Op::bit_and: e.pand(d, r); break;
        case Op::bit_or: e.por(d, r); break;
        case Op::bit_xor: e.pxor(d, r); break;
        default: break;
        }
        return;
    }
    unsigned map = 1, pp = 1, opcode = 0;
    bool w = false;
    if (floating(type)) {
        pp = type == Element::f32 ? 0 : 1;
        w = width == Width::avx512 && type == Element::f64;
        opcode = op == Op::add ? 0x58 : op == Op::sub ? 0x5c : 0x59;
    } else {
        bool q = type == Element::i64;
        if (q && op == Op::mul) {
            i64_mul_expand(e, width, dst, lhs, rhs);
            return;
        }
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
    bool pinned_ptrs = false;
    std::array<R, 32> buf_regs{};

    static constexpr std::array<R, 6> kPinRegs = {
        R::RSI, R::RDI, R::R12, R::R13, R::R14, R::R15
    };

    void load_vector_reg(unsigned reg, R base, std::int32_t offset = 0) {
        if (width == Width::avx512) e.vmovdqu32_zmm_load(static_cast<std::uint8_t>(reg), base, offset);
        else if (width == Width::avx2) e.vmovdqu_load(static_cast<std::uint8_t>(reg), base, offset);
        else e.movdqu128_load(static_cast<x64::XmmReg>(reg & 15), base, offset);
    }
    void store_vector_reg(unsigned reg, R base, std::int32_t offset, bool nt) {
        if (nt) {
            if (width == Width::avx512) e.vmovntdq_zmm_store(base, offset, static_cast<std::uint8_t>(reg));
            else if (width == Width::avx2) e.vmovntdq_store(base, offset, static_cast<std::uint8_t>(reg));
            else e.movntdq_store(base, offset, static_cast<x64::XmmReg>(reg & 15));
        } else {
            if (width == Width::avx512) e.vmovdqu32_zmm_store(base, offset, static_cast<std::uint8_t>(reg));
            else if (width == Width::avx2) e.vmovdqu_store(base, offset, static_cast<std::uint8_t>(reg));
            else e.movdqu128_store(base, offset, static_cast<x64::XmmReg>(reg & 15));
        }
    }
    void broadcast_constant(unsigned reg, std::uint64_t bits) {
        e.mov_imm64(R::RAX, bits);
        broadcast_rax(reg);
    }
    void broadcast_rax(unsigned reg) {
        if (width == Width::avx512) {
            if (size == 4) e.vpbroadcastd_zmm(static_cast<std::uint8_t>(reg), R::RAX);
            else e.vpbroadcastq_zmm(x64::ZmmReg(reg), R::RAX);
        } else if (width == Width::avx2) {
            if (size == 4) e.vpbroadcastd(static_cast<std::uint8_t>(reg), R::RAX);
            else e.vpbroadcastq(static_cast<std::uint8_t>(reg), R::RAX);
        } else {
            auto x = static_cast<x64::XmmReg>(reg & 15);
            if (size == 4) {
                e.buffer().emit8(0x66);
                if (static_cast<std::uint8_t>(x) >= 8) e.buffer().emit8(0x44);
                e.buffer().emit8(0x0F); e.buffer().emit8(0x6E);
                e.buffer().emit8(e.modrm(3, static_cast<R>(x), R::RAX));
                e.pshufd(x, x, 0);
            } else {
                e.vmovq_from_gpr(x, R::RAX);
                e.punpcklqdq(x, x);
            }
        }
    }
    void init_index_vectors(bool dual_stream) {
        if (!p.uses_index) return;
        std::int32_t scratch_off = frame - 64;
        for (unsigned lane = 0; lane < lanes; ++lane) {
            if (size == 4) {
                e.mov_imm32(R::RAX, static_cast<std::int32_t>(lane));
                memory32(e, true, R::RAX, R::RSP, scratch_off + static_cast<std::int32_t>(lane * 4));
            } else {
                e.mov_imm32(R::RAX, static_cast<std::int32_t>(lane));
                e.mov_store(R::RSP, scratch_off + static_cast<std::int32_t>(lane * 8), R::RAX);
            }
        }
        load_vector_reg(14, R::RSP, scratch_off);
        e.mov(R::RAX, R::RBX);
        broadcast_rax(15);
        vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 14, 14, 15);
        if (dual_stream) {
            e.mov_imm32(R::RAX, static_cast<std::int32_t>(lanes));
            broadcast_rax(15);
            vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 13, 14, 15);
            e.mov_imm32(R::RAX, static_cast<std::int32_t>(lanes * 2));
            broadcast_rax(15);
        } else {
            e.mov_imm32(R::RAX, static_cast<std::int32_t>(lanes));
            broadcast_rax(15);
        }
    }

    void vector_step_stream(unsigned stream_bank, std::int32_t disp, unsigned accumulator, bool nt) {
        unsigned reg_off = stream_bank * 3;
        auto map_reg = [&](unsigned r) -> unsigned {
            if (r < 6) return r + reg_off;
            if (r == 14 && stream_bank == 1) return 13;
            return r;
        };
        for (const auto& s : p.steps) {
            const auto& n = s.node;
            if (n.op == Op::load) {
                unsigned buf = static_cast<unsigned>(n.bits);
                if (pinned_ptrs) {
                    load_vector_reg(map_reg(s.dst), buf_regs[buf], disp);
                } else {
                    e.mov_load(R::R11, R::R10, static_cast<std::int32_t>(buf * 8));
                    e.add(R::R11, R::R8);
                    load_vector_reg(map_reg(s.dst), R::R11, 0);
                }
            } else if (n.op == Op::constant || n.op == Op::index) {
            } else if (n.op == Op::shl_imm) {
                vector_shl_imm(e, loop.element, width, map_reg(s.dst), map_reg(s.lhs), static_cast<std::uint8_t>(n.bits));
            } else {
                vector_binary(e, loop.element, width, n.op, map_reg(s.dst), map_reg(s.lhs), map_reg(s.rhs));
            }
        }
        unsigned root_r = map_reg(p.root_reg);
        if (loop.result == Result::map) {
            if (pinned_ptrs) {
                store_vector_reg(root_r, buf_regs[loop.output], disp, nt);
            } else {
                e.mov_load(R::R11, R::R10, static_cast<std::int32_t>(loop.output * 8));
                e.add(R::R11, R::R8);
                store_vector_reg(root_r, R::R11, 0, false);
            }
        } else {
            vector_binary(e, loop.element, width, Op::add, accumulator, accumulator, root_r);
        }
    }

    void emit_vector_loop(unsigned unroll, bool dual_bank, bool nt) {
        if (unroll > 1) {
            init_index_vectors(dual_bank);
            e.cmp_imm(R::RDX, lanes * unroll);
            auto short_loop = e.jcc_rel32(E::CC_B);
            auto top = e.buffer().pos();
            if (pinned_ptrs && !p.inputs.empty()) {
                for (auto b : p.inputs) e.prefetch_t0(buf_regs[b], 512);
            }
            for (unsigned u = 0; u < unroll; ++u) {
                std::int32_t disp = pinned_ptrs ? static_cast<std::int32_t>(u * vector_bytes) : 0;
                vector_step_stream(dual_bank ? (u % 2) : 0, disp, 4 + u % 2, nt);
                if (!pinned_ptrs) e.add_imm(R::R8, vector_bytes);
            }
            if (pinned_ptrs) {
                for (auto b : p.active_buffers) e.add_imm(buf_regs[b], static_cast<std::int32_t>(unroll * vector_bytes));
            }
            if (p.uses_index) {
                if (dual_bank) {
                    vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 14, 14, 15);
                    vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 13, 13, 15);
                } else {
                    vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 14, 14, 15);
                    e.add_imm(R::RBX, lanes);
                }
                if (dual_bank) e.add_imm(R::RBX, lanes * unroll);
            }
            e.sub_imm(R::RDX, lanes * unroll);
            e.cmp_imm(R::RDX, lanes * unroll);
            e.jcc_rel32_to(E::CC_AE, top);
            e.patch_jump(short_loop);
        }

        if (p.uses_index) {
            if (unroll <= 1) {
                init_index_vectors(false);
            } else if (dual_bank) {
                e.mov_imm32(R::RAX, static_cast<std::int32_t>(lanes));
                broadcast_rax(15);
            }
        }
        e.cmp_imm(R::RDX, lanes);
        auto no_vector = e.jcc_rel32(E::CC_B);
        auto top = e.buffer().pos();
        vector_step_stream(0, 0, 4, nt);
        if (pinned_ptrs) {
            for (auto b : p.active_buffers) e.add_imm(buf_regs[b], static_cast<std::int32_t>(vector_bytes));
        } else {
            e.add_imm(R::R8, vector_bytes);
        }
        if (p.uses_index) {
            vector_binary(e, size == 4 ? Element::i32 : Element::i64, width, Op::add, 14, 14, 15);
            e.add_imm(R::RBX, lanes);
        }
        e.sub_imm(R::RDX, lanes);
        e.cmp_imm(R::RDX, lanes);
        e.jcc_rel32_to(E::CC_AE, top);
        e.patch_jump(no_vector);
    }

    void scalar_body() {
        for (const auto& s : p.steps) {
            const auto& n = s.node;
            if (n.op == Op::load) {
                unsigned buf = static_cast<unsigned>(n.bits);
                if (pinned_ptrs) {
                    if (size == 4) memory32(e, false, R::RAX, buf_regs[buf]);
                    else e.mov_load(R::RAX, buf_regs[buf]);
                } else {
                    e.mov_load(R::R11, R::R10, static_cast<std::int32_t>(buf * 8));
                    e.add(R::R11, R::R8);
                    if (size == 4) memory32(e, false, R::RAX, R::R11);
                    else e.mov_load(R::RAX, R::R11);
                }
            } else if (n.op == Op::constant) {
                e.mov_imm64(R::RAX, n.bits);
            } else if (n.op == Op::index) {
                e.mov(R::RAX, R::RBX);
            } else if (n.op == Op::shl_imm) {
                e.mov_load(R::RAX, R::RSP, static_cast<std::int32_t>(n.a * 8));
                e.shl_imm(R::RAX, static_cast<std::uint8_t>(n.bits));
            } else {
                e.mov_load(R::RAX, R::RSP, static_cast<std::int32_t>(n.a * 8));
                e.mov_load(R::R10, R::RSP, static_cast<std::int32_t>(n.b * 8));
                scalar_binary(e, loop.element, n.op);
            }
            e.mov_store(R::RSP, static_cast<std::int32_t>(s.original * 8), R::RAX);
        }
        e.mov_load(R::RAX, R::RSP, static_cast<std::int32_t>(loop.root * 8));
        if (loop.result == Result::map) {
            if (pinned_ptrs) {
                if (size == 4) memory32(e, true, R::RAX, buf_regs[loop.output]);
                else e.mov_store(buf_regs[loop.output], 0, R::RAX);
            } else {
                e.mov_load(R::R11, R::R10, static_cast<std::int32_t>(loop.output * 8));
                e.add(R::R11, R::R8);
                if (size == 4) memory32(e, true, R::RAX, R::R11);
                else e.mov_store(R::R11, 0, R::RAX);
            }
        } else {
            e.mov(R::R10, R::R9);
            scalar_binary(e, loop.element, Op::add);
            e.mov(R::R9, R::RAX);
        }
        if (pinned_ptrs) {
            for (auto b : p.active_buffers) e.add_imm(buf_regs[b], static_cast<std::int32_t>(size));
        } else {
            e.add_imm(R::R8, size);
        }
        if (p.uses_index) e.inc(R::RBX);
        e.sub_imm(R::RDX, 1);
    }

public:
    Kernel(E& emitter, const Loop& l, const Plan& plan, Width w)
        : e(emitter), loop(l), p(plan), width(w), size(bytes(l.element)),
          lanes((w == Width::avx512 ? 64 : w == Width::avx2 ? 32 : 16) / size), vector_bytes(lanes * size),
          frame(static_cast<std::int32_t>((l.nodes.size() * 8 + 192 + 15) & ~std::size_t(15))) {
        if (p.active_buffers.size() <= kPinRegs.size()) {
            pinned_ptrs = true;
            for (std::size_t i = 0; i < p.active_buffers.size(); ++i) {
                buf_regs[p.active_buffers[i]] = kPinRegs[i];
            }
        }
    }

    void emit(unsigned unroll) {
        for (R r : kPinRegs) e.push(r);
        e.push(R::RBX);
        // 7 pushes = 56 bytes + 8 byte retaddr = 64 bytes -> RSP is 16-byte aligned
        e.sub_imm(R::RSP, frame);

        e.mov(R::RBX, R::R8);  // cur_index = start_index
        e.xor_(R::R8, R::R8);  // byte_offset = 0 (non-pinned fallback)
        e.xor_(R::R9, R::R9);  // sum_accum = 0
        if (pinned_ptrs) {
            for (auto b : p.active_buffers) {
                e.mov_load(buf_regs[b], R::RCX, static_cast<std::int32_t>(b * 8));
            }
        } else {
            e.mov(R::R10, R::RCX);
        }

        e.test(R::RDX, R::RDX);
        auto empty = e.jcc_rel32(E::CC_E);

        std::vector<std::size_t> fallback;
        if (loop.result == Result::map && !p.inputs.empty()) {
            e.mov(R::RAX, R::RDX); e.shr_imm(R::RAX, size == 4 ? 62 : 61);
            e.test(R::RAX, R::RAX); fallback.push_back(e.jcc_rel32(E::CC_NE));
            e.mov(R::RAX, R::RDX); e.imul_imm(R::RAX, R::RAX, size);
            // keep byte_span in [rsp + frame - 136] so R10 stays free for non-pinned base table
            e.mov_store(R::RSP, frame - 136, R::RAX);
            for (auto input : p.inputs) {
                if (input == loop.output) continue;
                e.mov_load(R::RAX, R::RCX, static_cast<std::int32_t>(loop.output * 8));
                e.mov_load(R::R11, R::RCX, static_cast<std::int32_t>(input * 8));
                e.cmp(R::RAX, R::R11);
                auto identical = e.jcc_rel32(E::CC_E);
                auto below = e.jcc_rel32(E::CC_B);
                e.sub(R::RAX, R::R11); e.cmp_load(R::RAX, R::RSP, frame - 136);
                fallback.push_back(e.jcc_rel32(E::CC_B));
                auto next = e.jmp_rel32_placeholder();
                e.patch_jump(below);
                e.sub(R::R11, R::RAX); e.cmp_load(R::R11, R::RSP, frame - 136);
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

        const bool dual_bank = (loop.result == Result::map && p.peak <= 3 && unroll >= 2);
        const unsigned eff_unroll = dual_bank ? 2 : unroll;

        bool dst_in_inputs = std::find(p.inputs.begin(), p.inputs.end(), loop.output) != p.inputs.end();
        bool can_nt = pinned_ptrs && loop.result == Result::map && !dst_in_inputs;
        std::size_t after_vector_done = 0;

        if (can_nt) {
            R dst_reg = buf_regs[loop.output];
            std::int32_t nt_threshold_elems = static_cast<std::int32_t>(262144 / size);
            e.cmp_imm(R::RDX, nt_threshold_elems);
            auto use_normal = e.jcc_rel32(E::CC_B);
            e.test_imm(dst_reg, static_cast<std::int32_t>(size - 1));
            auto use_normal_align = e.jcc_rel32(E::CC_NE);

            auto peel_top = e.buffer().pos();
            e.test_imm(dst_reg, static_cast<std::int32_t>(vector_bytes - 1));
            auto peel_done = e.jcc_rel32(E::CC_E);
            e.test(R::RDX, R::RDX);
            auto peel_empty = e.jcc_rel32(E::CC_E);
            scalar_body();
            e.jmp_rel32_to(peel_top);

            e.patch_jump(peel_done);
            emit_vector_loop(eff_unroll, dual_bank, true);
            e.sfence();
            after_vector_done = e.jmp_rel32_placeholder();

            e.patch_jump(peel_empty);
            e.patch_jump(use_normal);
            e.patch_jump(use_normal_align);
        }

        emit_vector_loop(eff_unroll, dual_bank, false);

        if (can_nt) {
            e.patch_jump(after_vector_done);
        }

        if (loop.result == Result::sum) {
            vector_binary(e, loop.element, width, Op::add, 4, 4, 5);
            e.lea(R::R11, R::RSP, frame - 128);
            store_vector_reg(4, R::R11, 0, false);
            for (unsigned lane = 0; lane < lanes; ++lane) {
                if (size == 4) memory32(e, false, R::RAX, R::RSP, frame - 128 + static_cast<std::int32_t>(lane * size));
                else e.mov_load(R::RAX, R::RSP, frame - 128 + static_cast<std::int32_t>(lane * size));
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
        if (width != Width::sse2) {
            e.buffer().emit8(0xc5); e.buffer().emit8(0xf8); e.buffer().emit8(0x77);
        }
        e.add_imm(R::RSP, frame);
        e.pop(R::RBX);
        for (auto it = kPinRegs.rbegin(); it != kPinRegs.rend(); ++it) e.pop(*it);
        e.ret();
    }
};

} // namespace detail

// emits vector loop kernel directly into output emitter
[[nodiscard]] inline Report compile(const Loop& loop, const Target& target, x64::Emitter& output) {
    Report report;
    if (!target.sse2) { report.reason = "sse2 execution support required"; return report; }
    report.width = Width::sse2;
    if (target.preferred == Width::avx512 && target.avx512f) report.width = Width::avx512;
    else if (target.avx2) report.width = Width::avx2;
    auto p = detail::plan(loop, loop.result == Result::sum ? 4 : 6, report.reason);
    if (!p) return report;
    report.lanes = (report.width == Width::avx512 ? 64 : report.width == Width::avx2 ? 32 : 16) / detail::bytes(p->loop.element);
    report.unroll = p->steps.size() > 24 ? 1 : std::clamp(target.unroll, 1u, 4u);
    report.peak_registers = p->peak + (p->loop.result == Result::sum ? 2 : 0);
    report.live_nodes = p->steps.size();
    x64::Emitter temporary;
    detail::Kernel(temporary, p->loop, *p, report.width).emit(report.unroll);
    report.code_bytes = temporary.buffer().pos();
    output.buffer().emit_bytes(temporary.buffer().span());
    report.emitted = true;
    return report;
}

} // namespace sable::vectorize
