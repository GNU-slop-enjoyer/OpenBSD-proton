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

On the OpenBSD machine, as your normal user (not root):

    doas pkg_add git            # or: ftp https://github.com/GNU-slop-enjoyer/OpenBSD-proton/archive/main.tar.gz
    git clone https://github.com/GNU-slop-enjoyer/OpenBSD-proton
    cd OpenBSD-proton
    make

`make` runs `scripts/openbsd-proton-setup`, which:

1. checks the system and stops with an explanation if something is
   missing: OpenBSD 8.0/amd64, doas set up, a `wxallowed` filesystem with
   about 15 GB free for the build, `/usr/local` `wxallowed`, the memory
   limit of your login class, sndiod, X, the GPU driver (amdgpu → radv,
   inteldrm → anv);
2. if your login class allows less than 8 GB per process, adds a login
   class `proton` (datasize unlimited, more files) to `/etc/login.conf` and
   moves you into it — log out and in once afterwards;
3. fetches the 8.0 ports tree into `/usr/ports` if it is missing (checked
   with signify);
4. installs the build dependencies from the 8.0 packages;
5. builds the ports in this repository in order (mingw-w64, DXVK,
   vkd3d-proton, proton-wine, Wine Mono/Gecko, fonts, proton-openbsd,
   proton-launcher) and installs each package;
6. builds a test program and runs it, and checks that proton-launcher
   reaches Steam.

Every root step is printed with the reason and asked for first (`-y`
skips the questions). `make dry-run` shows what would happen, `make check`
only runs the checks and tests, `make update` pulls and rebuilds what
changed, `make uninstall` removes the packages (games and prefixes stay).
Logs: `~/.cache/openbsd-proton/`. The first build takes one to two hours
(wine is most of it).

The packages are ordinary unsigned packages built on your machine; the
build area is `openbsd-proton/` on the largest `wxallowed` filesystem.

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

## Steam games and your own games: proton-launcher

`proton-launcher` logs in to Steam, downloads the Windows games your
account owns from Steam's servers and starts them with `proton-openbsd`.
It is a native OpenBSD program; Valve's Steam client is not used or
installed.

    proton-launcher login            # password, then the Steam Guard code or app approval
    proton-launcher library          # your Windows games and their app IDs
    proton-launcher install 1234560  # download (re-run to update or resume)
    proton-launcher run 1234560

    proton-launcher add Mygame ~/games/mygame/Mygame.exe
    proton-launcher run Mygame

    proton-launcher                  # menu: installed games, log in, install, add

Each game gets its own wine prefix under
`~/.local/share/proton-launcher/prefixes/`. The login token Steam returns is
kept in `~/.local/share/proton-launcher/login` (mode 0600); the password
is never stored. `proton-launcher logout` deletes the token.

What to expect: games that do not need a running Steam client work like
any other program. Many games call the Steamworks API at start
(`steam_api64.dll`) and expect the Steam client to be running; without it
they usually quit at once or show an error. There is no way around that
without running Valve's client or a Steam emulator, and neither is done
here. Anti-cheat does not work.
`proton-launcher info APPID` shows which depots would be downloaded; a
game whose Windows depots are all 32-bit cannot run on OpenBSD.

## Common problems

- "32-bit program": the launcher refuses 32-bit executables. Look for a
  64-bit `.msi` or portable archive of the program.
- Non-ASCII file names: the launcher switches to a UTF-8 locale if yours
  is not; plain `wine` needs `LC_CTYPE=C.UTF-8` for them.
- No sound: `pactl info` should show a default sink; check that sndiod runs.
- Direct3D programs fail to start without a GPU: try `PROTON_USE_WINED3D=1`.
- Steam and programs with anti-cheat do not work.
