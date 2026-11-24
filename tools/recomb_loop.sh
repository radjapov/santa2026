#!/usr/bin/env bash
# Фоновый трек v2: LNS с записью рецептов + CP-SAT перекомбинация (код goodpjw2008, Apache-2.0).
# Каждый проход: экспорт лучшего из хранилища -> recomb_pass -> merge обратно через наш верификатор.
#   [FRESH_HOURS=12 — только изменившиеся за N часов]  JOBS=14 LO=60 HI=180 TIME=15 CP=25 tools/recomb_loop.sh      Остановка: touch work/STOP_RC
S="$(cd "$(dirname "$0")/.." && pwd)"; R=${RDIR:-$S/work/rc}; JOBS=${JOBS:-3}; LO=${LO:-60}; HI=${HI:-180}; TIME=${TIME:-15}; CP=${CP:-25}
G=$S/third_party/goodpjw_dag_lns
mkdir -p $R/src $R/data $R/bin $R/work
cp $G/elf.py $G/pfmt.py $G/recombine.py $G/recomb_pass.py $R/src/
ln -sf $S/data/puzzles.csv $R/data/puzzles.csv
ln -sf $S/bin/elfopt $R/bin/elfopt
cd $S
while [ ! -e work/STOP_RC ]; do
  python3 store.py export $R/work/best.csv > /dev/null || exit 1
  if [ -n "$FRESH_HOURS" ]; then
    # FRESH_HOURS=12: только головоломки, чьи программы изменились за последние N часов (git-история best.json)
    python3 tools/fresh_ids.py $FRESH_HOURS $LO $HI > $R/work/focus_ids.txt
  else
  python3 -c "
from store import load_best; b=load_best()
open('$R/work/focus_ids.txt','w').write('\n'.join(p for p,v in b.items() if $LO<=v['score']<=$HI))"
  fi
  (cd $R && python3 src/recomb_pass.py --only work/focus_ids.txt --time $TIME --cp $CP --jobs $JOBS --bin bin/elfopt --tag rc) 2>&1 | grep -v "^LNS start"
  python3 store.py merge $R/work/best.csv
  rm -rf $R/work/rc_*
  # AUTOPUSH=1: после каждого прохода сохранить улучшения в git (контейнер может исчезнуть)
  if [ "${AUTOPUSH:-0}" = 1 ]; then
    python3 store.py commit-push "Store: $(python3 store.py status | awk '{print $4}') (recomb pass)"
  fi
done
