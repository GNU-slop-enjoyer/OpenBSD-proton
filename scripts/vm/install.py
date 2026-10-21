#!/usr/bin/env python3
# Unattended OpenBSD install into a QEMU/KVM disk image, run as an
# unprivileged user.
#
# Boots cdNN.iso, switches the boot loader to the serial console by typing
# into the emulated keyboard through the QEMU monitor, selects autoinstall(8)
# and points it at a response file served, together with the verified sets,
# by a loopback-only HTTP server.  QEMU user networking exposes the host as
# 10.0.2.2.  The installer verifies SHA256.sig itself with signify(1).
#
# usage: scripts/vm/install.py [--release 8.0] [--disk-size 80G]

import argparse
import functools
import http.server
import os
import re
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import time

TOP = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VMDIR = os.path.join(TOP, "vm")


def monitor_cmd(path, cmd):
    s = socket.socket(socket.AF_UNIX)
    s.connect(path)
    s.settimeout(2)
    try:
        s.recv(4096)  # banner
    except socket.timeout:
        pass
    s.sendall(cmd.encode() + b"\n")
    time.sleep(0.1)
    s.close()


KEYNAMES = {" ": "spc", "\n": "ret", ".": "dot", "/": "slash", "-": "minus"}


def type_keys(path, text):
    for ch in text:
        monitor_cmd(path, "sendkey " + KEYNAMES.get(ch, ch))
        time.sleep(0.05)


def wait_for(path, timeout):
    end = time.time() + timeout
    while not os.path.exists(path):
        if time.time() > end:
            sys.exit(f"timed out waiting for {path}")
        time.sleep(0.1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", default="8.0")
    ap.add_argument("--arch", default="amd64")
    ap.add_argument("--disk-size", default="80G")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--timeout", type=int, default=3600)
    args = ap.parse_args()

    ver = args.release.replace(".", "")
    setdir = os.path.join(TOP, "openbsd-sets", args.release, args.arch)
    iso = os.path.join(setdir, f"cd{ver}.iso")
    disk = os.path.join(VMDIR, "openbsd.qcow2")
    os.makedirs(VMDIR, exist_ok=True)
    os.makedirs(os.path.join(TOP, "logs"), exist_ok=True)
    if os.path.exists(disk):
        sys.exit(f"{disk} exists; move it aside to reinstall")

    pwfile = os.path.join(VMDIR, "root-password")
    if not os.path.exists(pwfile):
        with open(os.open(pwfile, os.O_WRONLY | os.O_CREAT, 0o600), "w") as f:
            f.write(secrets.token_urlsafe(18) + "\n")
    with open(pwfile) as f:
        rootpw = f.read().strip()
    key = os.path.join(VMDIR, "id_ed25519")
    if not os.path.exists(key):
        subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "",
                        "-C", "openbsd-proton-vm", "-f", key], check=True)
    with open(key + ".pub") as f:
        sshkey = f.read().strip()

    # Serve a private directory: the response file plus a link to the sets.
    www = tempfile.mkdtemp(prefix="obsd-www-", dir=VMDIR)
    os.symlink(os.path.join(TOP, "openbsd-sets"), os.path.join(www, "pub"))
    with open(os.path.join(os.path.dirname(__file__), "install.conf.in")) as f:
        conf = f.read()
    for k, v in {"ROOTPW": rootpw, "SSHKEY": sshkey,
                 "HTTPSERVER": f"10.0.2.2:{args.port}",
                 "SETDIR": f"pub/{args.release}/{args.arch}"}.items():
        conf = conf.replace(f"@{k}@", v)
    with open(os.open(os.path.join(www, "install.conf"),
                      os.O_WRONLY | os.O_CREAT, 0o600), "w") as f:
        f.write(conf)
    handler = functools.partial(http.server.SimpleHTTPRequestHandler,
                                directory=www)
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    subprocess.run(["qemu-img", "create", "-f", "qcow2", disk, args.disk_size],
                   check=True)
    mon = os.path.join(VMDIR, "install-monitor.sock")
    ser = os.path.join(VMDIR, "install-serial.sock")
    for p in (mon, ser):
        if os.path.exists(p):
            os.unlink(p)
    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-machine", "q35,accel=kvm", "-cpu", "host",
        "-smp", "4", "-m", "2048", "-display", "none", "-no-reboot",
        "-drive", f"file={disk},if=virtio,format=qcow2",
        "-cdrom", iso, "-boot", "d",
        "-netdev", "user,id=n0", "-device", "virtio-net-pci,netdev=n0",
        "-monitor", f"unix:{mon},server,nowait",
        "-serial", f"unix:{ser},server,nowait",
    ])
    wait_for(ser, 10)
    con = socket.socket(socket.AF_UNIX)
    con.connect(ser)
    con.settimeout(1)

    # boot(8) waits 5 seconds at its prompt on the emulated VGA console.
    time.sleep(2.5)
    type_keys(mon, "set tty com0\n")
    # After "set tty com0" boot(8) reads its input from the serial line.

    log = open(os.path.join(TOP, "logs", "vm-install.log"), "wb")
    answers = [
        (rb"boot> $", b"boot\n"),
        (rb"\(A\)utoinstall or \(S\)hell\? $", b"a\n"),
        (rb"Response file location\?.*\] $",
         f"http://10.0.2.2:{args.port}/install.conf\n".encode()),
    ]
    buf = b""
    deadline = time.time() + args.timeout
    while qemu.poll() is None:
        if time.time() > deadline:
            qemu.kill()
            sys.exit("install timed out; see logs/vm-install.log")
        try:
            data = con.recv(4096)
        except socket.timeout:
            data = b""
        except OSError:
            break
        if data:
            log.write(data)
            log.flush()
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
            buf = (buf + data)[-2048:]
        for pat, resp in answers:
            if re.search(pat, buf):
                con.sendall(resp)
                buf = b""
                break
    qemu.wait()
    httpd.shutdown()
    with open(os.path.join(TOP, "logs", "vm-install.log"), "rb") as f:
        tail = f.read()[-4096:]
    if b"CONGRATULATIONS" not in tail:
        sys.exit("installer did not report success; see logs/vm-install.log")
    print("\ninstall complete:", disk)


if __name__ == "__main__":
    main()
