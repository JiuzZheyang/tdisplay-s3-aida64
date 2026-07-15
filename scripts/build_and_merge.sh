#!/usr/bin/env bash
# 一键构建 + 合并 + 上传
#   1. 探测 ESP-IDF 环境（/opt/esp-idf/export.sh 或环境变量 IDF_PATH）
#   2. idf.py build
#   3. idf.py merge-bin -> firmware/aida64-merged.bin (4MB raw)
#   4. （可选） ./scripts/sync_source_to_seafile.sh 上传固件 + 源码 + BUILD.md
# 用法: ./scripts/build_and_merge.sh [--no-upload]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ---- 1. 探测 ESP-IDF 环境 ----
if [ -z "${IDF_PATH:-}" ] && [ -f /opt/esp-idf/export.sh ]; then
  echo "==> 探测到 /opt/esp-idf，source export.sh"
  # shellcheck disable=SC1091
  source /opt/esp-idf/export.sh >/dev/null
fi

if ! command -v idf.py >/dev/null 2>&1; then
  echo "!! 未找到 idf.py，请先安装 ESP-IDF v5.4 或设置 IDF_PATH" >&2
  exit 1
fi

cd "${ROOT_DIR}"

# ---- 2. 构建 ----
echo "==> idf.py build"
idf.py build

# ---- 3. 合并为整片固件 ----
echo "==> idf.py merge-bin (4MB raw)"
mkdir -p firmware
idf.py merge-bin -o /workspace/firmware/aida64-merged.bin --format raw --fill-flash-size 4MB

ls -lh firmware/aida64-merged.bin

# ---- 4. （可选）上传 ----
if [ "${1:-}" != "--no-upload" ]; then
  echo "==> 上传固件 + 源码 + BUILD.md"
  "${SCRIPT_DIR}/sync_source_to_seafile.sh"
fi
