# Security policy

## Reporting a vulnerability

Please report privately through GitHub: **Security › Report a vulnerability**
(https://github.com/lborruto/sims2-ds-wide/security/advisories/new). Do not open a public
issue. Include the version (the release tag or the commit), what an attacker needs (a file you
open, code running as you) and the steps. Never attach a ROM or anything from the game: describe
it instead. sims2-ds-wide is maintained in spare time: reports are handled on a best-effort
basis, with no guaranteed response time.

## Supported versions

The latest release only: the most recent `vX.Y.Z` tag and its downloads on the
[releases page](https://github.com/lborruto/sims2-ds-wide/releases). Fixes land on `main` and in
the next release; older tags are not patched. Each download comes with a build provenance
attestation: `gh attestation verify <file> --repo lborruto/sims2-ds-wide`.

## Security model

sims2-ds-wide is a local program: the melonDS emulator core and a window. It is not affiliated
with, endorsed by or supported by Electronic Arts, Maxis, Nintendo or the melonDS team.

- **No network at all.** It opens no port and makes no connection: no update check, no
  telemetry. The DS's Wi-Fi and wireless play are stubs that send and receive nothing.
- **It reads what you give it.** The ROM on the command line or chosen in the file picker, its save (`.sav` beside it, or
  `--save`), a save state (`--state-load`, F7) and an input script (`--input`). melonDS parses
  the ROM, the save and the save state: treat ones from other people as untrusted files, as you
  would any file an emulator opens.
- **It writes in a few known places.** Beside the ROM: the game's save (or `--save`),
  `<rom>.quick.st` (F5) and `<rom>-shot.png` (F12). In its per-user settings folder:
  `last-rom.txt`, the path of the ROM chosen last time. Where you say: the `--state-save` file and
  the `--shot-dir` folder (default `shots`, in the folder you run it from). Nothing else.
- **Not signed by Apple or Microsoft.** The Mac app is ad-hoc signed and not notarised, the
  Windows program not signed: the systems warn the first time. Check a download with
  `gh attestation verify <archive> --repo lborruto/sims2-ds-wide` instead.
- **No microphone or camera.** They are stubs; nothing is recorded.

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
