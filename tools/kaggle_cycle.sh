#!/usr/bin/env bash
# Сторож Kaggle: ждёт, пока все ноутбуки elf-rc-* закончат, затем pull (через верификатор) и push на свежих головоломках.
#   nohup tools/kaggle_cycle.sh >> work/kaggle_cycle.log 2>&1 &     (одна итерация; LOOP=1 — повторять бесконечно)
cd "$(dirname "$0")/.." || exit 1
while :; do
  while python3 tools/kaggle_rc.py status --shards 5 2>/dev/null | grep -q "running\|queued"; do sleep 300; done
  echo "[$(date +%H:%M)] all done -> pull"
  python3 tools/kaggle_rc.py pull --shards 5
  python3 store.py commit-push "Store: $(python3 store.py status | awk '{print $4}') (kaggle rc)"
  echo "[$(date +%H:%M)] push fresh"
  python3 tools/kaggle_rc.py push --shards 5 --lo 30 --hi 400 --fresh 12 --hours 11
  [ "${LOOP:-0}" = 1 ] || break
  sleep 1800
done
