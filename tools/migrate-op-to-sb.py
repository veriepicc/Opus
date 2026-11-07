#!/usr/bin/env python3
"""Migrate Opus codebase to Sable: renames .op to .sb and updates references."""

import argparse
import os
import re
import subprocess
from pathlib import Path

def is_git_repo(path: Path) -> bool:
    try:
        p = subprocess.run(["git", "rev-parse", "--is-inside-work-tree"], cwd=path, capture_output=True, text=True)
        return p.returncode == 0 and "true" in p.stdout
    except Exception:
        return False

def move_file(src: Path, dst: Path, git: bool):
    if git:
        p = subprocess.run(["git", "mv", str(src), str(dst)], capture_output=True, text=True)
        if p.returncode == 0:
            return
    src.rename(dst)

def migrate(target_dir: Path):
    target = target_dir.resolve()
    git = is_git_repo(target)
    
    skip_dirs = {".git", "bin", "tmp", ".msvc_cache", "__pycache__", "build", "dist"}
    binary_exts = {".exe", ".obj", ".ifc", ".bin", ".lib", ".dll", ".png", ".jpg", ".svg"}
    
    renamed_files = 0
    updated_files = 0

    # 1. Rename files: opus.project -> sable.project, *.op -> *.sb
    for root, dirs, files in os.walk(target, topdown=True):
        dirs[:] = [d for d in dirs if d not in skip_dirs]
        for f in files:
            path = Path(root) / f
            if f == "opus.project":
                new_path = path.parent / "sable.project"
                move_file(path, new_path, git)
                renamed_files += 1
                print(f"[RENAME] {path} -> {new_path.name}")
            elif f.endswith(".op"):
                new_path = path.with_suffix(".sb")
                move_file(path, new_path, git)
                renamed_files += 1
                print(f"[RENAME] {path} -> {new_path.name}")

    # 2. Update references in source, project, config, and text files
    for root, dirs, files in os.walk(target):
        dirs[:] = [d for d in dirs if d not in skip_dirs]
        for f in files:
            path = Path(root) / f
            if path.suffix.lower() in binary_exts:
                continue
            try:
                content = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, PermissionError):
                continue

            orig = content
            # update .op file references to .sb
            content = re.sub(r"([a-zA-Z0-9_\-\/]+)\.op\b", r"\1.sb", content)
            # update opus.project references
            content = content.replace("opus.project", "sable.project")

            if content != orig:
                path.write_text(content, encoding="utf-8")
                updated_files += 1
                print(f"[UPDATE] {path}")

    print(f"\nMigration complete: {renamed_files} files renamed, {updated_files} files updated.")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", default=".", type=Path, help="Target directory to migrate (defaults to current directory)")
    args = parser.parse_args()
    migrate(args.path)

if __name__ == "__main__":
    main()
