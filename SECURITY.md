# Security policy

## Reporting a vulnerability

Please report privately through GitHub: **Security › Report a vulnerability**
(https://github.com/lborruto/sims2-ds-hd/security/advisories/new). Do not open a public
issue. Include the version (the release tag or the commit), what an attacker needs (a file you
open, code running as you) and the steps. Never attach a ROM or anything from the game: describe
it instead. sims2-ds-hd is maintained in spare time: reports are handled on a best-effort
basis, with no guaranteed response time.

## Supported versions

The latest release only: the most recent `vX.Y.Z` tag and its downloads on the
[releases page](https://github.com/lborruto/sims2-ds-hd/releases). Fixes land on `main` and in
the next release; older tags are not patched. Each download comes with a build provenance
attestation once the repository is public: `gh attestation verify <file> --repo lborruto/sims2-ds-hd`.

## Security model

sims2-ds-hd is a local program: the melonDS emulator core and a window. It is not affiliated
with, endorsed by or supported by Electronic Arts, Maxis, Nintendo or the melonDS team.

- **No network at all.** It opens no port and makes no connection: no update check, no
  telemetry. The DS's Wi-Fi and wireless play are stubs that send and receive nothing.
- **It reads what you give it.** The ROM on the command line, chosen in the file picker or
  dropped on the window (and the recent ones it lists), its save (`.sav` beside it, or `--save`),
  save states (the pause menu's, F7, Continue, `--state-load`), an input script (`--input`) and
  your own `gamecontrollerdb.txt`. melonDS parses the ROM, the save and the save states: treat
  ones from other people as untrusted files, as you would any file an emulator opens.
- **It writes in a few known places.** Beside the ROM: the game's save (or `--save`) and its
  `backups/`. In its per-user settings folder (`%APPDATA%\sims2-ds-hd`, `~/Library/Application
  Support/sims2-ds-hd` or `~/.local/share/sims2-ds-hd`): `settings.ini`, `recent.txt` and
  `last-rom.txt` (the paths of your recent ROMs and of the file picker's folder), `states/` (save states, their pictures, the place kept on quitting),
  `screenshots/` (F12), `saves/` (the game's save when the ROM's folder cannot be written) and
  `captures/`, a picture and a save state of each screen the game shows that the program does not
  know yet (at most 200). Where you say: the `--state-save` file, the `--shot-dir` folder and
  `--data-dir`. Nothing else.
- **Not signed by Apple or Microsoft.** The Mac app is ad-hoc signed and not notarised, the
  Windows program not signed: the systems warn the first time. Check a download with
  `gh attestation verify <archive> --repo lborruto/sims2-ds-hd` instead.
- **The microphone only if you ask.** By default the DS hears a key (the blow key) or nothing;
  your real microphone is opened only with the Microphone setting (or `--mic real`) and only while
  the game listens (the vacuum, the music mixer). Nothing is recorded or kept. The camera is a stub.

### What it does not protect against

- A crafted ROM, save or save state that exploits a bug in the melonDS core. Open only your own.
- Malware or any program running as your user: it can change the program, the ROM or the save.
- Losing your save: keep a copy before you try a new version.

## In scope

The code in this repository (`src/`, `tools/`, `scripts/`), the build files, the GitHub
workflows and the release downloads.

## Out of scope

- Bugs in the melonDS core itself: report them to [melonDS](https://github.com/melonDS-emu/melonDS).
- SDL2: report to [SDL](https://github.com/libsdl-org/SDL).
- The game.
- Attacks that need code execution as your user, or physical access to your unlocked machine.
- A build distributed from somewhere else than this repository's releases.
