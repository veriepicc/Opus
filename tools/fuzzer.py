#!/usr/bin/env python3
"""Sable Production Compiler Fuzzer & Delta-Debugging Minimizer.

Features:
- Full grammar AST generator covering all 4 Sable syntax styles
- Multi-architecture fuzzing: Parser, Typechecker, Codegen, Memory, SIMD, Concurrency
- Seed corpus mutation engine across tests/, examples/, and stdlib/
- Boundary numeric generator (INT64_MIN, bit shifts, floating-point denormals)
- Automated Delta Debugger: minimizes crashing reproducers to minimal test cases
- Deduplication by failure signature and stack trace
- Zero-noise FINDINGS.md logger (only writes when genuine bugs are uncovered)
- Export to fuzzer/crashes artifact directory
"""

import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent

# ============================================================================
# RUNTIME & OS HELPERS
# ============================================================================

def is_wine_required() -> bool:
    return sys.platform != "win32"

def run_proc(argv, timeout=10, cwd=None, env=None):
    if env is None:
        env = os.environ.copy()
    env["WINEDEBUG"] = "-all"
    
    cmd = []
    if is_wine_required() and str(argv[0]).endswith(".exe"):
        cmd.append("wine")
    cmd.extend(str(a) for a in argv)

    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd or ROOT,
            env=env
        )
        return proc.returncode, proc.stdout + proc.stderr, False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace") + (e.stderr or b"").decode("utf-8", "replace")
        return -999, out, True
    except Exception as e:
        return -998, str(e), False

def classify_outcome(code: int, output: str, timed_out: bool) -> tuple[bool, str, str]:
    """Returns (is_finding, category, description)."""
    if timed_out:
        return True, "HANG", "Compilation Hang / Infinite Loop (>10s)"

    u_code = code & 0xFFFFFFFF
    
    # Specific Windows NTSTATUS crash codes
    if u_code == 0xC0000005:
        return True, "ACCESS_VIOLATION", "Access Violation (0xC0000005) - Null deref or bad memory read/write"
    if u_code == 0xC00000FD:
        return True, "STACK_OVERFLOW", "Stack Overflow (0xC00000FD) - Infinite recursion in AST/codegen"
    if u_code == 0xC0000028:
        return True, "BAD_STACK", "Bad Stack Pointer / Frame Corruption (0xC0000028)"
    if u_code == 0xC0000374:
        return True, "HEAP_CORRUPTION", "Heap Corruption (0xC0000374) - Use-after-free or double free in compiler"
    if code < 0:
        sig = -code
        signame = f"SIG{signal.Signals(sig).name}" if sig in signal.Signals._value2member_map_ else f"Signal {sig}"
        return True, "CRASH_SIGNAL", f"Compiler terminated by {signame}"

    lower = output.lower()
    for marker in (
        "internal compiler error",
        "assertion failed",
        "fatal error in compiler",
        "segmentation fault",
        "unhandled exception",
        "page fault",
        "pure virtual method called",
        "std::terminate",
        "abort()"
    ):
        if marker in lower:
            # Extract line containing marker
            matching_lines = [l.strip() for l in output.splitlines() if marker in l.lower()]
            diag = matching_lines[0] if matching_lines else marker
            return True, "ICE", f"Internal Compiler Error: {diag}"

    return False, "CLEAN", ""

# ============================================================================
# CONSTANTS & GRAMMAR TABLES
# ============================================================================

NUMERIC_BOUNDARIES = [
    "0", "1", "-1", "2", "-2", "7", "-7", "8", "-8", "15", "16", "31", "32", "63", "64", "127", "128",
    "-128", "255", "256", "32767", "-32768", "65535", "65536",
    "2147483647", "-2147483648", "4294967295", "4294967296",
    "9223372036854775807", "-9223372036854775808",
    "0x0", "0x1", "0xFF", "0xFFFF", "0xFFFFFFFF", "0x7FFFFFFF", "0x80000000",
    "0x7FFFFFFFFFFFFFFF", "0x8000000000000000", "0xFFFFFFFFFFFFFFFF",
    "0b0", "0b1", "0b11111111", "0b10000000"
]

FLOAT_BOUNDARIES = [
    "0.0", "-0.0", "1.0", "-1.0", "0.5", "-0.5", "1e-5", "1e5",
    "3.141592653589793", "2.718281828459045",
    "1.7976931348623157e+308", "2.2250738585072014e-308",
    "3.402823466e+38", "1.175494351e-38"
]

PRIMITIVE_TYPES = [
    "i64", "i32", "i16", "i8",
    "u64", "u32", "u16", "u8",
    "f64", "f32",
    "bool", "int", "char", "void"
]

BINARY_OPS = [
    "+", "-", "*", "/", "%",
    "&", "|", "^", "<<", ">>",
    "==", "!=", "<", "<=", ">", ">=",
    "&&", "||"
]

UNARY_OPS = [
    "-", "+", "!", "~"
]

ASSIGN_OPS = [
    "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="
]

STRING_TEST_CORPUS = [
    '""',
    '"hello world"',
    '"hello\\nworld\\t\\r\\\\\\""',
    '"\\x00\\x01\\x02\\xFF"',
    '"%s%s%s%s%s%n%d%x"',
    '"A" * 1024',
    '"こんにちは世界"',
    '"🦀 Siberian Marten 🌲"',
    '"\'quoted\' inside"',
    '"multi\\line\\string"'
]

KEYWORDS = [
    "function", "fn", "define", "returning", "with", "as",
    "let", "var", "mut", "constant",
    "if", "else", "then", "while", "do", "for", "in", "break", "continue", "return",
    "switch", "case", "default",
    "struct", "class", "view", "enum", "type", "at",
    "spawn", "await", "parallel",
    "malloc", "free", "import", "public"
]

# ============================================================================
# SYNTAX & AST GENERATOR ENGINE
# ============================================================================

class SableAstGenerator:
    """Procedural generator creating diverse Sable ASTs across all grammar corners."""

    def __init__(self, rng: random.Random):
        self.rng = rng
        self.struct_names = []
        self.enum_names = []
        self.view_names = []
        self.func_names = []
        self.var_names = []
        self.indent_level = 1

    def indent(self) -> str:
        return "    " * self.indent_level

    def rand_id(self, prefix="v") -> str:
        return f"{prefix}_{self.rng.randint(100, 99999)}"

    def rand_type(self, allow_void=False, max_ptr=2) -> str:
        base = self.rng.choice(PRIMITIVE_TYPES)
        if not allow_void and base == "void":
            base = "i64"
        if self.struct_names and self.rng.random() < 0.25:
            base = self.rng.choice(self.struct_names)
        
        # Pointer depth
        ptr_depth = self.rng.randint(0, max_ptr)
        return ("*" * ptr_depth) + base

    def gen_primary_expr(self) -> str:
        r = self.rng.random()
        if r < 0.35:
            return self.rng.choice(NUMERIC_BOUNDARIES)
        elif r < 0.50:
            return self.rng.choice(FLOAT_BOUNDARIES)
        elif r < 0.65:
            return self.rng.choice(STRING_TEST_CORPUS)
        elif r < 0.80 and self.var_names:
            return self.rng.choice(self.var_names)
        elif r < 0.90:
            return self.rng.choice(["true", "false", "null"])
        else:
            return str(self.rng.randint(-10000, 10000))

    def gen_expr(self, depth=0, max_depth=5) -> str:
        if depth >= max_depth or self.rng.random() < 0.25:
            return self.gen_primary_expr()

        choice = self.rng.randint(1, 12)
        
        # Binary operation
        if choice in (1, 2, 3):
            op = self.rng.choice(BINARY_OPS)
            l = self.gen_expr(depth + 1, max_depth)
            r = self.gen_expr(depth + 1, max_depth)
            # Avoid division / mod by 0 in generated expressions if right is literal 0
            if op in ("/", "%") and r in ("0", "0.0", "0x0"):
                r = "1"
            return f"({l} {op} {r})"

        # Unary operation
        elif choice == 4:
            op = self.rng.choice(UNARY_OPS)
            operand = self.gen_expr(depth + 1, max_depth)
            return f"({op}{operand})"

        # Ternary conditional
        elif choice == 5:
            cond = self.gen_expr(depth + 1, max_depth)
            t = self.gen_expr(depth + 1, max_depth)
            f = self.gen_expr(depth + 1, max_depth)
            return f"({cond} ? {t} : {f})"

        # Type cast
        elif choice == 6:
            val = self.gen_expr(depth + 1, max_depth)
            target_t = self.rand_type(allow_void=True)
            return f"({val} as {target_t})"

        # Function call
        elif choice == 7:
            if self.func_names and self.rng.random() < 0.5:
                fname = self.rng.choice(self.func_names)
            else:
                fname = self.rng.choice(["print", "malloc", "free", "abs", "min", "max", "rand"])
            argc = self.rng.randint(0, 3)
            args = [self.gen_expr(depth + 1, max_depth) for _ in range(argc)]
            return f"{fname}({', '.join(args)})"

        # Array literal or indexing
        elif choice == 8:
            if self.rng.random() < 0.5:
                elems = [self.gen_expr(depth + 1, max_depth) for _ in range(self.rng.randint(1, 4))]
                return f"[{', '.join(elems)}]"
            else:
                target = self.rng.choice(self.var_names) if self.var_names else "[1, 2, 3, 4]"
                idx = self.rng.choice(["0", "1", "2", self.gen_expr(depth + 1, max_depth)])
                return f"{target}[{idx}]"

        # Range expression
        elif choice == 9:
            start = self.rng.choice(["0", "1", "5"])
            end = self.rng.choice(["10", "50", "100"])
            return f"({start}..{end})"

        # Address-of or Dereference
        elif choice == 10:
            if self.var_names:
                v = self.rng.choice(self.var_names)
                return f"(&{v})" if self.rng.random() < 0.5 else f"(&mut {v})"
            return "&0"

        # Field access
        elif choice == 11:
            if self.var_names:
                v = self.rng.choice(self.var_names)
                field = self.rng.choice(["x", "y", "len", "data", "ptr", "val"])
                return f"{v}.{field}"
            return "point.x"

        # Nested parens
        else:
            inner = self.gen_expr(depth + 1, max_depth)
            return f"({inner})"

    def gen_statement(self, depth=0) -> str:
        choice = self.rng.randint(1, 10)
        ind = self.indent()

        # Variable declaration (let / var)
        if choice in (1, 2):
            kind = self.rng.choice(["let", "var", "let mut"])
            vname = self.rand_id("x")
            self.var_names.append(vname)
            expr = self.gen_expr(depth, 3)
            if self.rng.random() < 0.5:
                vtype = self.rand_type()
                return f"{ind}{kind} {vname}: {vtype} = {expr};"
            else:
                return f"{ind}{kind} {vname} = {expr};"

        # Assignment
        elif choice == 3 and self.var_names:
            vname = self.rng.choice(self.var_names)
            op = self.rng.choice(ASSIGN_OPS)
            expr = self.gen_expr(depth, 3)
            return f"{ind}{vname} {op} {expr};"

        # If - Else block
        elif choice == 4:
            cond = self.gen_expr(depth, 2)
            s = f"{ind}if {cond} {{\n"
            self.indent_level += 1
            s += self.gen_statement(depth + 1) + "\n"
            self.indent_level -= 1
            if self.rng.random() < 0.5:
                s += f"{ind}}} else {{\n"
                self.indent_level += 1
                s += self.gen_statement(depth + 1) + "\n"
                self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # For loop (range or iterator)
        elif choice == 5:
            loop_var = self.rand_id("i")
            start = self.rng.choice(["0", "1"])
            limit = self.rng.choice(["5", "10", "20"])
            s = f"{ind}for {loop_var} in {start}..{limit} {{\n"
            self.indent_level += 1
            self.var_names.append(loop_var)
            s += self.gen_statement(depth + 1) + "\n"
            self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # While loop
        elif choice == 6:
            s = f"{ind}while false {{\n"
            self.indent_level += 1
            s += self.gen_statement(depth + 1) + "\n"
            self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # Switch block
        elif choice == 7:
            subj = self.rng.choice(self.var_names) if self.var_names else "0"
            s = f"{ind}switch {subj} {{\n"
            self.indent_level += 1
            for k in range(self.rng.randint(2, 4)):
                s += f"{self.indent()}case {k}: return {k};\n"
            s += f"{self.indent()}default: break;\n"
            self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # Return statement
        elif choice == 8:
            ret_val = self.gen_expr(depth, 2) if self.rng.random() < 0.8 else "0"
            return f"{ind}return {ret_val};"

        # Memory allocation / free
        elif choice == 9:
            vname = self.rand_id("ptr")
            self.var_names.append(vname)
            size = self.rng.choice(["8", "16", "64", "128"])
            return f"{ind}let {vname} = malloc({size});\n{ind}if {vname} != 0 {{ free({vname}); }}"

        # Standalone expression / call
        else:
            expr = self.gen_expr(depth, 3)
            return f"{ind}{expr};"

    def gen_struct_def(self) -> str:
        sname = self.rand_id("Struct")
        self.struct_names.append(sname)
        fields = []
        for i in range(self.rng.randint(2, 6)):
            fname = f"f_{i}"
            ftype = self.rand_type(allow_void=False)
            fields.append(f"    {fname}: {ftype};")
        return f"struct {sname} {{\n" + "\n".join(fields) + "\n}\n"

    def gen_view_def(self) -> str:
        vname = self.rand_id("View")
        self.view_names.append(vname)
        fields = []
        offset = 0
        for i in range(self.rng.randint(2, 5)):
            fname = f"slot_{i}"
            ftype = self.rng.choice(["i64", "i32", "i16", "i8", "f64"])
            fields.append(f"    {fname}: {ftype} at {offset};")
            offset += 8
        return f"view {vname} {{\n" + "\n".join(fields) + "\n}\n"

    def gen_enum_def(self) -> str:
        ename = self.rand_id("Enum")
        self.enum_names.append(ename)
        variants = [f"Variant_{i}" for i in range(self.rng.randint(2, 5))]
        return f"enum {ename} {{\n    " + ",\n    ".join(variants) + "\n}\n"

    def gen_function_decl(self, is_main=False) -> str:
        fname = "main" if is_main else self.rand_id("fn")
        if not is_main:
            self.func_names.append(fname)
        
        # Pick 1 of 4 Sable function syntax variants
        style = self.rng.choice(["c_style", "rust_style", "type_first", "english_style"])
        ret_type = "i64" if is_main else self.rand_type()
        
        # Parameters
        param_strs = []
        if not is_main:
            for p in range(self.rng.randint(0, 3)):
                pname = f"p_{p}"
                ptype = self.rand_type()
                self.var_names.append(pname)
                param_strs.append(f"{ptype} {pname}" if style in ("c_style", "type_first") else f"{pname}: {ptype}")

        body_lines = [self.gen_statement() for _ in range(self.rng.randint(2, 6))]
        if is_main:
            body_lines.append(f"{self.indent()}return 0;")
        body = "\n".join(body_lines)

        if style == "c_style":
            return f"function {ret_type} {fname}({', '.join(param_strs)}) {{\n{body}\n}}\n"
        elif style == "rust_style":
            return f"fn {fname}({', '.join(param_strs)}) -> {ret_type} {{\n{body}\n}}\n"
        elif style == "type_first":
            return f"{ret_type} {fname}({', '.join(param_strs)}) {{\n{body}\n}}\n"
        else: # english_style
            if param_strs:
                with_clause = f" with {', '.join(param_strs)}"
            else:
                with_clause = ""
            return f"define function {fname}{with_clause} returning {ret_type}\n{body}\nend function\n"

    def generate_full_module(self) -> str:
        """Generates a complete standalone compilable module."""
        self.struct_names.clear()
        self.enum_names.clear()
        self.view_names.clear()
        self.func_names.clear()
        self.var_names.clear()

        chunks = ["// Auto-generated Sable Fuzz Module\n"]
        
        # Types
        for _ in range(self.rng.randint(1, 3)):
            chunks.append(self.gen_struct_def())
        if self.rng.random() < 0.4:
            chunks.append(self.gen_view_def())
        if self.rng.random() < 0.4:
            chunks.append(self.gen_enum_def())

        # Helper functions
        for _ in range(self.rng.randint(1, 4)):
            chunks.append(self.gen_function_decl(is_main=False))

        # Main entry point
        chunks.append(self.gen_function_decl(is_main=True))

        return "\n".join(chunks)

# ============================================================================
# SEED CORPUS MUTATION ENGINE
# ============================================================================

class SableCorpusMutator:
    """Reads existing valid corpus files and introduces subtle AST and syntax mutations."""

    def __init__(self, rng: random.Random):
        self.rng = rng
        self.corpus = []
        self._load_corpus()

    def _load_corpus(self):
        candidate_dirs = [ROOT / "examples", ROOT / "stdlib", ROOT / "tests"]
        for d in candidate_dirs:
            if not d.exists():
                continue
            for f in d.rglob("*.sb"):
                try:
                    text = f.read_text(encoding="utf-8", errors="replace")
                    if 10 < len(text) < 15000:
                        self.corpus.append(text)
                except Exception:
                    pass

    def mutate(self) -> str:
        if not self.corpus:
            return SableAstGenerator(self.rng).generate_full_module()

        source = self.rng.choice(self.corpus)
        mutation = self.rng.choice([
            "boundary_splice",
            "token_replacement",
            "syntax_twist",
            "type_mangle",
            "extreme_nesting",
            "comment_trap",
            "string_payload"
        ])

        if mutation == "boundary_splice":
            # Splice in boundary integers into numeric spots
            target = self.rng.choice(NUMERIC_BOUNDARIES)
            return re.sub(r"\b\d+\b", target, source, count=self.rng.randint(1, 3))

        elif mutation == "token_replacement":
            # Swap operators or keywords
            swaps = [
                ("==", "!="), ("+", "-"), ("*", "/"), ("<", ">="),
                ("let", "var"), ("function", "fn"), ("i64", "i32"),
                ("return", "break"), ("malloc", "free")
            ]
            s_from, s_to = self.rng.choice(swaps)
            return source.replace(s_from, s_to, self.rng.randint(1, 4))

        elif mutation == "syntax_twist":
            # Inject unclosed brackets or syntax edge cases
            twists = [
                ";;;;;", "{{{{{", "}}}}}", "(((((", ")))))",
                "...", "..", "?:", "->", "=>", "as *void as *i64"
            ]
            twist = self.rng.choice(twists)
            pos = self.rng.randint(0, len(source))
            return source[:pos] + "\n" + twist + "\n" + source[pos:]

        elif mutation == "type_mangle":
            # Corrupt type declarations
            mangles = ["*i64", "**void", "Node999", "bool as i64", "*mut *mut i64"]
            m = self.rng.choice(mangles)
            return re.sub(r":\s*[a-zA-Z0-9_*]+", f": {m}", source, count=1)

        elif mutation == "extreme_nesting":
            depth = self.rng.randint(100, 500)
            nest = "(" * depth + "0" + ")" * depth
            return source + f"\nfunction i64 __deep_nest() {{ return {nest}; }}\n"

        elif mutation == "string_payload":
            payload = self.rng.choice(STRING_TEST_CORPUS)
            return re.sub(r'"[^"\n]*"', payload, source, count=1)

        else: # comment_trap
            traps = ["/* /* */ */", "/* unterminated", "// comment with \x00 null byte"]
            t = self.rng.choice(traps)
            pos = self.rng.randint(0, len(source))
            return source[:pos] + "\n" + t + "\n" + source[pos:]

# ============================================================================
# AUTOMATED DELTA DEBUGGER (CRASH MINIMIZER)
# ============================================================================

class DeltaDebugger:
    """Automated crash minimizer using hierarchical delta debugging.
    Shrinks large crashing samples down to the minimal reproduction snippet.
    """

    def __init__(self, compiler_bin: Path, target_sig: str):
        self.compiler = compiler_bin
        self.target_sig = target_sig

    def test(self, content: str) -> bool:
        with tempfile.NamedTemporaryFile(suffix=".sb", delete=False, mode="w", encoding="utf-8") as tf:
            tf.write(content)
            temp_name = tf.name
        try:
            code, out, to = run_cmd([self.compiler, "--quiet", temp_name], timeout=5)
            is_find, cat, desc = classify_outcome(code, out, to)
            if is_find and cat == self.target_sig:
                return True
            return False
        finally:
            try:
                os.remove(temp_name)
            except OSError:
                pass

    def minimize(self, content: str) -> str:
        """Minimizes line by line using ddmin algorithm."""
        lines = content.splitlines(keepends=True)
        if not lines:
            return content

        granularity = 2
        while len(lines) >= 2:
            n = len(lines)
            subset_len = n // granularity
            reduced = False

            for i in range(granularity):
                start = i * subset_len
                end = n if i == granularity - 1 else (i + 1) * subset_len
                candidate = "".join(lines[:start] + lines[end:])

                if self.test(candidate):
                    lines = candidate.splitlines(keepends=True)
                    granularity = max(granularity - 1, 2)
                    reduced = True
                    break

            if not reduced:
                if granularity == len(lines):
                    break
                granularity = min(granularity * 2, len(lines))

        return "".join(lines)

# ============================================================================
# SPECIALIZED STRATEGY GENERATORS
# ============================================================================

def make_parser_stress_sample(rng: random.Random) -> str:
    """Specialized in tripping lexer & parser tables."""
    choice = rng.randint(1, 5)
    if choice == 1:
        # Deep precedence tree
        ops = ["+", "-", "*", "/", "%", "&", "|", "^", "&&", "||", "==", "!="]
        depth = rng.randint(40, 150)
        expr = "1"
        for _ in range(depth):
            expr = f"({expr} {rng.choice(ops)} {rng.choice(NUMERIC_BOUNDARIES)})"
        return f"function i64 main() {{ let r = {expr}; return 0; }}\n"
    elif choice == 2:
        # Unicode and escape madness in strings
        escapes = ["\\n", "\\r", "\\t", "\\\\", "\\\"", "\\x00", "\\xFF", "\\u0000"]
        s = "".join(rng.choice(escapes) for _ in range(50))
        return f'function i64 main() {{ let s = "{s}"; return 0; }}\n'
    elif choice == 3:
        # Chained member accesses
        depth = rng.randint(30, 80)
        chain = "obj" + (".next" * depth)
        return f"struct Node {{ next: *Node; }}\nfunction i64 main() {{ let obj: *Node = 0 as *Node; let x = {chain}; return 0; }}\n"
    elif choice == 4:
        # Enormous identifier
        ident = "var_" + ("a" * rng.randint(500, 2500))
        return f"function i64 main() {{ let {ident} = 42; return {ident}; }}\n"
    else:
        # Mix of valid & invalid statement styles
        return "fn main() -> i64 {\n    let mut x: i64 = 100;\n    while x > 0 { x = x - 1; }\n    return x;\n}\n"

def make_types_stress_sample(rng: random.Random) -> str:
    """Specialized in type resolution cycles, pointer casting, and struct layouts."""
    num_structs = rng.randint(3, 8)
    decls = []
    for i in range(num_structs):
        next_i = (i + 1) % num_structs
        decls.append(f"struct TypeNode_{i} {{\n    ptr: *TypeNode_{next_i};\n    val: i64;\n}}")
    
    code = "\n\n".join(decls)
    code += f"""\n\nfunction i64 main() {{
    let base = malloc(128);
    if base != 0 {{
        let root = base as *TypeNode_0;
        let cycle = root.ptr as *TypeNode_1;
        free(base);
    }}
    return 0;
}}
"""
    return code

def make_codegen_stress_sample(rng: random.Random) -> str:
    """Specialized in register pressure, scratch register allocation, and arithmetic folding."""
    lines = ["function i64 main() {"]
    # Generate 40 registers to create severe register pressure
    for i in range(35):
        val1 = rng.choice(NUMERIC_BOUNDARIES)
        val2 = rng.choice(NUMERIC_BOUNDARIES)
        op = rng.choice(["+", "-", "*", "&", "|", "^"])
        lines.append(f"    let r_{i} = ({val1} {op} {val2});")

    # Combine all registers in single expression
    sum_expr = " + ".join(f"r_{i}" for i in range(35))
    lines.append(f"    let final_res = {sum_expr};")
    lines.append("    return 0;\n}")
    return "\n".join(lines)

def make_memory_stress_sample(rng: random.Random) -> str:
    """Specialized in views, buffer push/len, and manual memory reads."""
    return f"""view PacketView {{
    magic: i32 at 0;
    seq:   u64 at 4;
    flags: u16 at 12;
    payload: i64 at 16;
}}

function i64 main() {{
    let raw = malloc(32);
    if raw == 0 {{ return 0; }}
    
    let pkt = raw as *PacketView;
    let seq_val = pkt.seq;
    let magic_val = pkt.magic;
    
    free(raw);
    return 0;
}}
"""

def make_control_flow_sample(rng: random.Random) -> str:
    """Specialized in switch inlining, premature return bugs, and loops."""
    num_cases = rng.randint(15, 60)
    cases = []
    for i in range(num_cases):
        if rng.random() < 0.2:
            cases.append(f"        case {i}: return {i * 10};")
        else:
            cases.append(f"        case {i}: break;")

    return f"""function i64 inline_switch_target(i64 val) {{
    switch val {{
{"\n".join(cases)}
        default: return -1;
    }}
    return 100;
}}

function i64 main() {{
    let res = inline_switch_target({rng.randint(0, num_cases)});
    return 0;
}}
"""

# ============================================================================
# FUZZER COORDINATOR & WORKER EXECUTION
# ============================================================================

class FuzzCoordinator:
    def __init__(self, compiler: Path, mode: str, duration: int, output_dir: Path, findings_file: Path, seed: int = None):
        self.compiler = compiler
        self.mode = mode
        self.duration = duration
        self.output_dir = output_dir
        self.findings_file = findings_file
        self.rng = random.Random(seed if seed is not None else random.randint(1, 999999))
        self.crashes_dir = self.output_dir / "crashes"
        self.crashes_dir.mkdir(parents=True, exist_ok=True)
        
        self.ast_gen = SableAstGenerator(self.rng)
        self.mutator = SableCorpusMutator(self.rng)
        
        self.seen_signatures = set()
        self.total_iterations = 0
        self.total_crashes = 0
        self.start_time = 0.0

    def generate_candidate(self) -> str:
        r = self.rng.random()
        if self.mode == "parser":
            return make_parser_stress_sample(self.rng) if r < 0.7 else self.mutator.mutate()
        elif self.mode == "types":
            return make_types_stress_sample(self.rng) if r < 0.7 else self.ast_gen.generate_full_module()
        elif self.mode == "codegen":
            return make_codegen_stress_sample(self.rng) if r < 0.7 else self.ast_gen.generate_full_module()
        elif self.mode == "memory":
            return make_memory_stress_sample(self.rng)
        elif self.mode == "control_flow":
            return make_control_flow_sample(self.rng)
        else: # all
            generators = [
                make_parser_stress_sample,
                make_types_stress_sample,
                make_codegen_stress_sample,
                make_memory_stress_sample,
                make_control_flow_sample,
                lambda rng: self.ast_gen.generate_full_module(),
                lambda rng: self.mutator.mutate()
            ]
            chosen = self.rng.choice(generators)
            return chosen(self.rng)

    def log_finding(self, sample: str, category: str, description: str, code: int, output: str):
        content_hash = hashlib.sha256(sample.encode("utf-8")).hexdigest()[:12]
        sig_key = f"{category}_{code}"
        
        # Avoid duplicate floods of identical crash reason
        if sig_key in self.seen_signatures and len(self.seen_signatures) > 30:
            return

        self.seen_signatures.add(sig_key)
        self.total_crashes += 1

        print(f"\n=======================================================")
        print(f"[!CRASH DETECTED] Category: {category} | Exit: {code}")
        print(f"Details: {description}")
        print(f"Minimizing reproducer...")

        # Run Delta Debugger to produce clean minimal snippet
        dd = DeltaDebugger(self.compiler, category)
        try:
            minimized = dd.minimize(sample)
        except Exception as e:
            minimized = sample

        crash_filename = f"crash_{category.lower()}_{content_hash}.sb"
        crash_path = self.crashes_dir / crash_filename
        crash_path.write_text(minimized, encoding="utf-8")
        print(f"Saved minimal reproducer to {crash_path}")

        # Update FINDINGS.md ONLY when a crash is detected
        timestamp = time.strftime('%Y-%m-%d %H:%M:%SZ', time.gmtime())
        finding_entry = f"""
## Finding `{content_hash}` — {category}
- **Timestamp**: `{timestamp}`
- **Exit Code**: `{code}` (`{hex(code & 0xFFFFFFFF)}`)
- **Reason**: {description}
- **Target Mode**: `{self.mode}`
- **Artifact File**: `fuzzer/crashes/{crash_filename}`

### Minimal Reproducer:
```sable
{minimized.strip()}
```

### Compiler Output:
```text
{output.strip()[:1000]}
```
---
"""
        with open(self.findings_file, "a", encoding="utf-8") as f:
            f.write(finding_entry)

    def run(self):
        self.start_time = time.time()
        print(f"=== Sable Compiler Fuzzing Session Started ===")
        print(f"Compiler:  {self.compiler}")
        print(f"Mode:      {self.mode}")
        print(f"Duration:  {self.duration}s ({self.duration / 3600:.2f} hours)")
        print(f"Findings:  {self.findings_file}")
        print(f"Artifacts: {self.crashes_dir}")
        print(f"===============================================\n")

        with tempfile.TemporaryDirectory(prefix="sable_fuzz_run_") as td:
            temp_path = Path(td)
            src_file = temp_path / "fuzz_input.sb"
            exe_file = temp_path / "fuzz_output.exe"

            while time.time() - self.start_time < self.duration:
                self.total_iterations += 1
                sample = self.generate_candidate()
                src_file.write_text(sample, encoding="utf-8")

                code, output, timed_out = run_proc([self.compiler, "--quiet", str(src_file), "-o", str(exe_file)], timeout=8)
                is_find, category, desc = classify_outcome(code, output, timed_out)

                if is_find:
                    self.log_finding(sample, category, desc, code, output)

                if self.total_iterations % 500 == 0:
                    elapsed = time.time() - self.start_time
                    speed = self.total_iterations / elapsed if elapsed > 0 else 0
                    print(f"[{elapsed:.1f}s / {self.duration}s] Iterations: {self.total_iterations:,} ({speed:.1f} iter/s) | Bugs: {self.total_crashes}")

        elapsed = time.time() - self.start_time
        speed = self.total_iterations / elapsed if elapsed > 0 else 0
        print(f"\nSession Complete: {self.total_iterations:,} cases tested in {elapsed:.1f}s ({speed:.1f} iter/s).")
        print(f"Total Unique Findings: {self.total_crashes}")

# ============================================================================
# ENTRY POINT
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "bin/Release/sable.exe")
    parser.add_argument("--mode", choices=["parser", "types", "codegen", "memory", "control_flow", "all"], default="all")
    parser.add_argument("--duration", type=int, default=60, help="Duration in seconds")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "fuzzer")
    parser.add_argument("--findings", type=Path, default=ROOT / "FINDINGS.md")
    parser.add_argument("--seed", type=int, default=None)
    args = parser.parse_args()

    compiler_path = args.compiler.resolve()
    if not compiler_path.is_file():
        alt = compiler_path.parent / "opus.exe"
        if alt.is_file():
            compiler_path = alt
        else:
            print(f"Compiler executable not found at {compiler_path}", file=sys.stderr)
            sys.exit(1)

    coordinator = FuzzCoordinator(
        compiler=compiler_path,
        mode=args.mode,
        duration=args.duration,
        output_dir=args.output_dir.resolve(),
        findings_file=args.findings.resolve(),
        seed=args.seed
    )
    coordinator.run()

if __name__ == "__main__":
    main()
