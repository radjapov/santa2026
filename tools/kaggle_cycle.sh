#!/usr/bin/env bash
# Сторож Kaggle: раз в 10 мин каждый закончившийся ноутбук elf-rc-k → pull (верификатор) → push на свежих (12 ч).
cd "$(dirname "$0")/.." || exit 1
while [ ! -e work/STOP_KAGGLE ]; do
  out=$(python3 tools/kaggle_rc.py cycle --shards 5 --lo 30 --hi 400 --fresh 24 --hours 11 --engine mix 2>&1)
  if [ -n "$out" ]; then echo "$out"; python3 store.py commit-push "Store: $(python3 store.py status | awk '{print $4}') (kaggle rc)"; fi
  sleep 600
done
