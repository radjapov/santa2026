#!/usr/bin/env bash
# Запуск всех фоновых циклов после перезагрузки ПК (WSL2). Уже работающие (по pid-файлам work/*.pid) не дублируются.
#   tools/start_all.sh            статус: tools/start_all.sh status
cd "$(dirname "$0")/.." || exit 1
mkdir -p work
alive() { [ -f "work/$1.pid" ] && kill -0 "$(cat "work/$1.pid")" 2> /dev/null; }

if [ "$1" = status ]; then
  for n in rc rc2 sync; do alive $n && echo "работает: $n (pid $(cat work/$n.pid))" || echo "НЕ работает: $n"; done
  python3 store.py status; exit 0
fi

[ -x bin/elfopt ] || ./build.sh
git config user.name > /dev/null || { git config user.name Claude; git config user.email noreply@anthropic.com; }
rm -f work/STOP_RC work/STOP_SYNC

if alive rc; then echo "rc (60–180) уже работает"
else rm -rf work/rc/work/rc_*; AUTOPUSH=1 JOBS=12 nohup tools/recomb_loop.sh > work/rc.log 2>&1 &
  echo $! > work/rc.pid; echo "запущен rc: перекомбинация 60–180, 12 потоков"; fi

if alive rc2; then echo "rc2 (181–400) уже работает"
else rm -rf work/rc2/work/rc_*; RDIR=$PWD/work/rc2 LO=181 HI=400 AUTOPUSH=1 JOBS=4 nohup tools/recomb_loop.sh > work/rc2.log 2>&1 &
  echo $! > work/rc2.pid; echo "запущен rc2: перекомбинация 181–400, 4 потока"; fi

if alive sync; then echo "sync уже работает"
else nohup python3 tools/sync_public.py --every 1800 --push >> work/sync.log 2>&1 &
  echo $! > work/sync.pid; echo "запущен sync: публичный фронт раз в 30 мин"; fi

python3 store.py status
