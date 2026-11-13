## What and why

## How it was checked

- [ ] `ninja -C build sims2hd` builds with no new warnings
- [ ] The unit tests pass; ran the game headless from the situations it touches (`--compose` shots for a picture change)
- [ ] Played it in the window, if it changes controls or the window

## Checklist

- [ ] Nothing of the game in the change: no ROM, save, save state or dump; screenshots only in
      `docs/screenshots`
- [ ] No personal path, host name or account detail
- [ ] No new dependency (or it was discussed in an issue first); melonDS unmodified (patches only in `patches/melonDS/`)
- [ ] CONSOLE is still the default layout
- [ ] `CHANGELOG.md` has a line under `## [Unreleased]` if players will notice the change;
      README.md, docs/manual.md, CLAUDE.md or docs/dev-notes.md updated if it changes what they say
