#!/usr/bin/env bash
# Сборка нативных движков в bin/ (Linux / WSL2, нужен g++ с поддержкой C++17).
set -e
cd "$(dirname "$0")"; mkdir -p bin
G=third_party/goodpjw_dag_lns
g++ -std=c++17 -O3 -march=native -o bin/elfopt $G/elfopt.cpp
for b in elf_multiplier_frontier elf_interface_closure; do
  g++ -std=c++17 -O3 -march=native -o bin/$b $G/atakan_optimizers/$b.cpp 2>/dev/null
done
ls -la bin
