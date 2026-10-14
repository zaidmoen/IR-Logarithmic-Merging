#!/usr/bin/env bash
# run_experiments.sh — PROVIDED. Runs the experiments required for the report
# and writes results/dynamic.csv and results/trace_*.csv.
#   usage: scripts/run_experiments.sh [corpus.txt]   (run from the project root)
set -euo pipefail
CORPUS=${1:-data/corpus_50k.txt}
B=build
OUT=results
WORK=$(mktemp -d)
mkdir -p "$OUT"

if [ ! -f "$CORPUS" ]; then
  echo "generating $CORPUS"
  $B/gen_corpus "$CORPUS" 50000
fi

echo "mode,n,flushes,merges,max_disk_indexes,postings_written,postings_written_with_finalize,write_amplification,seconds" > $OUT/dynamic.csv
for MODE in aux log; do
  for N in 50000 100000 200000 400000; do
    R=$($B/build_dynamic $MODE "$CORPUS" "$WORK/dyn" $N "$WORK/final_dyn" --trace $OUT/trace_${MODE}_$N.csv)
    g() { echo "$R" | grep "^$1" | awk '{print $NF}'; }
    W=$(echo "$R" | grep '^postings_written' | awk '{print $2}')
    WF=$(echo "$R" | grep '^postings_written' | tr -d ')' | awk '{print $NF}')
    M=$(echo "$R" | grep '^merges' | awk '{print $NF}')
    echo "$MODE,$N,$(g flushes),$M,$(g max_disk_indexes),$W,$WF,$(g write_amplification),$(g seconds)" >> $OUT/dynamic.csv
    $B/verify "$CORPUS" "$WORK/final_dyn" > /dev/null || { echo "$MODE index wrong for n=$N"; exit 1; }
  done
done
rm -rf "$WORK"
echo "done:"; cat $OUT/dynamic.csv
