#!/usr/bin/env bash
# Cycle Wang Jakie's strategies (and our seeded variants) over the focus puzzles, with fresh seeds each time.
# Stop: touch work/STOP_WJ
cd "$(dirname "$0")/.."
G='outs*/*/*.csv'
for it in $(seq 1 100); do
  for spec in "anneal_seeded 700 seeds" "anneal 1000 -" "incremental_seeded 700 seeds" "compact_rotate 300 -" "anneal 700 -"; do
    [ -e work/STOP_WJ ] && exit 0
    set -- $spec
    if [ "$3" = "seeds" ]; then
      .venv/bin/python src/wj_driver.py --strategy "$1" --rounds "$2" --only work/focus_ids.txt --jobs 19 --batch 4 --tag wj --seeds-glob "$G"
    else
      .venv/bin/python src/wj_driver.py --strategy "$1" --rounds "$2" --only work/focus_ids.txt --jobs 19 --batch 4 --tag wj
    fi
  done
done
