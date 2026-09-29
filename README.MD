# My Time

**My Time** (`SNMYTIME`) is a personal-clock module for **The Major BBS v10**. Every caller can set their own display-time offset from the BBS clock — so a user two time zones away sees *their* time, not the board's. The offset changes display only: the BBS's own timekeeping is never touched.

---

## Features

- **`/time` anywhere** — shows the actual BBS time from any prompt on the board.
- **`/mytime` anywhere** — shows the caller's personal time (BBS time plus their offset).
- **Per-user offsets** — each caller sets their own offset in hours (`/mytime +2`, `/mytime -1`), including half-hour zones (`/mytime +9:30`). Offsets run from `-23:59` to `+23:59`.
- **One-command reset** — `/mytime reset` puts the caller back on BBS time. New users are always on BBS time until they choose otherwise.
- **`MYTIME` text variable** — a named text variable any message on the board can embed. It renders the caller's time as `h:mm` with an `a`/`p` tail (`10:00a`, `9:05p`); with no offset set it shows BBS time. Companion variables `MYT_BBS` (the BBS clock in the same format) and `MYT_OFS` (the caller's offset, signed, e.g. `+2:00`) are also registered.
- **Audit Trail logging** — offset changes can be recorded in the Audit Trail; the sysop toggles this live with `/mytime audit on|off`, no restart needed.
- **Display only** — no engine timekeeping, accounting, or scheduling is affected in any way.

---

## Requirements

- The Major BBS v10

---

## Installation

1. Copy the contents of `DIST\1.0.0\` (`SNMYTIME.DLL`, `SNMYTIME.MDF`, `SNMYTIME.MSG`, `SNMYTIME.RLN`) into the BBS installation directory.
2. Restart the BBS. My Time announces its version in the Audit Trail at startup.

No menu entry is required — the module's commands are global. (Adding it to the menu tree is harmless: visiting it shows the caller their time and the usage summary.)

---

## Configuration

One option, editable two ways:

| Setting | Default | Live command |
|---|---|---|
| Log user offset changes to the Audit Trail | YES | `/mytime audit on` / `/mytime audit off` (Sysop) |

The CNF option is the **install default only** — it seeds the live setting the first time the module loads. After that, use the live command; changes apply immediately and are themselves always recorded in the Audit Trail.

---

## Usage

| Command | What it does |
|---|---|
| `/time` | Show the BBS clock |
| `/mytime` | Show your clock |
| `/mytime +2` | Run 2 hours ahead of the BBS |
| `/mytime -1` | Run 1 hour behind the BBS |
| `/mytime +9:30` | Half-hour zones work too |
| `/mytime reset` | Match the BBS clock again |
| `/mytime ?` | Usage summary |

Sysops can put the caller's clock into any message text on the board by embedding the `MYTIME` text variable.

---

## Version Information

Current version: **1.0.0** — see `CHANGELOG.MD` for the full history.

---

## Tips / Donations

- **Cash App:** [$laudenbachm](https://cash.app/$laudenbachm)
- **Ko-fi:** [ko-fi.com/laudenbachm](https://ko-fi.com/laudenbachm)

---

## License

MIT License — see `LICENSE`.

---

## Support

My Time is developed and maintained by Mark Laudenbach at Sysop Network.
