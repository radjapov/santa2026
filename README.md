# Santa 2026 — The Elf Calculator

Рабочий репозиторий команды. Приватный: код и данные не передавать вне команды (правила соревнования).

## Что здесь

| Путь | Назначение |
|---|---|
| `verifier.py` | строгий верификатор по вкладкам Data и Evaluation (48 регистров, ≤4096 инструкций, ≤131072 байт в ячейке) |
| `store.py` | хранилище лучших программ `best/best.json` + сборка и проверка сабмита |
| `tools/lns_loop.py` | фоновый цикл `elfopt` (value-DAG LNS) |
| `tools/recomb_loop.sh` | фоновый цикл LNS + CP-SAT перекомбинация |
| `tools/atakan_loop.py` | фоновый цикл оптимизаторов для длинных головоломок |
| `tools/anatomy.py` | анализ программ: личные и общие инструкции на цель |
| `tools/kaggle_fetch.py` | скачивание выходов ноутбуков и датасетов через Kaggle REST API |
| `proto/` | прототипы и зонды основного трека |
| `third_party/` | публичный код с Kaggle (Apache-2.0), см. NOTICE-файлы внутри |
| `data/` | `puzzles.csv`, `practice_puzzles.csv`, `families_inferred.csv` |

Любая программа попадает в хранилище только после проверки верификатором и только если она строго короче текущей.

## Установка (Linux или WSL2 Ubuntu на Windows)

```bash
sudo apt update && sudo apt install -y g++ python3-pip git
```

```bash
pip install numpy pandas ortools
```

```bash
git clone https://github.com/radjapov/santa2026 && cd santa2026 && ./build.sh
```

```bash
python3 store.py status
```

## Фоновые циклы

На i5-14400F (16 потоков) оставьте пару потоков системе:

```bash
mkdir -p work && JOBS=12 nohup tools/recomb_loop.sh > work/rc.log 2>&1 &
```

```bash
nohup python3 tools/atakan_loop.py --lo 181 > work/atakan.log 2>&1 &
```

Автосинхронизация с публичным фронтом (нужен Kaggle-токен в `~/.kaggle/access_token`, см. ниже):

```bash
nohup python3 tools/sync_public.py --every 5400 --push > work/sync.log 2>&1 &
```

Для автопуша цикла перекомбинации запускайте его с `AUTOPUSH=1`.
Остановка: `touch work/STOP_RC`, `touch work/STOP_SYNC`, `touch work/STOP_ATK`.

## Kaggle-токен на локальной машине

```bash
mkdir -p ~/.kaggle && nano ~/.kaggle/access_token && chmod 600 ~/.kaggle/access_token
```

(вставить токен `KGAT_...` со страницы kaggle.com → Settings → API)

## Сабмит

```bash
python3 store.py export subs/submit.csv && python3 store.py check subs/submit.csv
```

Отправлять только файл, на котором `check` напечатал `VALID`.

## Синхронизация между машинами

`best/best.json` закоммичен. Если две машины улучшали его параллельно и при `git pull` возник конфликт:
сохраните свою копию, возьмите версию из репозитория и слейте свою через верификатор:

```bash
cp best/best.json /tmp/mine.json && git checkout --theirs best/best.json && python3 store.py merge-best /tmp/mine.json
```
