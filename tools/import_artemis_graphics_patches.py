#!/usr/bin/env python3
import csv
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ARTEMIS_CODES = ROOT / "research" / "ArtemisPS3" / "docs" / "codes"
OUT = ROOT / "release" / "USRDIR" / "graphics_patches.csv"
REPORT = ROOT / "research" / "artemis_graphics_import.csv"

FIELDS = [
    "title_id",
    "version",
    "game",
    "label",
    "status",
    "method",
    "source",
    "payload",
    "note",
    "delay_seconds",
]

TITLE_RE = re.compile(r"\b[A-Z]{4}\d{5}\b")
DIRECT_WRITE_RE = re.compile(r"^0\s+([0-9A-Fa-f]{1,16})\s+([0-9A-Fa-f]{2,512})\s*$")
VERSION_RE = re.compile(r"(?<![A-Za-z])(\d{2}\.\d{2})\b", re.IGNORECASE)
VERSION_WITH_PREFIX_RE = re.compile(r"\bv(\d{2}\.\d{2})\b", re.IGNORECASE)

GRAPHICS_LABEL_PATTERNS = [
    r"\bmotion\s+blur\b",
    r"\bvelocity\s+motion\s+blur\b",
    r"\bdepth\s+of\s+field\b",
    r"\bdof\b",
    r"\bbloom\b",
    r"\bmlaa\b",
    r"\bfxaa\b",
    r"\bssao\b",
    r"\bambient\s+occlusion\b",
    r"\banti[- ]?alias(?:ing)?\b",
    r"\bbrightness\b",
    r"\bbright\s+white\b",
    r"\bblack\s+and\s+white\b",
    r"\bgr[ae]yscale\b",
    r"\bmonochrome\b",
    r"\bgamma\b",
    r"\bcontrast\b",
    r"\bsaturation\b",
    r"\bfilm\s+grain\b",
    r"\bgrain\b",
    r"\blens\s+flare\b",
    r"\bfog\b",
    r"\breflection\b",
    r"\breflections\b",
    r"\bmesh\s+trimming\b",
    r"\bvisual\s+damage\b",
    r"\bvisual\s+effect\b",
    r"\bgraphics?\b",
    r"\brender(?:ing)?\b",
    r"\bshader(?:s)?\b",
    r"\btextures?\b",
    r"\bdraw\s+distance\b",
    r"\blod\b",
    r"\bresolution\b",
    r"\b720p\b",
    r"\b1080p\b",
    r"\bv[- ]?sync\b",
    r"\bscreen\s+tear(?:ing)?\b",
    r"\bblack\s+bars?\b",
    r"\bletterbox\b",
    r"\boverlay\s+display\b",
    r"\bgrid\s+lines\s+effect\b",
]

GRAPHICS_LABEL_RE = re.compile("|".join(f"(?:{pattern})" for pattern in GRAPHICS_LABEL_PATTERNS), re.IGNORECASE)
SHADOW_RE = re.compile(r"\b(disable|enable|remove|no|less|more|toggle)\s+shadows?\b|\bshadows?\s+(off|on|quality)\b", re.IGNORECASE)


def clean(value):
    return str(value or "").replace("|", " ").replace("\r", " ").replace("\n", " ").strip()


def version_from_name(name):
    without_av = re.sub(r"\b(?:a|t)v\d{2}\.\d{2}\b", " ", name, flags=re.IGNORECASE)
    match = VERSION_RE.search(without_av)
    if match:
        return match.group(1)
    match = VERSION_WITH_PREFIX_RE.search(name)
    if match:
        return match.group(1)
    return "*"


def game_from_name(name):
    text = Path(name).stem
    text = TITLE_RE.sub(" ", text)
    text = re.sub(r"\b(?:v|av|tv)?\d{2}\.\d{2}\b", " ", text, flags=re.IGNORECASE)
    text = re.sub(r"\b(?:and|or)?\s*v\d{2}\.\d{2}\b", " ", text, flags=re.IGNORECASE)
    text = re.sub(r"\s+", " ", text)
    return text.strip(" -_") or "Unknown game"


def is_graphics_label(label):
    text = clean(label)
    if not text:
        return False
    if text.lower().startswith("aob "):
        return False
    if GRAPHICS_LABEL_RE.search(text):
        return True
    return bool(SHADOW_RE.search(text))


def ncl_blocks(text):
    block = []
    in_comment = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("/*"):
            in_comment = True
            continue
        if in_comment:
            if "*/" in line:
                in_comment = False
            continue
        if line == "#":
            if block:
                yield block
            block = []
            continue
        if line:
            block.append(line)
    if block:
        yield block


def parse_block(block):
    if len(block) < 3:
        return None
    label, mode = block[0], block[1]
    if mode not in ("0", "1", "T", "t") or not is_graphics_label(label):
        return None
    method = "ncl_constant" if mode in ("1", "T", "t") else "ncl"
    author = ""
    code_start = 2
    probe = block[2].split()
    if not (len(probe) >= 3 and probe[0] == "0"):
        author = block[2]
        code_start = 3

    writes = []
    for line in block[code_start:]:
        if line.startswith("[") and line.endswith("]"):
            continue
        if line.startswith("[Z") or line.startswith("[ZZ"):
            continue
        match = DIRECT_WRITE_RE.match(line)
        if match:
            address = match.group(1).upper().rjust(8, "0")
            value = match.group(2).upper()
            if len(value) % 2:
                value = "0" + value
            writes.append((address, value))
            continue
        return None
    if not writes:
        return None
    return label, author, writes, method


def row_key(row):
    return tuple(row[:8])


def read_existing(path):
    rows = []
    seen = set()
    if not path.exists():
        return rows, seen
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        for row in csv.reader(handle, delimiter="|"):
            if not row:
                continue
            if row[0].startswith("#"):
                continue
            cleaned = [clean(value) for value in row]
            if len(cleaned) > 6 and cleaned[6].startswith("ArtemisPS3 /"):
                continue
            rows.append(cleaned)
            seen.add(row_key(cleaned))
    return rows, seen


def build_rows():
    rows = []
    for path in sorted(ARTEMIS_CODES.glob("*.ncl")):
        title_ids = TITLE_RE.findall(path.name)
        if not title_ids:
            continue
        version = version_from_name(path.name)
        game = game_from_name(path.name)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for block in ncl_blocks(text):
            parsed = parse_block(block)
            if not parsed:
                continue
            label, author, writes, method = parsed
            payload = ";".join(f"0 {address} {value}" for address, value in writes)
            source = f"ArtemisPS3 / {clean(author) or 'unknown'}"
            note = (
                "Imported from ArtemisPS3 docs/codes. "
                f"Original file: {path.name}. "
                "Direct PS3MAPI writes only; not confirmed by PSUF on real hardware."
            )
            for title_id in sorted(set(title_ids)):
                rows.append([
                    title_id,
                    version,
                    game,
                    clean(label),
                    "untested",
                    method,
                    source,
                    payload,
                    note,
                    "120",
                ])
    return rows


def main():
    if not ARTEMIS_CODES.exists():
        raise SystemExit(f"Missing Artemis codes folder: {ARTEMIS_CODES}")

    existing_rows, seen = read_existing(OUT)
    imported_rows = []
    for row in build_rows():
        key = row_key(row)
        if key in seen:
            continue
        seen.add(key)
        imported_rows.append(row)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# title_id|version|game|label|status|method|source|payload|note|delay_seconds\n")
        for row in existing_rows + imported_rows:
            handle.write("|".join(clean(value) for value in row) + "\n")

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    with REPORT.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# title_id|version|game|label|status|method|source|payload|note|delay_seconds\n")
        for row in imported_rows:
            handle.write("|".join(clean(value) for value in row) + "\n")

    print(f"Existing rows kept: {len(existing_rows)}")
    print(f"Artemis graphics rows imported: {len(imported_rows)}")
    print(f"Output: {OUT}")
    print(f"Report: {REPORT}")


if __name__ == "__main__":
    main()
