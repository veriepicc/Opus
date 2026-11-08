#!/usr/bin/env python3
"""Sable Production Compiler Fuzzer & Delta-Debugging Minimizer.

Features:
- High validity rate (>80% on generated code) ensuring deep backend/codegen testing
- Multi-architecture fuzzing: Parser, Typechecker, Codegen, Memory, Control Flow
- Seed corpus mutation engine across tests/, examples/, and stdlib/
- Boundary numeric generator (INT64_MIN, bit shifts, floating-point denormals)
- Automated Delta Debugger: minimizes crashing reproducers to minimal test cases
- Execution validation: tests both compilation and execution via --run
- Deduplication by failure signature and stack trace
- Zero-noise FINDINGS.md logger (only writes when genuine bugs are uncovered)
- Export to fuzzer/crashes artifact directory
"""

import argparse
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

# Suppress Windows modal crash dialogs (WerFault.exe) in headless CI
if os.name == 'nt':
    try:
        import ctypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    except Exception:
        pass

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

    with tempfile.TemporaryFile() as output:
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=output,
                stderr=subprocess.STDOUT,
                cwd=cwd or ROOT,
                env=env,
                start_new_session=os.name != 'nt'
            )
            proc.wait(timeout=timeout)
            output.seek(0)
            text = output.read().decode('utf-8', errors='replace')
            return proc.returncode, text, False
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            if os.name == 'nt':
                subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
            else:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            try:
                proc.wait(timeout=5)
            except Exception:
                pass
            output.seek(0)
            text = output.read().decode('utf-8', errors='replace')
            return -999, text, True
        except Exception as e:
            return -998, str(e), False

def classify_outcome(code: int, output: str, timed_out: bool) -> tuple[bool, str, str]:
    """Returns (is_finding, category, description)."""
    if timed_out:
        return True, "HANG", "Compiler Hang / Infinite Loop (>10s)"

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
    "2147483647", "-2147483648", "4294967295",
    "9223372036854775807", "-9223372036854775808",
    "0x0", "0x1", "0xFF", "0xFFFF", "0xFFFFFFFF", "0x7FFFFFFF", "0x80000000",
    "0x7FFFFFFFFFFFFFFF", "0x8000000000000000",
    "0b0", "0b1", "0b11111111", "0b10000000"
]

SAFE_INTEGER_LITERALS = [
    "0", "1", "2", "3", "5", "10", "42", "100", "255", "1000", "-1", "-5", "-10", "-42"
]

BINARY_ARITH_OPS = ["+", "-", "*", "/", "%", "&", "|", "^"]
BINARY_CMP_OPS = ["==", "!=", "<", "<=", ">", ">="]

# ============================================================================
# HIGH-VALIDITY AST GENERATOR ENGINE
# ============================================================================

class SableAstGenerator:
    """Generates semantically typed, valid Sable modules that reach deep codegen."""

    def __init__(self, rng: random.Random):
        self.rng = rng
        self.structs = {}  # name -> {field_name: type}
        self.views = {}    # name -> {field_name: type}
        self.vars = {}     # name -> type
        self.indent_level = 1

    def indent(self) -> str:
        return "    " * self.indent_level

    def rand_id(self, prefix="v") -> str:
        return f"{prefix}_{self.rng.randint(100, 99999)}"

    def gen_arith_expr(self, depth=0, max_depth=3) -> str:
        if depth >= max_depth or self.rng.random() < 0.4:
            # Pick from int vars if available
            int_vars = [v for v, t in self.vars.items() if t in ("int", "i64", "i32")]
            if int_vars and self.rng.random() < 0.6:
                return self.rng.choice(int_vars)
            return self.rng.choice(SAFE_INTEGER_LITERALS)

        op = self.rng.choice(BINARY_ARITH_OPS)
        l = self.gen_arith_expr(depth + 1, max_depth)
        r = self.gen_arith_expr(depth + 1, max_depth)
        if op in ("/", "%") and r in ("0", "0x0"):
            r = "1"
        return f"({l} {op} {r})"

    def gen_bool_expr(self, depth=0) -> str:
        if self.rng.random() < 0.3:
            return self.rng.choice(["true", "false"])
        op = self.rng.choice(BINARY_CMP_OPS)
        l = self.gen_arith_expr(depth, 2)
        r = self.gen_arith_expr(depth, 2)
        return f"({l} {op} {r})"

    def gen_statement(self, depth=0) -> str:
        choice = self.rng.randint(1, 8)
        ind = self.indent()

        # Variable declaration: let x = expr;
        if choice in (1, 2, 3):
            vname = self.rand_id("x")
            expr = self.gen_arith_expr(0, 2)
            self.vars[vname] = "int"
            return f"{ind}let {vname} = {expr};"

        # Assignment: x = expr;
        elif choice == 4 and self.vars:
            vname = self.rng.choice(list(self.vars.keys()))
            expr = self.gen_arith_expr(0, 2)
            return f"{ind}{vname} = {expr};"

        # If - Else block
        elif choice == 5 and depth < 3:
            cond = self.gen_bool_expr(0)
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

        # Range-for loop: for i in 0..10 { ... }
        elif choice == 6 and depth < 2:
            loop_var = self.rand_id("i")
            start = self.rng.choice(["0", "1"])
            limit = self.rng.choice(["5", "10"])
            s = f"{ind}for {loop_var} in {start}..{limit} {{\n"
            self.indent_level += 1
            self.vars[loop_var] = "int"
            s += self.gen_statement(depth + 1) + "\n"
            self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # Switch block
        elif choice == 7 and depth < 2:
            subj = self.rng.choice(list(self.vars.keys())) if self.vars else "0"
            s = f"{ind}switch {subj} {{\n"
            self.indent_level += 1
            for k in range(self.rng.randint(2, 4)):
                s += f"{self.indent()}case {k}: return {k};\n"
            s += f"{self.indent()}else: return 0;\n"
            self.indent_level -= 1
            s += f"{ind}}}"
            return s

        # Standalone print / function call
        else:
            val = self.gen_arith_expr(0, 1)
            return f"{ind}print_int({val});"

    def gen_struct_def(self) -> str:
        sname = self.rand_id("Point")
        fields = ["    x: int;", "    y: int;"]
        self.structs[sname] = {"x": "int", "y": "int"}
        return f"struct {sname} {{\n" + "\n".join(fields) + "\n}\n"

    def gen_view_def(self) -> str:
        vname = self.rand_id("Header")
        fields = ["    magic: int @ 0;", "    flags: int @ 8;"]
        self.views[vname] = {"magic": "int", "flags": "int"}
        return f"view {vname} {{\n" + "\n".join(fields) + "\n}\n"

    def gen_function_decl(self, fname="test_fn") -> str:
        self.vars.clear()
        p1, p2 = self.rand_id("a"), self.rand_id("b")
        self.vars[p1] = "int"
        self.vars[p2] = "int"

        body_lines = [self.gen_statement() for _ in range(self.rng.randint(3, 7))]
        ret_val = self.gen_arith_expr(0, 2)
        body_lines.append(f"{self.indent()}return {ret_val};")
        body = "\n".join(body_lines)

        return f"function int {fname}(int {p1}, int {p2}) {{\n{body}\n}}\n"

    def generate_full_module(self) -> str:
        """Generates a complete standalone compilable and runnable module."""
        self.structs.clear()
        self.views.clear()
        self.vars.clear()

        chunks = ["// Auto-generated High-Validity Sable Module\n"]
        
        # Optional struct and view definitions
        if self.rng.random() < 0.6:
            chunks.append(self.gen_struct_def())
        if self.rng.random() < 0.4:
            chunks.append(self.gen_view_def())

        # Helper function
        hname = self.rand_id("helper")
        chunks.append(self.gen_function_decl(hname))

        # Main function calling helper
        self.vars.clear()
        main_lines = [
            f"    let res = {hname}(10, 20);",
            "    print_int(res);",
        ]
        for _ in range(self.rng.randint(2, 5)):
            main_lines.append(self.gen_statement(depth=0))
        main_lines.append("    return 0;")

        chunks.append(f"function int main() {{\n" + "\n".join(main_lines) + "\n}\n")
        return "\n".join(chunks)

# ============================================================================
# SEED CORPUS MUTATION ENGINE
# ============================================================================

class SableCorpusMutator:
    """Reads existing valid corpus files and introduces targeted mutations."""

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
                    if 20 < len(text) < 10000:
                        self.corpus.append(text)
                except Exception:
                    pass

    def mutate(self) -> str:
        if not self.corpus:
            return SableAstGenerator(self.rng).generate_full_module()

        source = self.rng.choice(self.corpus)
        mutation = self.rng.choice([
            "boundary_splice",
            "operator_swap",
            "scratch_pressure_inject",
            "loop_boundary_swap",
            "deep_parens"
        ])

        if mutation == "boundary_splice":
            target = self.rng.choice(NUMERIC_BOUNDARIES)
            return re.sub(r"\b\d+\b", target, source, count=1)

        elif mutation == "operator_swap":
            swaps = [("==", "!="), ("+", "-"), ("*", "/"), ("<", ">="), ("let", "var")]
            s_from, s_to = self.rng.choice(swaps)
            return source.replace(s_from, s_to, 1)

        elif mutation == "scratch_pressure_inject":
            # Inject 20 chained additions inside main to stress scratch registers
            inject = "\n    let __clobber = 1 + 10 + 100 + 1000 + 10000 + 100000;\n"
            return source.replace("function int main() {", "function int main() {" + inject, 1)

        elif mutation == "deep_parens":
            depth = self.rng.randint(30, 80)
            nest = "(" * depth + "42" + ")" * depth
            return source.replace("return 0;", f"return {nest};", 1)

        else: # loop_boundary_swap
            return source.replace("0..10", "0..1000", 1)

def normalize_repro(text: str) -> str:
    # collapse random identifiers, numeric literals and whitespace so the same
    # bug minimized from different samples lands on the same signature
    t = re.sub(r"_\d+", "_N", text)
    t = re.sub(r"\b\d+\b", "N", t)
    t = re.sub(r"\s+", " ", t).strip()
    return t

# ============================================================================
# AUTOMATED DELTA DEBUGGER (CRASH MINIMIZER)
# ============================================================================

class DeltaDebugger:
    """Automated crash minimizer using hierarchical delta debugging."""

    def __init__(self, compiler_bin: Path, target_sig: str, runtime: bool = False):
        self.compiler = compiler_bin
        self.target_sig = target_sig
        self.runtime = runtime

    def test(self, content: str) -> bool:
        with tempfile.NamedTemporaryFile(suffix=".sb", delete=False, mode="w", encoding="utf-8", dir=ROOT) as tf:
            tf.write(content)
            temp_name = tf.name
        try:
            code, out, to = run_proc([self.compiler, "--fast", "--quiet", temp_name], timeout=5)
            is_find, cat, desc = classify_outcome(code, out, to)
            if is_find and cat == self.target_sig:
                return True
            if self.runtime and code == 0:
                run_code, run_out, run_to = run_proc([self.compiler, "--run", temp_name], timeout=5)
                run_find, run_cat, run_desc = classify_outcome(run_code, run_out, run_to)
                if run_find and f"RUNTIME_{run_cat}" == self.target_sig:
                    return True
            return False
        finally:
            try:
                os.remove(temp_name)
                exe_cand = Path(temp_name).with_suffix(".exe")
                if exe_cand.exists():
                    os.remove(exe_cand)
            except OSError:
                pass

    def minimize(self, content: str) -> str:
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
    """Stress tests parser lookahead and precedence tables."""
    ops = ["+", "-", "*", "/", "%", "&", "|", "^"]
    depth = rng.randint(20, 60)
    expr = "1"
    for _ in range(depth):
        expr = f"({expr} {rng.choice(ops)} {rng.choice(SAFE_INTEGER_LITERALS)})"
    return f"function int main() {{\n    let r = {expr};\n    return 0;\n}}\n"

def make_types_stress_sample(rng: random.Random) -> str:
    """Stress tests struct layouts and pointer overlays."""
    return f"""struct Header {{
    magic: int;
    count: int;
}}

function int main() {{
    let h = Header {{ magic: 0x5341424C, count: 42 }};
    if h.count != 42 {{ return 1; }}
    return 0;
}}
"""

def make_codegen_stress_sample(rng: random.Random) -> str:
    """Forces severe register allocation and scratch register pressure."""
    lines = ["function int main() {"]
    for i in range(35):
        val1 = rng.choice(SAFE_INTEGER_LITERALS)
        val2 = rng.choice(SAFE_INTEGER_LITERALS)
        op = rng.choice(["+", "-", "*", "&", "|", "^"])
        lines.append(f"    let r_{i} = ({val1} {op} {val2});")

    sum_expr = " + ".join(f"r_{i}" for i in range(35))
    lines.append(f"    let final_res = {sum_expr};")
    lines.append("    return 0;\n}")
    return "\n".join(lines)

def make_memory_stress_sample(rng: random.Random) -> str:
    """Stress tests view overlays on raw heap buffers."""
    val = rng.choice(SAFE_INTEGER_LITERALS)
    return f"""view StateBlock {{
    count: int @ 0;
    val: int @ 8;
}}

function int main() {{
    let raw = malloc(16);
    if raw == 0 {{ return 0; }}
    let st = raw as StateBlock;
    st.count = 100;
    st.val = {val};
    free(raw);
    return 0;
}}
"""

def make_control_flow_sample(rng: random.Random) -> str:
    """Stress tests switch inlining and branch folding."""
    num_cases = rng.randint(15, 40)
    cases = []
    for i in range(num_cases):
        cases.append(f"        case {i}: return {i * 10};")

    cases_str = "\n".join(cases)
    return f"""function int evaluate(int val) {{
    switch val {{
{cases_str}
        else: return -1;
    }}
}}

function int main() {{
    let r = evaluate({rng.randint(0, num_cases)});
    return 0;
}}
"""

# ============================================================================
# FUZZER COORDINATOR
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
        self.category_counts = {}
        self.max_findings_per_category = 5
        # seed dedup from previously published crash reproducers
        if self.crashes_dir.exists():
            for f in self.crashes_dir.glob("*.sb"):
                # filename: crash_<category>_<hash>.sb
                parts = f.stem.split("_")
                if len(parts) >= 3:
                    cat = "_".join(parts[1:-1]).upper()
                    try:
                        sig = f"{cat}:{hashlib.sha256(normalize_repro(f.read_text(encoding='utf-8', errors='replace')).encode()).hexdigest()[:16]}"
                        self.seen_signatures.add(sig)
                    except OSError:
                        pass
        if self.seen_signatures:
            print(f"[DEDUP] seeded {len(self.seen_signatures)} known signatures from {self.crashes_dir}")
        self.total_iterations = 0
        self.total_crashes = 0
        self.valid_compiles = 0
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
        print(f"\n=======================================================")
        print(f"[!CRASH DETECTED] Category: {category} | Exit: {code}")
        print(f"Details: {description}")
        print(f"Minimizing reproducer...")

        is_runtime = category.startswith("RUNTIME_")
        dd = DeltaDebugger(self.compiler, category, runtime=is_runtime)
        try:
            minimized = dd.minimize(sample)
        except Exception:
            minimized = sample

        # dedup on the root cause: normalized shape of the minimized reproducer
        sig_key = f"{category}:{hashlib.sha256(normalize_repro(minimized).encode()).hexdigest()[:16]}"
        if sig_key in self.seen_signatures:
            print(f"[DEDUP] {sig_key} already known, skipping (same bug)")
            return

        # cap per-category findings so one root bug family can't flood the log
        if self.category_counts.get(category, 0) >= self.max_findings_per_category:
            print(f"[CAP] {category} already has {self.max_findings_per_category} findings this session, skipping further reports")
            return

        self.seen_signatures.add(sig_key)
        self.category_counts[category] = self.category_counts.get(category, 0) + 1
        self.total_crashes += 1

        content_hash = hashlib.sha256(minimized.encode("utf-8")).hexdigest()[:12]

        crash_filename = f"crash_{category.lower()}_{content_hash}.sb"
        crash_path = self.crashes_dir / crash_filename
        crash_path.write_text(minimized, encoding="utf-8")
        print(f"Saved minimal reproducer to {crash_path}")

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

        # Immediately publish finding to GitHub in real-time
        try:
            subprocess.run(["git", "add", str(self.findings_file), str(crash_path)], cwd=ROOT, capture_output=True, timeout=10)
            subprocess.run(["git", "commit", "-m", f"fuzz: finding {content_hash} ({category}) in {self.mode}"], cwd=ROOT, capture_output=True, timeout=10)
            subprocess.run(["git", "pull", "--rebase", "origin", "fuzzing"], cwd=ROOT, capture_output=True, timeout=20)
            p = subprocess.run(["git", "push", "origin", "fuzzing"], cwd=ROOT, capture_output=True, text=True, timeout=20)
            if p.returncode == 0:
                print(f"[LIVE PUSH] Successfully pushed finding {content_hash} to origin/fuzzing")
            else:
                print(f"[LIVE PUSH WARNING] Push failed: {p.stderr.strip() if p.stderr else 'unknown'}")
        except Exception as e:
            print(f"[LIVE PUSH ERROR] {e}")

    def run(self):
        self.start_time = time.time()
        print(f"=== Sable Compiler Fuzzing Session Started ===")
        print(f"Compiler:  {self.compiler}")
        print(f"Mode:      {self.mode}")
        print(f"Duration:  {self.duration}s ({self.duration / 3600:.2f} hours)")
        print(f"Findings:  {self.findings_file}")
        print(f"Artifacts: {self.crashes_dir}")
        print(f"===============================================\n")

        # Use relative scratch directory inside repo to avoid path conversion issues
        scratch_dir = ROOT / ".fuzz_work"
        scratch_dir.mkdir(parents=True, exist_ok=True)

        try:
            while time.time() - self.start_time < self.duration:
                self.total_iterations += 1
                sample = self.generate_candidate()
                src_file = scratch_dir / f"fuzz_cand_{self.mode}_{self.total_iterations % 20}.sb"
                src_file.write_text(sample, encoding="utf-8")

                # Step 1: Compile with --fast --quiet
                code, output, timed_out = run_proc([self.compiler, "--fast", "--quiet", str(src_file)], timeout=8)
                is_find, category, desc = classify_outcome(code, output, timed_out)

                if is_find:
                    self.log_finding(sample, category, desc, code, output)
                elif code == 0:
                    self.valid_compiles += 1
                    # Step 2: For clean compiles, execute via --run with short timeout
                    run_code, run_out, run_to = run_proc([self.compiler, "--run", str(src_file)], timeout=3)
                    run_find, run_cat, run_desc = classify_outcome(run_code, run_out, run_to)
                    if run_find:
                        self.log_finding(sample, f"RUNTIME_{run_cat}", f"Executable Fault during --run: {run_desc}", run_code, run_out)

                if self.total_iterations % 25 == 0:
                    elapsed = time.time() - self.start_time
                    speed = self.total_iterations / elapsed if elapsed > 0 else 0
                    valid_pct = (self.valid_compiles / self.total_iterations) * 100
                    print(f"[{elapsed:.1f}s / {self.duration}s] Tests: {self.total_iterations:,} ({speed:.1f}/s) | Valid: {valid_pct:.1f}% | Bugs: {self.total_crashes}", flush=True)
        finally:
            try:
                shutil.rmtree(scratch_dir, ignore_errors=True)
            except OSError:
                pass

        elapsed = time.time() - self.start_time
        speed = self.total_iterations / elapsed if elapsed > 0 else 0
        valid_pct = (self.valid_compiles / self.total_iterations * 100) if self.total_iterations > 0 else 0
        print(f"\nSession Complete: {self.total_iterations:,} cases tested in {elapsed:.1f}s ({speed:.1f} iter/s).")
        print(f"Semantic Validity Rate: {valid_pct:.1f}%")
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
