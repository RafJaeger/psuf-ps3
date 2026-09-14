# Architecture

## V1: store-style foreground app

The PS3 app follows the same broad model used by foreground homebrew stores:

- open from XMB as a normal PKG app;
- load a compact local database from `USRDIR`;
- scan/list items in-app;
- let the user choose one action at a time;
- write only after confirmation;
- keep backups inside the app data folder.

This is the safest fit for CFW 4.90+ and PS3HEN because it avoids a resident
plugin, avoids boot-time hooks, and does not touch flash.

## What we borrow from store apps

- A simple database file that can later be updated.
- Filterable/list-like results.
- Clear action buttons.
- A visible About screen.
- Offline-first behavior.

## What we do not copy

The app does not install games, RAP/RIF files, or copyrighted content. Its
database stores patch metadata only.

## webMAN-style integration

webMAN MOD is better treated as an optional bridge:

- refresh game lists after Artemis files are copied;
- use FTP/HTTP from a PC helper;
- apply runtime PS3MAPI memory patches only when the user explicitly chooses an
  advanced/unsafe route.

The V1 PS3 app is not a webMAN-style resident plugin. A plugin can be added as a
separate CFW/HEN-specific build only after the foreground app is stable.

## V2.x advanced route boundary

The foreground PKG is still the normal app model. PSUF may create webMAN/Artemis
script files for known `.ncl` memory writes, but these are user-confirmed
runtime routes, not boot plugins and not flash/LV2 modifications. Runtime
scripts stay in place until the user removes the patch or restores the backup,
so webMAN can reapply the selected patch when the game starts again.

Mounted ISO/disc scans are deliberately conservative: the app identifies the
mounted game from webMAN metadata and does not open `/dev_bdvd` for EBOOT
analysis, because that path caused freezes on real hardware. If the mounted game
version is unknown, version-specific database rows are not treated as a match
unless the version is already known from the saved scan cache.

## Compatibility policy

- One foreground PKG for CFW 4.90+ and PS3HEN.
- HEN users must enable HEN before opening the app.
- Any feature that requires syscall8, LV2 access, VSH plugin loading, or memory
  writes to a running game must be separated from the safe flow and marked as
  advanced/unsafe in the UI.

## Runtime data

Bundled files live in `/dev_hdd0/game/FPSU00001/USRDIR`. User data lives in
`/dev_hdd0/FPSU`, with backups under `/dev_hdd0/FPSU/backups` and logs/cache
under `/dev_hdd0/FPSU/cache`.

Patch application creates only the files needed by webMAN/PS3MAPI or Artemis:

- `/dev_hdd0/tmp/wm_ingame/*.bat` for webMAN in-game auto-apply scripts.
- `/dev_hdd0/tmp/artemis/*.ncl` for Artemis-compatible direct writes.
- `/dev_hdd0/FPSU/cache/last_apply_setup.log` for support.
- `/dev_hdd0/FPSU/cache/webman_ingame.log` when webMAN actually runs the script.

Temporary `art.txt` and `art.log` files are cleared after normal setup. When a
backup is restored or a patch is removed for one game, PSUF removes the generated
runtime files for that game too.

## Patch data rules

`patches.csv` stores FPS-related entries. `graphics_patches.csv` stores visual
and graphics entries. `fix_patches.csv` stores companion fixes that can be
appended to an FPS patch, for example game-speed fixes. `native60.csv` only marks
games that already target 60 FPS so the app does not offer a useless 60 FPS
target patch.

Every direct memory write uses this format:

```text
0 ADDRESS VALUE
```

Multiple writes in one patch are separated with semicolons. The parser accepts
only hexadecimal addresses and values. Values imported from the PC app are
validated before they are saved or exported.

## CFW/HEN checks

The dependency check is intentionally read-only. It checks if webMAN responds,
tries to start PS3MAPI through webMAN, checks Artemis availability, and reports
what is missing. It does not modify flash, does not install files, and does not
change private console identifiers.

Some HEN consoles can pass the check but still fail to run webMAN in-game
scripts. In that case `webman_ingame.log` will be missing, which points to the
in-game webMAN trigger rather than a bad patch row.
