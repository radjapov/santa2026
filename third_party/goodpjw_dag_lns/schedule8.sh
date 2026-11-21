#!/usr/bin/env bash
# schedule7 + Atakan Aldemir's multiplier-frontier / interface-closure optimizers (third_party/atakan) and
# royalacecat's donor-retaining "diverse" crossover pool (third_party/wangjakie/*_diverse*).
# Plateau archive: work/archive.jsonl (equal/near-length alternatives become new starting points). Stop: touch work/STOP_WJ
cd "$(dirname "$0")/.."
G='outs*/*/*.csv'; A=work/archive.jsonl; L=work/sched8.log
stop() { [ -e work/STOP_WJ ] && exit 0; }
wj() { stop; .venv/bin/python src/wj_driver.py --strategy "$1" --rounds "$2" --only "$3" --jobs 19 --batch "$4" --tag wj --seeds-glob "$G" --archive "$A" --max-seeds 8 $5 >> $L 2>&1; }
ak() { stop; .venv/bin/python src/atakan_driver.py --bin "$1" --args "$2" --only "$3" --jobs 19 --batch "$4" --archive "$A" $5 >> $L 2>&1; }
CL=("90 26 7 {salt} 4150" "92 32 4 {salt} 5250" "100 28 9 {salt} 4950")
FR=("8 4200 9 10 9 {salt} 1500" "14 8000 12 13 9 {salt} 3000" "8 4200 9 10 13 {salt} 1500")
.venv/bin/python src/changed_ids.py > /dev/null
for it in $(seq 1 1000); do
  stop; n=$(.venv/bin/python src/changed_ids.py); echo "[$(date +%H:%M:%S)] changed puzzles: $n" >> $L
  c=${CL[$((it % 3))]}; f=${FR[$((it % 3))]}
  if [ "$n" -gt 0 ]; then
    ak closure "$c" work/changed_ids.txt 1
    ak frontier "$f" work/changed_ids.txt 1
    wj anneal_diverse 3000 work/changed_ids.txt 1
  fi
  ak frontier "$f" work/focus_ids.txt 4
  ak closure "$c" work/focus_ids.txt 4
  wj anneal_diverse 1500 work/focus_ids.txt 4 --start-variant
  wj anneal_seeded 1500 work/focus_ids.txt 4 --start-variant
  wj incremental_diverse 1500 work/focus_ids.txt 4 --start-variant
  if [ $((it % 2)) -eq 0 ]; then ak frontier "$f" work/focus_ids.txt 4 --from-archive; fi
  if [ $((it % 8)) -eq 0 ]; then
    stop; .venv/bin/python src/recomb_pass.py --only work/focus_ids.txt --time 15 --cp 25 --jobs 19 --bin bin/elfopt8 \
          --extra "--hall 1" --tag loop2 --banks "$G" --variants 2 >> $L 2>&1
  fi
done
