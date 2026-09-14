#!/usr/bin/env python3
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / "release" / "USRDIR" / "patches.csv"
GRAPHICS = ROOT / "release" / "USRDIR" / "graphics_patches.csv"
FIXES = ROOT / "release" / "USRDIR" / "fix_patches.csv"
CREDITS = ROOT / "release" / "USRDIR" / "CREDITS.txt"


def read_pipe_rows(path):
    rows = []
    for raw in path.read_text(encoding="utf-8-sig", errors="ignore").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        rows.append([part.strip() for part in line.split("|")])
    return rows


def write_counter(out, counter):
    for name, total in sorted(counter.items(), key=lambda item: (-item[1], item[0].lower())):
        out.append(f"- {name}: {total} entries")


def main():
    patch_rows = read_pipe_rows(PATCHES)
    graphics_rows = read_pipe_rows(GRAPHICS)
    fix_rows = read_pipe_rows(FIXES) if FIXES.exists() else []

    patch_sources = Counter(row[6] for row in patch_rows if len(row) > 6)
    patch_status = Counter(row[3] for row in patch_rows if len(row) > 3)
    graphics_sources = Counter(row[6] for row in graphics_rows if len(row) > 6)
    graphics_status = Counter(row[4] for row in graphics_rows if len(row) > 4)
    fix_sources = Counter(row[6] for row in fix_rows if len(row) > 6)
    fix_status = Counter(row[3] for row in fix_rows if len(row) > 3)

    out = [
        "PS3 FPS Unlocker (PSUF) - Credits / Creditos",
        "",
        "Idea and project direction / Ideia e direcao do projeto:",
        "- RafJaeger",
        "",
        "Project and SDK acknowledgements:",
        "- PSL1GHT / PS3DEV contributors",
        "- webMAN MOD / PS3MAPI contributors",
        "- ArtemisPS3 contributors",
        "",
        "FPS research acknowledgements:",
        "- Joey85 and PSX-Place contributors for public PS3 FPS patch research and discussion.",
        "- RPCS3 team and contributors for emulator-side patch research used as reference by the community.",
        "- Nascar1243, NunoRS2000, DoSpamu, RPCS3toArtemisPatches contributors, and other community testers/authors referenced by the database or original threads.",
        "- Local PS3 hardware lab notes for patches marked as tested or rejected during hardware testing.",
        "- Nascar1243 for the Graphics Patches for PS3MAPI PDF, EBOOT examples, and PS3-FPS-Patches NCL files used as patch references.",
        "- ArtemisPS3 contributors and individual NCL authors for additional public FPS and graphics/visual direct-write codes.",
        "",
        "Public source links:",
        "- ArtemisPS3: https://github.com/bucanero/ArtemisPS3",
        "- RPCS3toArtemisPatches: https://github.com/DoSpamu/RPCS3toArtemisPatches",
        "- Nascar1243 / PS3-FPS-Patches: https://github.com/Nascar1243/PS3-FPS-Patches",
        "- PSX-Place Game Patches: https://www.psx-place.com/threads/game-patches.43706/",
        "- PSX-Place 60/Unlock FPS Patches: https://www.psx-place.com/threads/60-unlock-fps-patches.49905/",
        "",
        "Important credit note:",
        "- Each FPS patch entry below keeps the source value from release/USRDIR/patches.csv.",
        "- Each graphics/extra entry below keeps the source value from release/USRDIR/graphics_patches.csv.",
        "- Each companion fix/warning entry below keeps the source value from release/USRDIR/fix_patches.csv.",
        "- A source is credit/provenance, not a safety guarantee. Check the status before applying.",
        "- Entries marked untested, pc, unavailable, update_required, or tied to another game version should be treated with extra care.",
        "- ArtemisPS3 graphics imports are limited to direct NCL writes that PSUF can apply. AoB/pattern codes and unrelated gameplay cheats are not bundled as graphics patches.",
        "- The ncl_constant method means the original NCL requested constant write mode.",
        "- When adding a new patch, keep the original author, forum thread, tool, or research source whenever possible.",
        "",
        "FPS source summary from current patches.csv:",
    ]
    write_counter(out, patch_sources)
    out.extend(["", "FPS status summary from current patches.csv:"])
    write_counter(out, patch_status)
    out.extend(["", "Graphics/extra source summary from current graphics_patches.csv:"])
    write_counter(out, graphics_sources)
    out.extend(["", "Graphics/extra status summary from current graphics_patches.csv:"])
    write_counter(out, graphics_status)
    out.extend(["", "Companion fix/warning source summary from current fix_patches.csv:"])
    write_counter(out, fix_sources)
    out.extend(["", "Companion fix/warning status summary from current fix_patches.csv:"])
    write_counter(out, fix_status)

    out.extend([
        "",
        "FPS patch source index",
        "Format: Title ID | version | kind | status/method | label | source",
    ])

    by_source = defaultdict(list)
    for row in patch_rows:
        if len(row) < 9:
            continue
        by_source[row[6]].append(row)
    index = 1
    for source in sorted(by_source):
        out.extend(["", f"Source: {source}"])
        for row in by_source[source]:
            out.append(
                f"{index:03d}. {row[0]} | {row[1]} | {row[2]} | "
                f"{row[3]}/{row[4]} | {row[5]} | {row[6]}"
            )
            index += 1

    out.extend([
        "",
        "Graphics/extra source index",
        "Format: Title ID | version | game | status/method | label | source",
    ])

    by_source.clear()
    for row in graphics_rows:
        if len(row) < 10:
            continue
        by_source[row[6]].append(row)
    index = 1
    for source in sorted(by_source):
        out.extend(["", f"Source: {source}"])
        for row in by_source[source]:
            out.append(
                f"{index:03d}. {row[0]} | {row[1]} | {row[2]} | "
                f"{row[4]}/{row[5]} | {row[3]} | {row[6]}"
            )
            index += 1

    out.extend([
        "",
        "Companion fix/warning source index",
        "Format: Title ID | version | kind | status/method | label | source",
    ])

    by_source.clear()
    for row in fix_rows:
        if len(row) < 9:
            continue
        by_source[row[6]].append(row)
    index = 1
    for source in sorted(by_source):
        out.extend(["", f"Source: {source}"])
        for row in by_source[source]:
            out.append(
                f"{index:03d}. {row[0]} | {row[1]} | {row[2]} | "
                f"{row[3]}/{row[4]} | {row[5]} | {row[6]}"
            )
            index += 1

    CREDITS.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {CREDITS}")


if __name__ == "__main__":
    main()
