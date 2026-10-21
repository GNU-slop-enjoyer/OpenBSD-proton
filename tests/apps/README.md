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

## How they were run (proton-openbsd 0.1p4, proton-wine p2, VM, Xvfb :1)

    proton-openbsd msiexec /i 7z2301-x64.msi /qn INSTALLDIR=C:\7zip
    proton-openbsd run 'C:\7zip\7z.exe' a -t7z test.7z src   # then t, x; cmp
    proton-openbsd run ./notepad++.exe test.txt                # screenshot via xwd
    proton-openbsd run 7z2301-x64.exe                          # refused: 32-bit

`scripts/debug/xwd2png.py` converts `xwd -root` screenshots for viewing.
