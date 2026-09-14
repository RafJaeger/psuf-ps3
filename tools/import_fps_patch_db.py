#!/usr/bin/env python3
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT / "research" / "RPCS3toArtemisPatches"
NASCAR_REPO = ROOT / "research" / "PS3-FPS-Patches"
ARTEMIS_CODES = ROOT / "research" / "ArtemisPS3" / "docs" / "codes"
OUT = ROOT / "release" / "USRDIR" / "patches.csv"
HARDWARE_FINDINGS = ROOT / "tools" / "hardware_fps_findings.csv"
MANUAL_OVERRIDES = ROOT / "tools" / "manual_patch_overrides.csv"

BLOCKED_PUBLIC_PATCHES = {
    ("BLUS30262", "01.01", "unlock", "0 00806EC0 2C030001"),
}

REPLACED_GTA_V_TITLE_IDS = {"BLES01807", "BLJM61019", "BLUS31156"}

BLOCKED_TITLE_KIND = {
    ("BCAS20002", "01.01", "60"),
    ("BCAS20055", "01.00", "60"),
    ("BCES00002", "01.00", "60"),
    ("BCUS98120", "01.00", "60"),
    ("BLES00322", "01.01", "60"),
    ("BLES01040", "01.00", "60"),
    ("BLES01040", "01.01", "60"),
    ("BLES01789", "01.00", "60"),
    ("BLUS30451", "01.61", "60"),
    ("BLUS30624", "01.00", "60"),
    ("BLUS30624", "01.01", "60"),
    ("BLUS30680", "01.00", "60"),
    ("BLUS30680", "01.01", "60"),
    ("BLUS31146", "01.00", "60"),
    ("BLUS31201", "01.03", "60"),
    ("BLUS31201", "01.00", "60"),
    ("BLUS98120", "01.60", "unlock"),
    ("BLUS30262", "01.01", "60"),
}

TITLE_ID_RE = re.compile(r"\b[A-Z]{4}\d{5}\b")
VERSION_RE = re.compile(r"\b(?:av|v)?(\d{2}\.\d{2})\b", re.IGNORECASE)


def clean_field(value: str) -> str:
    return value.replace("\ufeff", "").replace("|", " ").replace("\r", " ").replace("\n", " ").strip()


def parse_filename(path: Path):
    name = path.stem
    title_match = TITLE_ID_RE.search(name)
    if not title_match:
        return None
    title_ids = sorted(set(TITLE_ID_RE.findall(name)))
    versions = VERSION_RE.findall(name)
    version = versions[0] if versions else "*"
    game_name = name[: title_match.start()].strip(" -_")
    return title_ids, version, game_name or title_ids[0]


def read_fps_sections(path: Path):
    lines = path.read_text(encoding="utf-8-sig", errors="ignore").splitlines()
    sections = []
    i = 0
    while i < len(lines):
        title = lines[i].strip()
        i += 1
        if not title:
            continue
        body = []
        while i < len(lines) and lines[i].strip() != "#":
            body.append(lines[i].strip())
            i += 1
        if i < len(lines) and lines[i].strip() == "#":
            i += 1

        lowered = title.lower()
        if "fps" not in lowered and "frame" not in lowered:
            continue

        if not body:
            continue
        mode = body[0].strip()
        if mode not in ("0", "1", "T", "t"):
            continue
        method = "ncl_constant" if mode in ("1", "T", "t") else "ncl"
        code_start = 1
        if len(body) > 1:
            probe = body[1].strip()
            parts = probe.split()
            if not (len(parts) >= 3 and parts[0] == "0"):
                code_start = 2

        code_lines = []
        unsafe = False
        for raw in body[code_start:]:
            if not raw or raw.startswith("/*") or raw.startswith("*") or raw.endswith("*/"):
                continue
            if raw.startswith("["):
                unsafe = True
                continue
            parts = raw.split()
            if len(parts) >= 3 and parts[0] == "0":
                code_lines.append(" ".join(parts[:3]))
            elif raw:
                unsafe = True

        if code_lines and not unsafe:
            sections.append((title, code_lines, method))
    return sections


def classify_kind(label: str, status: str):
    lowered = label.lower()
    has_60 = "60" in lowered
    has_strong_unlock = (
        "unlimited" in lowered
        or "unlimit" in lowered
        or "uncap" in lowered
        or "+60" in lowered
        or "60+" in lowered
        or "disable fps cap" in lowered
    )
    has_unlock = has_strong_unlock or "unlock" in lowered
    has_30 = "30" in lowered

    if has_30:
        return ["30"]
    if has_60 and not has_unlock:
        return ["60"]
    if status == "untested" and not has_strong_unlock:
        return ["60"]
    if has_unlock and not has_60:
        return ["unlock"]
    return ["60", "unlock"]


def collect_rows():
    rows = []
    seen = set()
    sources = [
        (REPO / "Working Artemis Patches", "known", "RPCS3toArtemis Working", "Confirmed working on real PS3 hardware."),
        (REPO / "PSXPlace Confirmed", "known", "PSXPlace Confirmed", "Community confirmed on real PS3 hardware."),
        (NASCAR_REPO, "untested", "Nascar1243 / PS3-FPS-Patches", "Public Artemis/NCL code by Nascar1243. Not confirmed by PSUF on this PS3."),
        (ARTEMIS_CODES, "untested", "ArtemisPS3 docs/codes", "Imported from ArtemisPS3 docs/codes. Not confirmed by PSUF on this PS3."),
        (REPO / "USERLIST", "untested", "RPCS3 converted USERLIST", "Converted from RPCS3/community data. Not confirmed on this PS3."),
    ]

    for base, status, source, note in sources:
        if not base.exists():
            continue
        for path in sorted(base.rglob("*.ncl")):
            meta = parse_filename(path)
            if not meta:
                continue
            title_ids, version, game_name = meta
            for section_title, code_lines, method in read_fps_sections(path):
                kinds = classify_kind(section_title, status)
                if not kinds:
                    continue
                payload = ";".join(code_lines)
                label = section_title
                if "fps" not in label.lower():
                    label = "Unlock FPS"
                for title_id in title_ids:
                    for kind in kinds:
                        if (title_id, version, kind) in BLOCKED_TITLE_KIND:
                            continue
                        if (title_id, version, kind, payload) in BLOCKED_PUBLIC_PATCHES:
                            continue
                        key = (title_id, version, kind, payload)
                        if key in seen:
                            continue
                        seen.add(key)
                        rows.append([
                            title_id,
                            version,
                            kind,
                            status,
                            method,
                            clean_field(label),
                            clean_field(source),
                            clean_field(payload),
                            clean_field(f"{note} Source file: {game_name}."),
                            "120",
                        ])
    return rows


def keep_current_row(row):
    text = " ".join(row[5:9]).lower()
    if row[0] in REPLACED_GTA_V_TITLE_IDS and row[6] != "mnz / PSUF community":
        return False
    if row[0] == "BLUS31162" and ("visual" in text or "grafico" in text or "mlaa" in text):
        return False
    return True


def read_manual_findings():
    rows = []
    for path in (MANUAL_OVERRIDES, HARDWARE_FINDINGS):
        if not path.exists():
            continue
        for raw in path.read_text(encoding="utf-8-sig", errors="ignore").splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [clean_field(part) for part in line.split("|")]
            if len(parts) >= 9:
                rows.append(parts[:10])
    return rows


def strong_unlock(row):
    text = " ".join(row[5:9]).lower()
    return any(value in text for value in (
        "unlimited",
        "unlimit",
        "uncap",
        "+60",
        "60+",
        "disable fps cap",
    ))


def block_uncertain_duplicate_unlocks(rows):
    by_payload = {}
    for index, row in enumerate(rows):
        if len(row) < 9 or not row[7]:
            continue
        by_payload.setdefault((row[0], row[1], row[7]), []).append(index)

    for indexes in by_payload.values():
        has_60 = any(
            rows[index][2] == "60" and rows[index][3] != "untested"
            for index in indexes
        )
        if not has_60:
            continue
        for index in indexes:
            row = rows[index]
            if row[2] == "unlock" and not strong_unlock(row):
                row[3] = "unavailable"
                row[4] = "none"
                row[5] = "Use 60 FPS alvo"
                row[7] = ""
                row[8] = "Entrada ilimitada bloqueada: o mesmo payload tambem existe como 60 FPS alvo e nao ha confirmacao clara de FPS ilimitado."
                del row[9:]
    return rows


def block_uncertain_duplicate_60s(rows):
    by_payload = {}
    for index, row in enumerate(rows):
        if len(row) < 9 or not row[7]:
            continue
        by_payload.setdefault((row[0], row[1], row[7]), []).append(index)

    for indexes in by_payload.values():
        has_confirmed_unlock = any(
            rows[index][2] == "unlock" and rows[index][3] != "untested"
            for index in indexes
        )
        if not has_confirmed_unlock:
            continue
        for index in indexes:
            row = rows[index]
            text = " ".join(row[5:9]).lower()
            explicit_60 = "60 fps" in text or "60fps" in text or "60 hz" in text
            if row[2] == "60" and row[3] == "untested" and not explicit_60:
                row[3] = "unavailable"
                row[4] = "none"
                row[5] = "Use FPS ilimitado / +60"
                row[7] = ""
                row[8] = "Entrada 60 FPS bloqueada: o mesmo payload ja existe como desbloqueio confirmado e a copia publica nao confirma alvo 60 separado."
                del row[9:]
    return rows


def dedupe_final_rows(rows):
    deduped = []
    seen = set()
    for row in rows:
        key = tuple(row[:8])
        if key in seen:
            continue
        seen.add(key)
        deduped.append(row)
    return deduped


def main():
    rows = read_manual_findings() + collect_rows()
    base_rows = [
        ["BCES01893", "*", "unlock", "known", "eboot_pc", "Unlock FPS", "Manual research", "", "Known-compatible class, PC app required for safe EBOOT work."],
        ["BCES01893", "*", "60", "known", "eboot_pc", "60 FPS target", "Manual research", "", "Known-compatible class, PC app required for safe EBOOT work."],
    ]
    selected_rows = [row for row in base_rows + rows if keep_current_row(row)]
    final_rows = dedupe_final_rows(
        block_uncertain_duplicate_60s(block_uncertain_duplicate_unlocks(selected_rows))
    )
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="\n") as f:
        f.write("# title_id|version|kind|status|method|label|source|payload|note|delay_seconds\n")
        for row in final_rows:
            f.write("|".join(row) + "\n")
    print(f"wrote {len(final_rows)} rows to {OUT}")


if __name__ == "__main__":
    main()
