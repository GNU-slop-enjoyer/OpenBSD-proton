#!/bin/sh
# Compare two winetest-run.sh summary.txt files unit by unit.
#
# usage: scripts/winetest-compare.sh A.txt B.txt [all]
#
# Prints one line per unit present in both files whose status or failure
# count differs (or, with "all", every unit that is not a pass in both),
# then a status count for each file.  Lines starting with "#" are ignored,
# so docs/results/*.txt can be used directly.
set -eu

A=$1 B=$2 MODE=${3:-diff}

awk -v mode="$MODE" -v an="$(basename "$A")" -v bn="$(basename "$B")" '
/^#/ { next }
FNR == NR { st[$1 " " $2] = $3; fl[$1 " " $2] = $5; next }
{
	k = $1 " " $2
	if (!(k in st)) next
	both++
	if (mode == "all" ? (st[k] != "pass" || $3 != "pass") : (st[k] != $3 || fl[k] != $5))
		printf "%-16s %-24s %-8s %5s | %-8s %5s\n", $1, $2, st[k], fl[k], $3, $5
}
END { printf "%d units in both files (left: %s, right: %s)\n", both, an, bn }
' "$A" "$B"

for f in "$A" "$B"; do
	printf '%s: ' "$(basename "$f")"
	grep -v '^#' "$f" | awk '{n[$3]++} END {for (s in n) printf "%s=%d ", s, n[s]; print ""}'
done
