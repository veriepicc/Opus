#!/usr/bin/env python3
"""Generate src/stdlib.ixx containing embedded stdlib sources."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STDLIB_DIR = ROOT / "stdlib"
OUTPUT_FILE = ROOT / "src" / "stdlib.ixx"
MAX_CHUNK_CHARS = 8192

def get_module_name(root: Path, file_path: Path) -> str:
    rel = file_path.resolve().relative_to(root.resolve()).as_posix()
    if rel.endswith(".sb") or rel.endswith(".op"):
        rel = rel[:-3]
    return rel.replace("/", ".")

def generate():
    files = sorted([p for p in STDLIB_DIR.rglob("*") if p.is_file() and p.suffix in (".sb", ".op")])
    lines = [
        "// generated file - do not edit by hand",
        "",
        "export module sable.stdlib;",
        "",
        "import std;",
        "",
        "export namespace sable {",
        "export inline const std::unordered_map<std::string, std::string>& embedded_stdlib_sources() {",
        "    static const std::unordered_map<std::string, std::string> sources = {",
    ]

    seen = set()
    for f in files:
        mod_name = get_module_name(STDLIB_DIR, f)
        if mod_name in seen:
            continue
        seen.add(mod_name)

        safe = re.sub(r'[^A-Za-z0-9]', '', mod_name).upper()[:8]
        delimiter = f"SB{safe}"
        content = f.read_text(encoding="utf-8")

        if not content:
            lines.append(f'        {{"{mod_name}", std::string{{}}}},')
            continue

        chunks = []
        for offset in range(0, len(content), MAX_CHUNK_CHARS):
            piece = content[offset:offset + MAX_CHUNK_CHARS]
            chunks.append(f'R"{delimiter}({piece}){delimiter}"')

        chunk_str = "\n            ".join(chunks)
        lines.append(f'        {{"{mod_name}", {chunk_str}}},')

    lines.extend([
        "    };",
        "    return sources;",
        "}",
        "}",
        ""
    ])

    OUTPUT_FILE.write_text("\n".join(lines), encoding="utf-8")
    print(f"generated embedded stdlib module: {OUTPUT_FILE}")

if __name__ == "__main__":
    generate()
