# My Time - Technical Notes

`SNMYTIME` is a single-source-file module for The Major BBS v10. This
document describes how it hooks the host, where data lives, and the design
constraints behind those choices.

## Architecture overview

The module has no menu-driven user interface. It consists of:

1. a **global command handler** that fields `/time` and `/mytime` typed at
   any prompt on the board;
2. three **named text variables** (`MYTIME`, `MYT_BBS`, `MYT_OFS`) the host
   expands wherever they appear in message text;
3. a small **per-user record** in the host's general user database holding
   the offset;
4. a **per-channel cache** of the offsets of everyone currently online.

## Global command handling

`init__snmytime()` registers `glomyt()` with `globalcmd()`. The host calls
every registered global handler for each completed input line (unless the
user's `NOGLOB` flag is set) before the line reaches the current module.
The handler contract:

- return `0` - the line is not ours; the host tries the next handler;
- return `1` - the line was consumed; the host re-displays the prompt;
- return `-2` - consumed, and the host flushes output for us.

`glomyt()` follows the host's own paging handler (`/p`) pattern: it checks
`margv[0]` against its verbs, does its work, flushes with `outprf()`, and
returns `1`. Input typed before a caller is fully logged on
(`usrptr->usrcls != ACTUSR`) is never claimed, so the logon sequence is
unaffected.

## Time computation and formats

All times derive from the C runtime clock (`time()`), which is the same
clock the host displays. The user's view is `time() + offset*60` run
through `localtime()` and formatted `h:mm` with an `a`/`p` tail - `10:00a`,
`9:05p`, `12:00a` for midnight, `12:00p` for noon.

Offsets are stored in **minutes** (signed) so half-hour and
three-quarter-hour zones work; the command accepts `[+|-]H` and `[+|-]H:MM`
with hours 0-23 and minutes 00-59. `MYT_OFS` renders as `+2:00`, `-1:00`,
`+9:30`.

## Text variables

The host expands a text variable at message-emit time (`prfmsg()` /
menu display), in the context of the user the output is being built for,
by calling the registered routine and substituting its return string.
Each routine returns a small static buffer computed on the spot from the
per-channel cache, so the value is always current and always the right
user's - no pre-fill step is needed anywhere.

Names are global to the whole BBS, which is why the companions carry the
`MYT_` prefix. `MYTIME` itself is the product's promised name.

The module's own response messages embed these variables rather than
using positional `printf`-style arguments, so sysops can freely reword,
move, or repeat the values in any message with the CNF editor.

## Storage

One fixed-shape record per user in the host's **general user database**
(`wgsgen2.dat`, the same file the host's paging preferences live in),
accessed through the DFA API:

```
struct mytdisk {
    CHAR userid[UIDSIZ];    key part 1: user-id ("" = settings record)
    CHAR modnam[MNMSIZ];    key part 2: always "SNMYTIME"
    SHORT ofsmin;           user's offset in minutes
    SHORT auditflg;         settings record: log changes? 1/0
    CHAR resv[24];          reserved for future settings
};
```

- Records are keyed `userid + modnam`, so they can never collide with
  other facilities sharing the file, and the module ships **no Btrieve
  files** of its own.
- A user with no record is on BBS time (offset zero) - which is exactly
  the required default for new users, and means the file only grows with
  users who actually use the feature.
- Writes go through immediately when an offset changes (no save-at-logoff
  step to lose data in a crash or carrier drop).
- The delete-account hook removes the user's record.
- The module-wide settings live in one extra record under an **empty
  user-id**, which no real account can have.

## Per-channel cache lifecycle

`chanofs[]` (one `INT` per channel) shadows the stored offset of each
online user:

- **logon** - loaded from disk in the logon supplemental routine;
- **change** - updated in place when the user sets a new offset;
- **logoff and hangup** - cleared to zero, so a recycled channel can never
  display a previous occupant's time before the next logon reloads it.

Reads (the text variables, `/mytime` display) touch only the cache; disk
is involved only at logon and on change.

## Settings

One sysop setting: *log offset changes to the Audit Trail* (default YES).
The CNF option is the install default only - it seeds the live settings
record the first time the module ever loads. From then on the live value
is read at point of use and edited online with `/mytime audit on|off`
(requires the SYSOP key); a change applies to the very next offset change
and is itself always written to the Audit Trail.

## Build identification

The running build identifies itself in the Audit Trail at startup
("Loaded version ..."), and the DLL carries a full Windows version
resource. `MYT_VERSION` in `SNMYTIME.H` and the two version fields in
`SNMYTIME.RC` must move together on every release.

## Message file

`SRC/SNMYTIME.MSG` is the source of truth for all module text; it embeds
raw ESC color codes and 0x01-delimited text-variable tokens, so it must be
kept CRLF / CP437 byte-exact. Message numbers are positional — the
`#define`s in `SRC/MSGNUM.H` mirror the entry order — so the `.MSG` and
DLL always deploy together as a matched set, and new messages are appended
at the end so existing numbers never shift. The host compiles the `.MSG`
to `.MCV` itself at startup.
