#!/bin/sh
# Manage the OpenBSD build/test VM created by scripts/vm/install.py.
#
# usage: scripts/vm/run.sh start|stop|status|ssh [command ...]
#
# The VM runs headless under QEMU/KVM as the invoking user.  Its serial
# console is logged to logs/vm-console.log and sshd is forwarded to
# 127.0.0.1:$VM_SSH_PORT (default 2222).  VM_MEM and VM_CPUS size the guest;
# VM_DISK selects another image (for example a clean copy for installer
# tests), which then gets its own pid file, monitor socket and console log.
set -eu

TOP=$(cd "$(dirname "$0")/../.." && pwd)
VMDIR=$TOP/vm
DISK=${VM_DISK:-$VMDIR/openbsd.qcow2}
NAME=$(basename "$DISK" .qcow2)
if [ "$NAME" = openbsd ]; then
	PIDFILE=$VMDIR/qemu.pid
	MONITOR=$VMDIR/monitor.sock
	CONSOLE=$TOP/logs/vm-console.log
else
	PIDFILE=$VMDIR/qemu-$NAME.pid
	MONITOR=$VMDIR/monitor-$NAME.sock
	CONSOLE=$TOP/logs/vm-console-$NAME.log
fi
PORT=${VM_SSH_PORT:-2222}
MEM=${VM_MEM:-4096}
CPUS=${VM_CPUS:-6}

vm_ssh() {
	ssh -i "$VMDIR/id_ed25519" -p "$PORT" \
	    -o UserKnownHostsFile="$VMDIR/known_hosts" \
	    -o StrictHostKeyChecking=accept-new \
	    -o ConnectTimeout=5 -o BatchMode=yes \
	    "${VM_USER:-builder}@127.0.0.1" "$@"
}

running() {
	[ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null
}

case ${1:-} in
start)
	if running; then echo "already running"; exit 0; fi
	mkdir -p "$TOP/logs"
	qemu-system-x86_64 -machine q35,accel=kvm -cpu host \
	    -smp "$CPUS" -m "$MEM" -display none -daemonize \
	    -pidfile "$PIDFILE" \
	    -drive "file=$DISK,if=virtio,format=qcow2" \
	    -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$PORT-:22" \
	    -device virtio-net-pci,netdev=n0 \
	    -monitor "unix:$MONITOR,server,nowait" \
	    -serial "file:$CONSOLE"
	i=0
	until vm_ssh true 2>/dev/null; do
		i=$((i + 1))
		if [ $i -gt 60 ]; then
			echo "sshd not reachable; see $CONSOLE" >&2
			exit 1
		fi
		sleep 3
	done
	echo "VM up (ssh port $PORT)"
	;;
stop)
	if running; then
		VM_USER=root vm_ssh shutdown -p now || true
		i=0
		while running && [ $i -lt 60 ]; do sleep 1; i=$((i + 1)); done
		running && kill "$(cat "$PIDFILE")"
	fi
	rm -f "$PIDFILE"
	;;
status)
	if running; then echo running; else echo stopped; fi
	;;
ssh)
	shift
	vm_ssh "$@"
	;;
*)
	echo "usage: $0 start|stop|status|ssh [command ...]" >&2
	exit 2
	;;
esac
