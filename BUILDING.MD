# Building My Time

My Time is a Win32 DLL module for The Major BBS v10, built with Microsoft
Visual Studio against the MBBS v10 Module SDK.

## Prerequisites

- **Microsoft Visual Studio 2022** with the *Desktop development with C++*
  workload (the Build Tools edition is sufficient for command-line builds).
- **Platform toolset v143** (ships with Visual Studio 2022).
- **Windows 10 SDK** (`WindowsTargetPlatformVersion` 10.0).
- **The Major BBS v10 Module SDK** — provides the headers (`inc/`) and import
  libraries (`lib/importlibs`, `lib/v10`) the module links against.
- A Major BBS v10 test system for deployment and runtime testing.

## Platform / architecture

> **Build for x86 (Win32) only. Never x64.**
> The Major BBS v10 host process is 32-bit and will not load a 64-bit module.

## SDK location

The SDK location is supplied by a local `MBBSSDK.props` file, which is not
committed. Copy `MBBSSDK.props.example` to `MBBSSDK.props` and set the path
to your SDK (trailing backslash required).

## Build steps

1. Open `SNMYTIME.sln` in Visual Studio 2022.
2. Set the active configuration to **Release** and the platform to **Win32**.
3. Build the solution. The DLL is produced in `BUILD/Release/`.

Or from a command prompt:

```
MSBuild.exe SNMYTIME.sln -p:Configuration=Release -p:Platform=Win32
```

## Message file

`SRC/SNMYTIME.MSG` is the source of truth for all module text. It must keep
CRLF line endings and CP437 encoding (enforced by `.gitattributes`); the
host compiles it to `.MCV` itself at startup, so no `.MCV` ships.

Message numbers are positional: the `#define`s in `SRC/MSGNUM.H` must match
the `.MSG` entry order exactly. New messages are appended at the END so
existing numbers never shift, and the `.MSG` and DLL always deploy together
as a matched set.

No `.DAT` or `.VIR` files are shipped: per-user data lives in the host's
general user database.

## Packaging a release

1. Build the **Release** configuration.
2. Write the release notes: `DIST/<version>/SNMYTIME.RLN` (plain ASCII,
   CRLF). The RLN is **required**: the MDF's `Online user manual:` line
   references `SNMYTIME.RLN`, so it deploys alongside the DLL/MDF/MSG and
   sysops can read it from the online manual facility.
3. Copy the deployables (`SNMYTIME.DLL`, `SNMYTIME.MDF`, `SNMYTIME.MSG`,
   all UPPERCASE) into `DIST/<version>/` and zip the four files as
   `My-Time-<version>.zip`. Nothing else goes in the folder — no source,
   project, or build files.
4. Confirm the DLL carries correct Windows version-resource metadata
   (product name, version, copyright Sysop Network).
5. Never repackage a version that has been shared: a version number names
   exactly one binary — bump the version instead.

## Deploying to a test system

1. Copy the contents of `DIST/<version>/` (or unpack the zip) into the Major
   BBS v10 installation directory. (The host locks the DLL while running —
   deploy with the BBS shut down.)
2. Start the system. The Audit Trail shows a "My Time" entry naming the
   loaded version. No menu entry is needed; `/time` and `/mytime` work at
   any prompt.
