#!/usr/bin/env python3
import json
import sys
import zipfile
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
USRDIR = ROOT / "release" / "USRDIR"
FILES = ["patches.csv", "graphics_patches.csv", "native60.csv", "fix_patches.csv"]


def main():
    version = sys.argv[1] if len(sys.argv) > 1 else datetime.now().strftime("%Y.%m.%d")
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / f"PSUF-code-database-{version}.psufdb"
    missing = [name for name in FILES if not (USRDIR / name).exists()]
    if missing:
        raise SystemExit("Missing database file(s): " + ", ".join(missing))

    manifest = {
        "format": "PSUF database update v1",
        "version": version,
        "created_at": datetime.now().isoformat(timespec="seconds"),
        "files": FILES,
        "usage": "Import this .psufdb in PSUF PC Updater, send to PS3, then use App > Update with PC in PSUF.",
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("manifest.json", json.dumps(manifest, indent=2))
        for name in FILES:
            archive.write(USRDIR / name, arcname=name)
    print(out)


if __name__ == "__main__":
    main()
