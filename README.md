# PSUF

PSUF is a PlayStation 3 homebrew app focused on FPS and visual patch management.
It scans compatible PS3 games, shows known patch routes, and applies the selected
route through webMAN/PS3MAPI or Artemis-style direct writes when the console
supports it.

The app is built with PSL1GHT/PS3DEV and targets CFW 4.90+ and PS3HEN.

## What It Does

- Scans games from the internal HDD, USB devices, mounted disc/game, and PS3ISO.
- Reads `PARAM.SFO` to identify Title ID and update version whenever possible.
- Keeps the saved game library when scanning only the mounted game.
- Shows FPS options separately from visual/graphics patches.
- Supports 60 FPS target, unlocked FPS, 30 FPS test routes, native 60 FPS notes,
  update-required status, and alternative untested routes.
- Creates backups for supported patch operations.
- Uses Portuguese, English, or Spanish based on the PS3 system language.
- Includes a dependency check screen for webMAN/PS3MAPI/Artemis availability.

## Safety

PSUF is a foreground app. It does not install boot plugins and does not write to
`dev_flash`, `dev_blind`, LV1, LV2, console IDs, PSID, PSN data, or private
system files.

Patch results depend on the game region, game update, console setup, HEN/CFW
state, and the patch itself. Some routes are confirmed, some are experimental,
and some may only work after the game has finished loading.

Use the exact Title ID and update version whenever possible. Untested patches can
cause black screens, freezes, broken animations, wrong game speed, audio issues,
or no visible change.

## Source Layout

```text
include/              C headers
source/               PS3 app source
release/              files bundled into the PKG
release/USRDIR/       patch databases, tutorials, credits, QR images
tools/                database maintenance scripts
tests/                patch database validation tests
docs/                 architecture and technical notes
```

Important database files:

```text
release/USRDIR/patches.csv
release/USRDIR/graphics_patches.csv
release/USRDIR/fix_patches.csv
release/USRDIR/native60.csv
```

## Build With Docker

```sh
make -f Makefile.docker docker-pkg
```

The generated PKG is written to the project root as:

```text
ps3-fps-unlocker.pkg
```

## Build With Local PS3DEV

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
make pkg
```

## Database Tests

```sh
python -m unittest discover -s tests
```

These tests validate CSV shape, direct-write syntax, duplicate routes, packaged
database contents, and basic public-file privacy checks.

## Patch Row Format

`patches.csv` uses pipe-separated rows:

```text
title_id|version|kind|status|method|label|source|payload|note|delay_seconds
```

Direct memory writes use:

```text
0 ADDRESS VALUE
```

Multiple writes in one patch are separated with semicolons.

## Credits

PSUF depends on public PS3 homebrew work and community patch research. Credits
are kept in `release/USRDIR/CREDITS.txt` and in the source field of each patch
row whenever the original source is known.

Special thanks to the PSL1GHT/PS3DEV projects, webMAN MOD, ArtemisPS3, PS3MAPI
developers, PS3 Developer Wiki contributors, PSX-Place contributors, and every
tester who reports real console results.
