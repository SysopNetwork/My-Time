# Changelog

All notable changes to My Time are documented here.

## 1.0.0 - 2026-07-29

First public release. Functionally identical to 0.1.2 after test-cycle
sign-off.

## 0.1.2 - 2026-07-29

### Changed
- The startup announcement is now two Audit Trail entries, each on its own
  time-stamped line: "My Time vX.X.X" followed by "By SysopNetwork.com".

## 0.1.1 - 2026-07-29

### Changed
- CNF editor section titles are now just "My Time" (previously
  "My Time: configurable options" / "My Time: module text").
- Startup Audit Trail entry now reads "My Time vX.X.X" with
  "By SysopNetwork.com" as the detail line.

## 0.1.0 - 2026-07-29

First test build.

### Added
- `/time` global command: shows the actual BBS time from anywhere on the board.
- `/mytime` global command: shows the caller's personal time (BBS time plus
  their stored offset).
- `/mytime +H`, `/mytime -H`, `/mytime +H:MM` set a personal offset
  (range -23:59 to +23:59, half-hour zones supported); `/mytime reset`
  (or an offset of 0) returns the caller to BBS time; `/mytime ?` shows usage.
- Named text variables `MYTIME` (caller's time, `h:mm` + `a`/`p`),
  `MYT_BBS` (BBS time, same format) and `MYT_OFS` (caller's offset, signed).
- Per-user offset storage in the host's general user database, keyed by
  user-id + module name. New users default to BBS time; offsets are written
  through to disk the moment they change. Deleting an account removes its
  stored offset.
- Sysop setting: log offset changes to the Audit Trail (install default from
  CNF; edited live with `/mytime audit on|off`, effective immediately,
  setting changes always audited).
- Version announcement in the Audit Trail at startup; Windows version
  resource on the DLL.
