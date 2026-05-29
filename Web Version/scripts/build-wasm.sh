#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEB_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_ROOT="$(cd "${WEB_ROOT}/.." && pwd)"
CONES_ROOT="${PROJECT_ROOT}"
OUT_DIR="${WEB_ROOT}/public/runtime"

EM_CACHE="${EM_CACHE:-/private/tmp/emscripten-cache}"
EIGEN_INCLUDE="${EIGEN_INCLUDE:-/opt/homebrew/include/eigen3}"

mkdir -p "${OUT_DIR}" "${EM_CACHE}"

cd "${CONES_ROOT}"

env EM_CACHE="${EM_CACHE}" em++ -O2 -std=c++20 \
  -fexceptions \
  -I . -I "${EIGEN_INCLUDE}" \
  src/main.cpp \
  -o "${OUT_DIR}/cnes-module.js" \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createCoNESModule \
  -sENVIRONMENT=worker,node \
  -sINVOKE_RUN=0 \
  -sEXPORTED_RUNTIME_METHODS=FS,callMain \
  -sALLOW_MEMORY_GROWTH=1
