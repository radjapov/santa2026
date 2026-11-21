#!/usr/bin/env bash
# Repeat LNS-harvest + CP-SAT recombination passes on the focus puzzles until stopped (touch work/STOP to stop).
# Each pass also starts LNS from up to 2 distinct public variants per puzzle and adds public program recipes to CP-SAT.
cd "$(dirname "$0")/.."
for i in $(seq 1 1000); do
  [ -e work/STOP ] && { echo "stop file found"; break; }
  .venv/bin/python src/recomb_pass.py --only work/focus_ids.txt --time 15 --cp 25 --jobs 19 --bin bin/elfopt8 \
      --extra "--hall 1" --tag loop2 --banks "outs*/*/*.csv" --variants 2
done
