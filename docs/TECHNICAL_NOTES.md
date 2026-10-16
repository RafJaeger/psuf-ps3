# PSUF technical notes

This file is meant for people reviewing or maintaining the project source.

## Main modules

- `source/main.c`: app state, screens, controller flow, language selection, and
  user actions.
- `source/scanner.c`: internal HDD, USB, mounted game/disc, folder game, and ISO
  detection.
- `source/iso9660.c`: read-only ISO9660 access used to read `PARAM.SFO` from
  PS3 ISO files.
- `source/sfo.c`: `PARAM.SFO` parsing, including update version handling.
- `source/cache.c`: saved game library, scan progress, and scan cache.
- `source/patch_db.c`: FPS, 30 FPS, unlock, graphics, companion-fix, native-60,
  backup cleanup, NCL, and webMAN script handling.
- `source/backup.c`: per-game backup and restore.
- `source/repo_update.c`: online database downloads, validation and replacement.
- `source/webman_control.c`: local webMAN checks and clock control.

## Source layout for release

This repository contains the PS3 app source, bundled database files, docs, and
database tests. It should not include generated PKGs, local scan dumps, Discord
bot setup files, private logs, machine-local configuration, or the Windows helper
app source.

## Error handling

The PS3 app keeps user-facing errors short and stores technical context in
`/dev_hdd0/FPSU/cache`. For support, ask users for:

- `scan_cache.csv`
- `progress.txt`
- `last_apply_setup.log`
- `webman_ingame.log`, only when it exists

Never ask for PS3ID, Console ID, PSID, PSN data, private keys, passwords, or
game files.

## Validation

Run the database tests before making a release:

```sh
python -m unittest discover -s tests
```

The tests check CSV shape, duplicate patch rows, direct-write syntax, database
package contents, and basic privacy guardrails for public files.

## Online database

Publish database changes to the `main` branch under `release/USRDIR/`:

- `patches.csv`: FPS patches.
- `graphics_patches.csv`: visual patches.
- `fix_patches.csv`: companion fixes.
- `native60.csv`: games with native 60 FPS support.

Keep the existing pipe-delimited format. A new patch using the supported format
does not require a new PKG. The console downloads these four files through the
configured HTTP mirror, validates them and saves them in `/dev_hdd0/FPSU/`.
Each request uses a fresh cache key. A valid downloaded database takes priority
over the bundled copy even when entries were removed. The saved game library is
then matched against the downloaded databases without rescanning the storage.

Run the tests with a host C compiler installed to also exercise the downloader
and database selection. Those tests use a local HTTP server and do not access a
console. Availability and freshness still depend on the external mirror.
