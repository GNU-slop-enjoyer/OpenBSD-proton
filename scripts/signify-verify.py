#!/usr/bin/env python3
# Minimal signify(1) verifier for OpenBSD SHA256.sig files.
#
# Verifies an embedded-signature file ("signify -V -e") against an Ed25519
# public key in signify format, then checks the listed SHA256 digests of the
# requested files.  Used on non-OpenBSD hosts where signify is not installed.
#
# usage: signify-verify.py PUBKEY SHA256.sig DIR [FILE ...]
#   With no FILE arguments only the signature itself is checked.

import base64
import hashlib
import os
import re
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey


def read_b64_record(lines, what):
    if len(lines) < 2 or not lines[0].startswith("untrusted comment: "):
        sys.exit(f"{what}: missing untrusted comment line")
    raw = base64.b64decode(lines[1].strip(), validate=True)
    if raw[:2] != b"Ed":
        sys.exit(f"{what}: unsupported algorithm {raw[:2]!r}")
    return raw


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__ or "usage: signify-verify.py PUBKEY SHA256.sig DIR [FILE ...]")
    pubpath, sigpath, setdir, files = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]

    with open(pubpath) as f:
        pub = read_b64_record(f.read().splitlines(), pubpath)
    if len(pub) != 2 + 8 + 32:
        sys.exit(f"{pubpath}: bad public key length")

    with open(sigpath, "rb") as f:
        sigdata = f.read()
    text = sigdata.decode()
    lines = text.splitlines(keepends=True)
    sig = read_b64_record([l.rstrip("\n") for l in lines[:2]], sigpath)
    if len(sig) != 2 + 8 + 64:
        sys.exit(f"{sigpath}: bad signature length")
    if sig[2:10] != pub[2:10]:
        sys.exit("key number mismatch: signature was not made with this key")
    message = "".join(lines[2:]).encode()

    try:
        Ed25519PublicKey.from_public_bytes(pub[10:]).verify(sig[10:], message)
    except InvalidSignature:
        sys.exit("Signature verification FAILED")
    print("Signature Verified")

    digests = {}
    for line in message.decode().splitlines():
        m = re.fullmatch(r"SHA256 \((.+)\) = ([0-9a-f]{64})", line)
        if m:
            digests[m.group(1)] = m.group(2)

    failed = False
    for name in files:
        want = digests.get(name)
        if want is None:
            print(f"{name}: not listed in {sigpath}")
            failed = True
            continue
        h = hashlib.sha256()
        with open(os.path.join(setdir, name), "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        ok = h.hexdigest() == want
        failed |= not ok
        print(f"{name}: {'OK' if ok else 'FAIL'}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
