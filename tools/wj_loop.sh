#!/usr/bin/env bash
# Стратегии Wang Jakie (incremental / factored, Apache-2.0, third_party/wangjakie_gpu_nb) на свежих головоломках
# (изменившихся за FRESH_HOURS часов). Каждый проход: export → wj_driver → store merge (верификатор) → commit-push.
#   JOBS=6 FRESH_HOURS=12 ROUNDS=1000 tools/wj_loop.sh        Остановка: touch work/STOP_WJ
S="$(cd "$(dirname "$0")/.." && pwd)"; R=$S/work/wj; JOBS=${JOBS:-6}; FRESH_HOURS=${FRESH_HOURS:-12}; ROUNDS=${ROUNDS:-1000}
mkdir -p $R/src $R/data $R/third_party/wangjakie $R/work
cp $S/third_party/goodpjw_dag_lns/elf.py $S/third_party/goodpjw_dag_lns/wj_driver.py $R/src/
ln -sf $S/data/puzzles.csv $R/data/puzzles.csv
ln -sf $S/bin/wj_incremental $R/third_party/wangjakie/incremental; ln -sf $S/bin/wj_factored $R/third_party/wangjakie/factored
cd $S
while [ ! -e work/STOP_WJ ]; do
  for strat in incremental factored; do
    [ -e work/STOP_WJ ] && break
    python3 store.py export $R/work/best.csv > /dev/null || exit 1
    OLD=$(git -C $S log --before="$FRESH_HOURS hours ago" -1 --format=%H -- best/best.json)
    git -C $S show $OLD:best/best.json > $R/work/old.json
    python3 -c "
import json; from store import load_best; b=load_best(); o=json.load(open('$R/work/old.json'))
open('$R/work/focus_ids.txt','w').write('\n'.join(p for p,v in b.items() if 30<=v['score']<=400 and json.dumps(v['program'])!=json.dumps(o[p]['program'])))"
    (cd $R && python3 src/wj_driver.py --strategy $strat --rounds $ROUNDS --only work/focus_ids.txt --jobs $JOBS --batch 1 --tag wj) 2>&1
    python3 store.py merge $R/work/best.csv
    [ "${AUTOPUSH:-0}" = 1 ] && python3 store.py commit-push "Store: $(python3 store.py status | awk '{print $4}') (wj $strat)"
  done
done
