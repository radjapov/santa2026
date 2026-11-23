#!/usr/bin/env bash
# Запуск всех фоновых циклов после перезагрузки ПК (WSL2). Уже работающие (по pid-файлам work/*.pid) не дублируются.
#   tools/start_all.sh            статус: tools/start_all.sh status
cd "$(dirname "$0")/.." || exit 1
mkdir -p work
alive() { [ -f "work/$1.pid" ] && kill -0 "$(cat "work/$1.pid")" 2> /dev/null; }

if [ "$1" = status ]; then
  for n in rc rcf sync autosubmit; do alive $n && echo "работает: $n (pid $(cat work/$n.pid))" || echo "НЕ работает: $n"; done
  python3 store.py status; exit 0
fi

[ -x bin/elfopt ] || ./build.sh
git config user.name > /dev/null || { git config user.name Claude; git config user.email noreply@anthropic.com; }
rm -f work/STOP_RC work/STOP_SYNC work/STOP_AUTOSUBMIT

if alive rc; then echo "rc (60–180) уже работает"
else rm -rf work/rc/work/rc_*; AUTOPUSH=1 JOBS=${RC_JOBS:-4} nohup tools/recomb_loop.sh > work/rc.log 2>&1 &
  echo $! > work/rc.pid; echo "запущен rc: перекомбинация 60–180, ${RC_JOBS:-4} потоков"; fi

# rc2 (181–400) отключён 22 ноя: 0 за все проходы.
if alive rcf; then echo "rcf (свежие) уже работает"
else rm -rf work/rcf/work/rc_*; RDIR=$PWD/work/rcf FRESH_HOURS=12 LO=30 HI=400 AUTOPUSH=1 JOBS=${RCF_JOBS:-12} nohup tools/recomb_loop.sh > work/rcf.log 2>&1 &
  echo $! > work/rcf.pid; echo "запущен rcf: перекомбинация изменившихся за 12 ч (30–400), ${RCF_JOBS:-12} потоков"; fi

if alive sync; then echo "sync уже работает"
else nohup python3 tools/sync_public.py --every 600 --push >> work/sync.log 2>&1 &
  echo $! > work/sync.pid; echo "запущен sync: публичный фронт раз в 10 мин"; fi

if alive autosubmit; then echo "autosubmit уже работает"
else nohup python3 tools/autosubmit.py --every 600 --min 3 >> work/autosubmit.log 2>&1 &
  echo $! > work/autosubmit.pid; echo "запущен autosubmit: сабмит при выигрыше >= 3 (разрешено пользователем 22 ноя)"; fi

python3 store.py status
