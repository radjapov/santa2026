#!/usr/bin/env bash
# Фоновый трек v2: LNS с записью рецептов + CP-SAT перекомбинация (код goodpjw2008, Apache-2.0).
# Каждый проход: экспорт лучшего из хранилища -> recomb_pass -> merge обратно через наш верификатор.
#   JOBS=14 LO=60 HI=180 tools/recomb_loop.sh      Остановка: touch work/STOP_RC
S="$(cd "$(dirname "$0")/.." && pwd)"; R=$S/work/rc; JOBS=${JOBS:-3}; LO=${LO:-60}; HI=${HI:-180}
G=$S/third_party/goodpjw_dag_lns
mkdir -p $R/src $R/data $R/bin $R/work
cp $G/elf.py $G/pfmt.py $G/recombine.py $G/recomb_pass.py $R/src/
ln -sf $S/data/puzzles.csv $R/data/puzzles.csv
ln -sf $S/bin/elfopt $R/bin/elfopt
cd $S
while [ ! -e work/STOP_RC ]; do
  python3 store.py export $R/work/best.csv > /dev/null || exit 1
  python3 -c "
from store import load_best; b=load_best()
open('$R/work/focus_ids.txt','w').write('\n'.join(p for p,v in b.items() if $LO<=v['score']<=$HI))"
  (cd $R && python3 src/recomb_pass.py --only work/focus_ids.txt --time 15 --cp 25 --jobs $JOBS --bin bin/elfopt --tag rc) 2>&1 | grep -v "^LNS start"
  python3 store.py merge $R/work/best.csv
  rm -rf $R/work/rc_*
  # AUTOPUSH=1: после каждого прохода сохранить улучшения в git (контейнер может исчезнуть)
  if [ "${AUTOPUSH:-0}" = 1 ] && ! git diff --quiet best/best.json; then
    git commit -q -m "Store: $(python3 store.py status | awk '{print $4}') (recomb pass)" best/best.json \
      && git push -q origin HEAD 2>&1 | tail -1
  fi
done
