#!/usr/bin/env bash
# xopt (menes97, Apache-2.0) порциями: ids из файла, каждая порция → store.offer через scx_run.  Остановка: touch work/STOP_XOPT
#   IDS=work/xopt_ids.txt JOBS=10 T=600 FAM=1 tools/xopt_loop.sh
cd "$(dirname "$0")/.." || exit 1
IDS=${IDS:-work/xopt_ids.txt}; JOBS=${JOBS:-10}; T=${T:-600}; FAM=${FAM:-1}; CH=${CH:-20}
split -l $CH -d "$IDS" work/xopt_chunk_
for f in work/xopt_chunk_*; do
  [ -e work/STOP_XOPT ] && break
  echo "[$(date +%H:%M:%S)] chunk $f"
  python3 tools/scx_run.py --ids-file $f --bin bin/xopt --time $T --jobs $JOBS --tag xopt \
    --extra="--depth 6 --mincone 6 --mincone6 7 --t6 45 --fam $FAM --alts 24" 2>&1 | grep -E "\->|offer|done"
  rm -f $f
  [ "${AUTOPUSH:-0}" = 1 ] && python3 store.py commit-push "Store: $(python3 store.py status | awk '{print $4}') (xopt)"
done
rm -f work/xopt_chunk_*
