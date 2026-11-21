#!/usr/bin/env bash
# Фоновый трек v2: LNS с записью рецептов + CP-SAT перекомбинация (код goodpjw2008, Apache-2.0).
# Каждый проход: экспорт лучшего из хранилища -> recomb_pass -> merge обратно через наш верификатор.
# Остановка: touch work/STOP_RC
S=/home/user/santa2026; R=$S/work/rc; JOBS=${JOBS:-3}; LO=${LO:-60}; HI=${HI:-180}
cd $S
while [ ! -e work/STOP_RC ]; do
  python3 store.py export $R/work/best.csv > /dev/null || exit 1
  python3 -c "
from store import load_best; b=load_best()
open('$R/work/focus_ids.txt','w').write('\n'.join(p for p,v in b.items() if $LO<=v['score']<=$HI))"
  (cd $R && python3 src/recomb_pass.py --only work/focus_ids.txt --time 15 --cp 25 --jobs $JOBS --bin bin/elfopt --tag rc) 2>&1 | grep -v "^LNS start"
  python3 store.py merge $R/work/best.csv
  rm -rf $R/work/rc_*
done
