#!/bin/sh
# Run a ports target inside the VM, detached from the ssh session.
#
# usage: scripts/vm/port-build.sh start PKGPATH [TARGET]   (default: package)
#        scripts/vm/port-build.sh status PKGPATH           (tail + exit status)
#        scripts/vm/port-build.sh fetch-log PKGPATH        (copy log to logs/)
#
# The guest log is ~builder/build-logs/<pkgpath with / → _>.log; the last line
# is "exit=N" once the target has finished.
set -eu

TOP=$(cd "$(dirname "$0")/../.." && pwd)
RUN=$TOP/scripts/vm/run.sh
CMD=${1:-}
PKGPATH=${2:-}
TARGET=${3:-package}
[ -n "$CMD" ] && [ -n "$PKGPATH" ] || {
	echo "usage: $0 start|status|fetch-log PKGPATH [TARGET]" >&2
	exit 2
}
LOG="build-logs/$(echo "$PKGPATH" | tr / _).log"

case $CMD in
start)
	"$RUN" ssh "mkdir -p build-logs && cd /usr/ports/$PKGPATH &&
		nohup sh -c 'make $TARGET; echo exit=\$?' \
		    > \$HOME/$LOG 2>&1 < /dev/null &
		echo started $PKGPATH $TARGET"
	;;
status)
	"$RUN" ssh "tail -n 20 \$HOME/$LOG"
	;;
fetch-log)
	mkdir -p "$TOP/logs"
	"$RUN" ssh "cat \$HOME/$LOG" > "$TOP/logs/$(basename "$LOG")"
	echo "logs/$(basename "$LOG")"
	;;
*)
	echo "unknown command $CMD" >&2
	exit 2
	;;
esac
