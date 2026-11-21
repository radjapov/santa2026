#!/usr/bin/env bash
# Запуск всех фоновых циклов после перезагрузки ПК (WSL2). Уже работающие не дублируются.
#   tools/start_all.sh            статус: tools/start_all.sh status
cd "$(dirname "$0")/.." || exit 1
mkdir -p work
running() { pgrep -f "$1" > /dev/null; }

if [ "$1" = status ]; then
  for p in recomb_loop.sh sync_public.py; do running "$p" && echo "работает: $p" || echo "НЕ работает: $p"; done
  python3 store.py status; exit 0
fi

[ -x bin/elfopt ] || ./build.sh
git config user.name > /dev/null || { git config user.name Claude; git config user.email noreply@anthropic.com; }
rm -f work/STOP_RC work/STOP_SYNC

if running recomb_loop.sh; then echo "recomb_loop уже работает"
else AUTOPUSH=1 JOBS=12 nohup tools/recomb_loop.sh > work/rc.log 2>&1 & echo "запущен recomb_loop (12 потоков)"; fi

if running sync_public.py; then echo "sync_public уже работает"
else nohup python3 tools/sync_public.py --every 1800 --push >> work/sync.log 2>&1 & echo "запущен sync_public (раз в 30 мин)"; fi

python3 store.py status
