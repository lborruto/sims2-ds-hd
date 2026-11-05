## What and why

## How it was checked

- [ ] `ninja -C build sims2wide` builds with no new warnings
- [ ] Ran the game headless from the situations it touches (`--compose` shots for a layout change)
- [ ] Played it in the window, if it changes controls or the window

## Checklist

- [ ] Nothing of the game in the change: no ROM, save, save state or dump; screenshots only in
      `docs/screenshots`
- [ ] No personal path, host name or account detail
- [ ] No new dependency (or it was discussed in an issue first); melonDS unmodified
- [ ] CONSOLE is still the default layout
- [ ] `CHANGELOG.md` has a line under `## [Unreleased]` if players will notice the change;
      README.md updated if it changes what it says
