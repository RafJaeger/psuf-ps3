#!/usr/bin/env python3
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESEARCH = ROOT / "research"
SOURCES = [
    {
        "name": "RPCS3toArtemisPatches",
        "url": "https://github.com/DoSpamu/RPCS3toArtemisPatches.git",
        "path": RESEARCH / "RPCS3toArtemisPatches",
    },
]


def run(args, cwd=None):
    result = subprocess.run(
        args,
        cwd=str(cwd) if cwd else None,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return result.returncode, result.stdout.strip()


def git_head(path):
    if not (path / ".git").exists():
        return ""
    code, out = run(["git", "rev-parse", "--short", "HEAD"], path)
    return out if code == 0 else ""


def update_source(source):
    path = source["path"]
    before = git_head(path)
    if path.exists():
        code, out = run(["git", "pull", "--ff-only"], path)
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        code, out = run(["git", "clone", source["url"], str(path)])
    after = git_head(path)
    changed = before != after
    print(f"{source['name']}:")
    print(f"  before: {before or 'not present'}")
    print(f"  after : {after or 'unknown'}")
    print(f"  changed: {'yes' if changed else 'no'}")
    if code != 0:
        print(out)
    return changed


def main():
    any_changed = False
    for source in SOURCES:
        any_changed = update_source(source) or any_changed

    if not any_changed:
        print("No known patch source changed. Regenerating local patches.csv anyway.")

    code, out = run(["python", str(ROOT / "tools" / "import_fps_patch_db.py")], ROOT)
    print(out)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
