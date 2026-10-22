# Using OpenBSD Proton

For building the packages see [BUILDING.md](BUILDING.md); for what has
actually been tested see [TESTING.md](TESTING.md).

## Requirements

- OpenBSD 8.0, amd64 (the ports were developed against the 8.0 ports tree).
- X11 (Xenocara) and a window manager; cwm, OpenBSD's default, is tested.
- `/usr/local` on a filesystem mounted `wxallowed` (the default layout of an
  OpenBSD install): wine needs W|X mappings.
- For DXVK (Direct3D 9–11) and vkd3d-proton (Direct3D 12): a GPU with a Mesa
  Vulkan driver (Intel anv or AMD radv). Without one, use
  `PROTON_USE_WINED3D=1` (Direct3D through OpenGL).
- For sound: PulseAudio (installed as a dependency; it starts on demand and
  plays through sndiod).
- 64-bit Windows programs only.

## Installing

The ports are not in the OpenBSD ports tree. Copy them into it (for example
under `/usr/ports/mystuff`) and build packages as usual:

    for p in devel/mingw-w64 graphics/dxvk graphics/vkd3d-proton \
        emulators/proton-wine emulators/proton-wine-addons \
        fonts/proton-fonts emulators/proton-openbsd; do
            (cd /usr/ports/mystuff/$p && make package)
    done
    doas env TRUSTED_PKG_PATH=/usr/ports/packages/amd64/all \
        pkg_add proton-openbsd proton-wine-addons

(`PACKAGE_REPOSITORY` decides where the packages land; locally built
packages are unsigned, hence `TRUSTED_PKG_PATH`.) proton-wine alone takes
about two hours to build on six cores.

## Running a program

    proton-openbsd run ./Program.exe [arguments]
    proton-openbsd msiexec /i Setup.msi

The first run creates the prefix (`~/.local/share/proton-openbsd/pfx`, or
`PROTON_PREFIX`), installs Wine Mono and Gecko, links Proton's fonts and puts
DXVK and vkd3d-proton into it; that takes a few minutes. `proton-openbsd(1)`
lists the environment variables; the most useful ones:

| Variable | Effect |
|---|---|
| `PROTON_PREFIX` | prefix directory |
| `PROTON_USE_WINED3D=1` | Direct3D through OpenGL instead of DXVK/vkd3d-proton (no Vulkan GPU) |
| `PROTON_NO_FSYNC=1` | turn off fsync (in-process synchronization) |
| `WINE_PRECISE_SLEEP=1` | precise Sleep() for all programs (costs CPU; by default only for programs that call timeBeginPeriod) |
| `PROTON_LOG=1` | log to `$HOME/proton-<program>.log` |

## Common problems

- "32-bit program": the launcher refuses 32-bit executables. Look for a
  64-bit `.msi` or portable archive of the program.
- Non-ASCII file names: the launcher switches to a UTF-8 locale if yours
  is not; plain `wine` needs `LC_CTYPE=C.UTF-8` for them.
- No sound: `pactl info` should show a default sink; check that sndiod runs.
- Direct3D programs fail to start without a GPU: try `PROTON_USE_WINED3D=1`.
- Steam and programs with anti-cheat do not work.
