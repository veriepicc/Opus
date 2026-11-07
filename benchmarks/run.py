#!/usr/bin/env python3
"""Sable Benchmark Showdown Runner
Compares Sable against MSVC (/O2), GCC (-O3), and TCC across scalar math,
branching, and trading engine workloads.

Measures:
1. Compile latency (ms)
2. Binary footprint (bytes)
3. Runtime execution latency (ms)

Auto-generates BENCHMARKS.md.
"""

import sys
import os
import time
import shutil
import platform
import argparse
import json
import subprocess
import zipfile
import urllib.request
import tempfile
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
BENCH_DIR = ROOT_DIR / "benchmarks"
SOURCES_DIR = BENCH_DIR / "sources"
SCALAR_C = SOURCES_DIR / "scalar.c"
OUT_DIR = BENCH_DIR / ".work"

BENCHMARKS = [
    {
        "id": "math",
        "name": "Math Loop (80M iter)",
        "desc": "Power-of-2 shifts, adds, and induction operations",
        "sable_file": ROOT_DIR / "tests" / "benchmark_math.sb",
        "c_macro": 1,
    },
    {
        "id": "state",
        "name": "State Accumulation (50M iter)",
        "desc": "Multiple state updates with scalar induction",
        "sable_file": ROOT_DIR / "tests" / "benchmark_state.sb",
        "c_macro": 2,
    },
    {
        "id": "branch",
        "name": "Branch Parity (40M iter)",
        "desc": "Alternating parity branch recurrence",
        "sable_file": ROOT_DIR / "tests" / "benchmark_branch.sb",
        "c_macro": 3,
    },
    {
        "id": "general",
        "name": "General Logic (6M iter)",
        "desc": "Control flow folding, conditionals, and math blend",
        "sable_file": ROOT_DIR / "tests" / "benchmark_general.sb",
        "c_macro": 4,
    },
    {
        "id": "hft",
        "name": "HFT Order Book (12M iter)",
        "desc": "L2 order book matching, inventory skew, and risk checksums",
        "sable_file": ROOT_DIR / "tests" / "benchmark_hft.sb",
        "c_macro": 5,
    },
]

def ensure_tcc(work_dir):
    tcc_bin = work_dir / "tcc" / "tcc.exe"
    if tcc_bin.exists():
        return tcc_bin
    
    zip_path = work_dir / "tcc-win64.zip"
    url = "https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip"
    try:
        urllib.request.urlretrieve(url, zip_path)
        with zipfile.ZipFile(zip_path, "r") as z:
            z.extractall(work_dir)
        if tcc_bin.exists():
            return tcc_bin
    except Exception as e:
        print(f"note: tcc not available: {e}", file=sys.stderr)
    return None

def find_compiler(name):
    return shutil.which(name)

def find_sable_bin():
    candidates = [
        ROOT_DIR / "bin" / "Release" / "sable.exe",
        ROOT_DIR / "bin" / "Release" / "Sable.exe",
        ROOT_DIR / "sable.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    return None

def run_proc(cmd, cwd=None, env=None):
    if env is None:
        env = os.environ.copy()
    env["WINEDEBUG"] = "-all"
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, env=env, timeout=120)

def execute_binary(exe_path, runs=4, warmup=1):
    is_windows = sys.platform.startswith("win")
    cmd = [str(exe_path)] if is_windows else ["wine", str(exe_path)]

    exits = []
    for _ in range(warmup):
        proc = run_proc(cmd)
        check_execution(proc)
        exits.append(proc.returncode)

    times = []
    exit_code = 0
    for _ in range(runs):
        t0 = time.perf_counter()
        p = run_proc(cmd)
        check_execution(p)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)
        exit_code = p.returncode
        exits.append(exit_code)

    if len(set(exits)) != 1:
        raise RuntimeError(f"inconsistent benchmark results: {exits}")

    times.sort()
    return {
        "min_ms": times[0],
        "med_ms": times[len(times) // 2],
        "avg_ms": sum(times) / len(times),
        "exit_code": exit_code,
    }


def check_execution(proc):
    if (proc.returncode < 0 or (proc.returncode & 0xffffffff) in
            (0xc0000005, 0xc00000fd, 0xc0000409, 0xc000001d, 0xc0000135, 0xc0000139)
            or 'wine: unhandled' in (proc.stdout + proc.stderr).lower()):
        raise RuntimeError(f'benchmark crashed ({proc.returncode})\n{proc.stdout}{proc.stderr}')

def main():
    parser = argparse.ArgumentParser(description="Sable Benchmark Showdown")
    parser.add_argument("--iterations", "-n", type=int, default=4, help="Measured iterations per benchmark")
    parser.add_argument("--warmup", "-w", type=int, default=1, help="Warmup iterations")
    parser.add_argument("--skip-update-md", action="store_true", help="Do not overwrite BENCHMARKS.md")
    parser.add_argument("--require-toolchains", nargs="*", default=[])
    args = parser.parse_args()
    if args.iterations < 1 or args.warmup < 0:
        parser.error("iterations must be positive and warmup must be nonnegative")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    is_windows = sys.platform.startswith("win")

    sable_bin = find_sable_bin()
    if not sable_bin:
        print("error: sable.exe not found", file=sys.stderr)
        sys.exit(1)

    compilers = {"Sable": {"type": "sable", "bin": sable_bin}}

    gcc_name = "gcc" if is_windows else "x86_64-w64-mingw32-gcc"
    gcc_bin = find_compiler(gcc_name) or find_compiler("gcc")
    if gcc_bin:
        compilers["GCC"] = {"type": "gcc", "bin": gcc_bin}

    cl_bin = find_compiler("cl.exe") or find_compiler("cl")
    if cl_bin:
        compilers["MSVC"] = {"type": "msvc", "bin": cl_bin}

    tcc_bin = ensure_tcc(OUT_DIR)
    if tcc_bin:
        compilers["TCC"] = {"type": "tcc", "bin": tcc_bin}

    print(f"Toolchains detected: {', '.join(compilers.keys())}")
    failures = [f"Missing toolchain: {name}" for name in args.require_toolchains if name not in compilers]

    all_results = {}

    for bench in BENCHMARKS:
        bench_id = bench["id"]
        print(f"Running {bench['name']}...")
        all_results[bench_id] = {
            "meta": bench,
            "results": {},
        }

        for c_name, c_info in compilers.items():
            build_dir = Path(tempfile.mkdtemp(prefix=f"{bench_id}_{c_name.lower()}_", dir=OUT_DIR))
            exe_out = build_dir / "benchmark.exe"

            t0 = time.perf_counter()
            if c_info["type"] == "sable":
                source = build_dir / "benchmark.sb"
                shutil.copy2(bench["sable_file"], source)
                cmd = [str(sable_bin), "--fast", str(source)]
                if not is_windows:
                    cmd = ["wine"] + cmd

            elif c_info["type"] == "gcc":
                cmd = [str(c_info["bin"]), "-O3", "-fwrapv", "-s", f"-DBENCH={bench['c_macro']}", str(SCALAR_C), "-o", str(exe_out)]

            elif c_info["type"] == "tcc":
                cmd = [str(c_info["bin"]), f"-DBENCH={bench['c_macro']}", str(SCALAR_C), "-o", str(exe_out)]
                if not is_windows:
                    cmd = ["wine"] + cmd

            elif c_info["type"] == "msvc":
                cmd = [
                    str(c_info["bin"]), "/nologo", "/O2", "/GS-", "/Ob2", "/Oi", "/Ot",
                    f"/DBENCH={bench['c_macro']}", str(SCALAR_C),
                    f"/Fe:{exe_out}", f"/Fo:{build_dir / 'benchmark.obj'}"
                ]

            try:
                built = run_proc(cmd, cwd=build_dir)
            except (OSError, subprocess.TimeoutExpired) as error:
                failures.append(f"{bench_id}/{c_name}: {error}")
                continue

            compile_time_ms = (time.perf_counter() - t0) * 1000

            if built.returncode != 0 or not exe_out.exists():
                failures.append(f"{bench_id}/{c_name}: build failed ({built.returncode})")
                print(built.stdout + built.stderr)
                continue

            binary_size = exe_out.stat().st_size
            try:
                metrics = execute_binary(exe_out, runs=args.iterations, warmup=args.warmup)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"{bench_id}/{c_name}: {error}")
                continue
            print(f"  {c_name:<5} compile: {compile_time_ms:6.1f}ms | size: {binary_size:6d}B | runtime: {metrics['min_ms']:6.2f}ms")

            all_results[bench_id]["results"][c_name] = {
                "compile_ms": compile_time_ms,
                "size_bytes": binary_size,
                "runtime_min_ms": metrics["min_ms"],
                "runtime_avg_ms": metrics["avg_ms"],
                "runtime_med_ms": metrics["med_ms"],
                "exit_code": metrics["exit_code"],
            }

        # Windows process exit codes are 32-bit, but on Linux/Wine, POSIX exit status is 8-bit (code & 0xFF).
        # We normalize to 8-bit so comparisons match across native Windows and Wine.
        exits = {r['exit_code'] & 0xFF for r in all_results[bench_id]['results'].values()}
        if len(exits) > 1:
            failures.append(f"{bench_id}: toolchain result checksums disagree")

    md = generate_markdown(all_results)
    if failures:
        md += "\n## Measurement Errors\n\n" + "\n".join(f"- {error}" for error in failures) + "\n"
    # Detect true host architecture
    machine = platform.machine()
    if platform.system() == "Windows":
        arch64 = os.environ.get("PROCESSOR_ARCHITEW6432")
        arch = os.environ.get("PROCESSOR_ARCHITECTURE")
        if "ARM" in str(arch64).upper() or "ARM" in str(arch).upper() or "cobalt" in get_cpu_info().lower():
            machine = "ARM64 (Prism x64)"

    report = {
        'host': f"{platform.system()} {machine}",
        'cpu': get_cpu_info(),
        'iterations': args.iterations, 'warmup': args.warmup,
        'toolchains': list(compilers), 'errors': failures,
        'benchmarks': {key: {'name': value['meta']['name'], 'results': value['results']}
                       for key, value in all_results.items()},
    }
    (ROOT_DIR / 'benchmark-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    (ROOT_DIR / 'benchmark-summary.md').write_text(
        generate_readme_summary(report), encoding='utf-8')

    if not args.skip_update_md:
        md_file = ROOT_DIR / "BENCHMARKS.md"
        with open(md_file, "w", encoding="utf-8") as f:
            f.write(md)
        print(f"Updated {md_file.name}")
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as f:
            f.write(md)
    return 1 if failures else 0


def generate_readme_summary(report):
    lines = ['## Performance', '']
    if report['errors']:
        return '\n'.join(lines + ['Benchmark comparison incomplete; see [measurement errors](BENCHMARKS.md#measurement-errors).', ''])
    benchmarks = report['benchmarks']
    sable = [b['results']['Sable'] for b in benchmarks.values() if 'Sable' in b['results']]
    if not sable:
        return '\n'.join(lines + ['No successful Sable measurements for this run.', ''])
    speedups = [b['results']['MSVC']['compile_ms'] / b['results']['Sable']['compile_ms']
                for b in benchmarks.values() if 'MSVC' in b['results'] and 'Sable' in b['results']
                and b['results']['Sable']['compile_ms'] > 0]
    highlights = []
    if speedups and max(speedups) > 1:
        highlights.append(f"Up to **{max(speedups):.1f}× faster compilation** than MSVC")
    highlights.append(f"**{min(r['size_bytes'] for r in sable) / 1024:.1f} KiB** smallest Sable executable")
    lines += [' · '.join(highlights) + '.', '',
              '| Workload | Sable compile | Sable runtime | vs MSVC | vs GCC | vs TCC |',
              '| :--- | ---: | ---: | ---: | ---: | ---: |']
    for name, bench in benchmarks.items():
        results = bench['results']
        sable_res = results.get('Sable')
        sable_rt = sable_res['runtime_min_ms'] if sable_res else None
        compile_time = f"{sable_res['compile_ms']:.1f} ms" if sable_res else '—'

        cells = []
        for comp in ('MSVC', 'GCC', 'TCC'):
            other = results.get(comp)
            if not other or not sable_rt:
                cells.append('—')
                continue
            other_rt = other['runtime_min_ms']
            diff = abs(sable_rt - other_rt)
            # Within 5% difference or 0.1ms is effectively tied
            if diff <= 0.1 or (diff / max(sable_rt, other_rt)) < 0.05:
                cells.append(f"tied ({other_rt:.2f}ms)")
            elif sable_rt < other_rt:
                ratio = other_rt / sable_rt
                cells.append(f"**{ratio:.1f}× faster** ({other_rt:.2f}ms)")
            else:
                ratio = sable_rt / other_rt
                cells.append(f"{ratio:.1f}× slower ({other_rt:.2f}ms)")

        sable_cell = f"{sable_rt:.2f} ms" if sable_rt else '—'
        lines.append(f"| {name} | {compile_time} | **{sable_cell}** | " + ' | '.join(cells) + ' |')
    lines += ['', f"{report['host']} · {report['iterations']} measured runs · {report['warmup']} warmups · minimum runtime.",
              '[Full results & methodology](BENCHMARKS.md)', '']
    return '\n'.join(lines)
def get_cpu_info():
    if platform.system() == "Windows":
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0")
            val, _ = winreg.QueryValueEx(key, "ProcessorNameString")
            return val.strip()
        except Exception:
            return platform.processor() or "Unknown CPU"
    elif platform.system() == "Linux":
        try:
            with open("/proc/cpuinfo") as f:
                for line in f:
                    if "model name" in line:
                        return line.split(":", 1)[1].strip()
        except Exception:
            pass
    return platform.processor() or "Unknown CPU"

def generate_markdown(data):
    date_str = time.strftime("%Y-%m-%d")
    cpu = get_cpu_info()
    sys_name = f"{platform.system()} {platform.machine()}"

    lines = [
        "# Benchmark Showdown",
        "",
        f"- **Date:** {date_str}",
        f"- **Host:** `{sys_name}`",
        f"- **CPU:** `{cpu}`",
        "- **Toolchains:** Sable (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC",
        "",
        "## Execution Runtime (lower is better)",
        "",
        "| Workload | Sable | MSVC | GCC | TCC | Winner |",
        "| :--- | ---: | ---: | ---: | ---: | :---: |",
    ]

    for bench_id, b_data in data.items():
        meta = b_data["meta"]
        res = b_data["results"]
        if not res:
            continue

        fastest_rt = min((r["runtime_min_ms"] for r in res.values()), default=1)
        winner_name = [name for name, r in res.items() if r["runtime_min_ms"] == fastest_rt][0]

        def fmt_rt(r):
            if not r:
                return "—"
            rt_str = f"{r['runtime_min_ms']:.2f} ms"
            if r['runtime_min_ms'] == fastest_rt:
                return f"**{rt_str}**"
            return rt_str

        sable_rt = fmt_rt(res.get("Sable"))
        msvc_rt = fmt_rt(res.get("MSVC"))
        gcc_rt = fmt_rt(res.get("GCC"))
        tcc_rt = fmt_rt(res.get("TCC"))
        winner_badge = f"**{winner_name}**"

        lines.append(
            f"| **{meta['name']}** | {sable_rt} | {msvc_rt} | {gcc_rt} | {tcc_rt} | {winner_badge} |"
        )

    lines += [
        "",
        "## Compilation Speed & Binary Size",
        "",
        "| Workload | Sable | MSVC (`/O2`) | GCC (`-O3`) | TCC |",
        "| :--- | ---: | ---: | ---: | ---: |",
    ]

    for bench_id, b_data in data.items():
        meta = b_data["meta"]
        res = b_data["results"]
        if not res:
            continue

        fastest_c = min((r["compile_ms"] for r in res.values()), default=1)

        def fmt_c(r):
            if not r:
                return "—"
            c_str = f"{r['compile_ms']:.1f} ms ({r['size_bytes'] / 1024:.1f} KiB)"
            if r['compile_ms'] == fastest_c:
                return f"**{c_str}**"
            return c_str

        sable_c = fmt_c(res.get("Sable"))
        msvc_c = fmt_c(res.get("MSVC"))
        gcc_c = fmt_c(res.get("GCC"))
        tcc_c = fmt_c(res.get("TCC"))

        lines.append(
            f"| **{meta['name']}** | {sable_c} | {msvc_c} | {gcc_c} | {tcc_c} |"
        )

    lines += [
        "",
        "## Reproduction",
        "",
        "```powershell",
        "python benchmarks/run.py --iterations 5 --warmup 2",
        "```",
        "",
    ]

    return "\n".join(lines)

if __name__ == "__main__":
    sys.exit(main())
