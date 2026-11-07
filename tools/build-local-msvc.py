#!/usr/bin/env python3
"""Build Opus compiler locally using MSVC from /mnt/windows via Wine."""

import os
import sys
import time
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MSVC_DIR = Path("/mnt/windows/Program Files/Microsoft Visual Studio/18/Insiders/VC/Tools/MSVC/14.50.35503")
MSVC_BIN = MSVC_DIR / "bin/Hostx64/x64"
CL_EXE = MSVC_BIN / "cl.exe"

if not CL_EXE.is_file():
    print(f"Error: MSVC compiler not found at {CL_EXE}", file=sys.stderr)
    sys.exit(1)

# Windows Wine include / lib paths
INC = [
    r"Z:\mnt\windows\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35503\include",
    r"Z:\mnt\windows\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt",
    r"Z:\mnt\windows\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared",
    r"Z:\mnt\windows\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um",
]
LIB = [
    r"Z:\mnt\windows\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35503\lib\x64",
    r"Z:\mnt\windows\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64",
    r"Z:\mnt\windows\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64",
]

CACHE_DIR = ROOT / "bin" / ".msvc_cache"
CACHE_DIR.mkdir(parents=True, exist_ok=True)
OUT_DIR = ROOT / "bin" / "Release"
OUT_DIR.mkdir(parents=True, exist_ok=True)

ENV = os.environ.copy()
ENV["INCLUDE"] = ";".join(INC)
ENV["LIB"] = ";".join(LIB)
ENV["WINEDEBUG"] = "-all"

# 1. Ensure std.ifc is compiled
STD_IFC = CACHE_DIR / "std.ifc"
STD_OBJ = CACHE_DIR / "std.obj"
STD_IXX = r"Z:\mnt\windows\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.50.35503\modules\std.ixx"

if not STD_IFC.is_file() or not STD_OBJ.is_file():
    print("Compiling std.ixx module (one-time cache)...")
    cmd = [
        "wine", str(CL_EXE), "/nologo", "/std:c++latest", "/EHsc", "/utf-8", "/O2",
        f"/Fo:{CACHE_DIR / 'std.obj'}",
        "/c", STD_IXX
    ]
    p = subprocess.run(cmd, env=ENV, cwd=CACHE_DIR)
    if p.returncode != 0:
        print("Failed to compile std.ixx", file=sys.stderr)
        sys.exit(p.returncode)

# 2. Build modules in dependency order
MODULES = [
    ("types", "src/types.ixx"),
    ("errors", "src/errors.ixx"),
    ("ast", "src/ast.ixx"),
    ("lexer", "src/lexer.ixx"),
    ("parser", "src/parser.ixx"),
    ("x64", "src/x64.ixx"),
    ("pe", "src/pe.ixx"),
    ("vectorize", "src/vectorize.ixx"),
    ("stdlib", "src/stdlib.ixx"),
    ("codegen", "src/codegen.ixx"),
    ("project", "src/project.ixx"),
    ("opus", "src/opus.ixx"),
    ("main", "src/main.ixx"),
]

obj_files = [CACHE_DIR / "std.obj"]

print("Compiling Opus modules locally with MSVC...")
t0 = time.time()

def to_win(p: Path) -> str:
    return "Z:" + str(p.resolve()).replace("/", "\\")

for mod_name, mod_file in MODULES:
    src_path = ROOT / mod_file
    obj_path = CACHE_DIR / f"{mod_name}.obj"
    obj_files.append(obj_path)
    
    cmd = [
        "wine", str(CL_EXE), "/nologo", "/std:c++latest", "/EHsc", "/utf-8", "/bigobj", "/O2", "/permissive-",
        f"/ifcSearchDir:{to_win(CACHE_DIR)}",
        f"/Fo:{to_win(obj_path)}",
        "/c", to_win(src_path)
    ]
    p = subprocess.run(cmd, env=ENV, cwd=CACHE_DIR, capture_output=True, text=True)
    if p.returncode != 0:
        print(f"Error compiling {mod_file}:", file=sys.stderr)
        print(p.stdout + p.stderr, file=sys.stderr)
        sys.exit(p.returncode)

# 3. Link executable
LINK_EXE = MSVC_BIN / "link.exe"
out_exe = OUT_DIR / "opus.exe"
print(f"Linking {out_exe}...")

link_cmd = [
    "wine", str(LINK_EXE), "/nologo", "/SUBSYSTEM:CONSOLE",
    *[to_win(obj) for obj in obj_files],
    f"/OUT:{to_win(out_exe)}",
    "kernel32.lib", "user32.lib", "shell32.lib"
]
p = subprocess.run(link_cmd, env=ENV, cwd=ROOT, capture_output=True, text=True)
if p.returncode != 0:
    print("Link failed:", file=sys.stderr)
    print(p.stdout + p.stderr, file=sys.stderr)
    sys.exit(p.returncode)

dt = time.time() - t0
print(f"Built Opus successfully in {dt:.1f}s -> {out_exe}")
