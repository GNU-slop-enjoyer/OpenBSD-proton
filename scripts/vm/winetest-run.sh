#!/bin/sh
# Run Wine conformance tests (built by winetest-build.sh) with an installed
# wine, one test unit per process so that a crash or hang only loses that
# unit.  Runs inside the VM as builder with an X server on $DISPLAY.
#
# usage: winetest-run.sh WINE OUTDIR test.exe ...
#   WINE    e.g. /usr/local/lib/proton-wine/bin/wine
#   OUTDIR  receives one log per unit and summary.txt
# Environment: WINEPREFIX (default ~/pfx-winetest), UNIT_TIMEOUT (seconds,
# default 300).
#
# summary.txt has one line per unit:
#   <exe> <unit> <status> <executed> <failures> <todo> <skipped> <rc> <seconds>
# status is pass, fail (failures > 0), crash (non-zero exit without a
# summary line), empty (exit 0 with no output at all), or timeout.
set -u

WINE=$1 OUTDIR=$2; shift 2
export WINEPREFIX=${WINEPREFIX:-$HOME/pfx-winetest}
export WINEDEBUG=${WINEDEBUG:--all}
TMO=${UNIT_TIMEOUT:-300}
mkdir -p "$OUTDIR"
: > "$OUTDIR/summary.txt"

[ -d "$WINEPREFIX" ] || "$WINE" wineboot -i > "$OUTDIR/wineboot.log" 2>&1

for exe; do
	name=$(basename "$exe" .exe)
	units=$("$WINE" "$exe" --list 2>/dev/null | tr -d '\r' |
	    sed -n 's/^    //p')
	if [ -z "$units" ]; then
		echo "$name - nolist 0 0 0 0" >> "$OUTDIR/summary.txt"
		continue
	fi
	for u in $units; do
		log=$OUTDIR/$name.$u.log
		start=$(date +%s)
		timeout -s KILL "$TMO" "$WINE" "$exe" "$u" > "$log" 2>&1
		rc=$?
		secs=$(( $(date +%s) - start ))
		# "0108:atom: 12 tests executed (0 marked as todo, 0 as flaky, 0 failures), 0 skipped."
		sum=$(tr -d '\r' < "$log" | sed -n \
		    's/^[0-9a-f]*:'"$u"': \([0-9]*\) tests executed (\([0-9]*\) marked as todo, [0-9]* as flaky, \([0-9]*\) failures*), \([0-9]*\) skipped.*/\1 \3 \2 \4/p' |
		    awk '{e+=$1; f+=$2; t+=$3; s+=$4} END {if (NR) print e, f, t, s}')
		if [ $rc -eq 137 ]; then
			st=timeout
		elif [ ! -s "$log" ] && [ $rc -eq 0 ]; then
			st=empty; sum="0 0 0 0"
		elif [ -z "$sum" ]; then
			st=crash; sum="0 0 0 0"
		elif [ "$(echo "$sum" | cut -d' ' -f2)" -gt 0 ] || [ $rc -ne 0 ]; then
			st=fail
		else
			st=pass
		fi
		echo "$name $u $st ${sum:-0 0 0 0} $rc $secs" >> "$OUTDIR/summary.txt"
	done
	"$WINE"server -k 2>/dev/null
done
awk '{n[$3]++} END {for (s in n) printf "%s=%d ", s, n[s]; print ""}' \
    "$OUTDIR/summary.txt"
