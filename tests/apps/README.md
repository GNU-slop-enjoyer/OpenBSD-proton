# Real-application tests

Third-party Windows binaries used for testing (approved by the project
owner 2026-10-21). They are run only inside the OpenBSD VM, never on the
host, and are not committed; fetch them from the URLs below and check the
hashes.

| Program | File | Source | sha256 | Verification |
|---|---|---|---|---|
| 7-Zip 23.01 x64 (installer) | `7z2301-x64.exe` | https://www.7-zip.org/a/ | `26cb6e9f56333682122fafe79dbcdfd51e9f47cc7217dccd29ac6fc33b5598cd` | none published (HTTPS only); **32-bit installer stub** |
| 7-Zip 23.01 x64 (MSI) | `7z2301-x64.msi` | https://www.7-zip.org/a/ | `0ba639b6dacdf573d847c911bd147c6384381a54dac082b1e8c77bc73d58958b` | none published (HTTPS only) |
| Notepad++ 8.9.8.1 x64 portable | `npp.8.9.8.1.portable.x64.zip` | GitHub release v8.9.8.1 | `beddf5548e75c97930d1575ba1c7d113e6e2b33d0a200310e734ca08e225f414` | GitHub digest + GPG (key 14BCE4362749B2B51F8C71226C429F1D8D84F46E) good |
| Notepad++ 8.9.8.1 x64 installer | `npp.8.9.8.1.Installer.x64.exe` | GitHub release v8.9.8.1 | `26f3bcced788fadbac4e7e577b430ea2ce6060cf3bcad993ef4612ecd7d743da` | GitHub digest + GPG good; **32-bit installer stub** |

| OpenTTD 15.3 win64 | `openttd-15.3-windows-win64.zip` | https://cdn.openttd.org/openttd-releases/15.3/ | `61c0a6a43d81008c7ff4330fb56351bbf66a980cdad041f1d0e08b51f2eeb34c` | matches the release `manifest.yaml` |
| OpenGFX 8.0 (base graphics for OpenTTD) | `opengfx-8.0-all.zip` | https://cdn.openttd.org/opengfx-releases/8.0/ | `43a0c1dabf39cb865394f3a6cc36d4da5c10ecfaaf55652043104806810903be` | matches `manifest.yaml` |
| SuperTuxKart 1.5 (Windows) | `SuperTuxKart-1.5-win.zip` | GitHub release 1.5 | `9df7e2d67e8562127a3bb633030f1bd4ee77fa9e7f0ae110897473025af8acdc` | GitHub digest |

## How they were run (proton-openbsd 0.1p4, proton-wine p2, VM, Xvfb :1)

    proton-openbsd msiexec /i 7z2301-x64.msi /qn INSTALLDIR=C:\7zip
    proton-openbsd run 'C:\7zip\7z.exe' a -t7z test.7z src   # then t, x; cmp
    proton-openbsd run ./notepad++.exe test.txt                # screenshot via xwd
    proton-openbsd run 7z2301-x64.exe                          # refused: 32-bit

    proton-openbsd run ./openttd.exe              # opengfx tar in baseset/
    proton-openbsd run ./supertuxkart.exe --race-now --track=lighthouse \
        --kart=tux --numkarts=1 --laps=1          # from build-x86_64/bin

Input was driven with `xdotool` (mouse clicks; `keydown Up` with the
pointer over the window — no window manager, so focus follows the
pointer). Screenshots: `docs/screenshots/`.

`scripts/debug/xwd2png.py` converts `xwd -root` screenshots for viewing.
