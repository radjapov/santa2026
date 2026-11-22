xopt.cpp — из публичного ноутбука Kaggle menes97 «Santa 2026 | Deep Cone Rebuilds on the Public Frontier | 131,955»,
код опубликован автором под Apache-2.0. Извлечён без изменений (22 ноя 2026), прочитан целиком перед запуском:
только stdin/stdout, без файлов и сети. Сборка: g++ -std=c++17 -O3 -march=native -o bin/xopt third_party/menes97_xopt/xopt.cpp
Запуск: tools/scx_run.py --bin bin/xopt --extra="--depth 6 --mincone 6 --mincone6 7 --t6 45 --fam 1 --alts 24"
