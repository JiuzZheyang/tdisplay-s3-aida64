#!/usr/bin/env bash
# 将源码 + 构建产物打包并上传到 Seafile（按用户约定：先删后传）
# 目标：
#   /seafdav/T-Display-S3-AIDA64/aida64-merged.bin   -- 已合并的 4MB 整片固件
#   /seafdav/T-Display-S3-AIDA64/src.zip            -- 完整源码
#   /seafdav/T-Display-S3-AIDA64/BUILD.md            -- 编译/烧录说明
# 用法: ./scripts/sync_source_to_seafile.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "${ROOT_DIR}/.seafile.conf"

BASE="${SEAFILE_URL}/seafdav/${SEAFILE_LIBRARY}/"
# 用户名含 '@' 时 curl -u 会误解析，改为显式 Authorization 头
AUTH_HDR="Authorization: Basic $(printf '%s:%s' "${SEAFILE_USER}" "${SEAFILE_PASS}" | base64 -w0)"

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

echo "==> 1. 打包源码 → ${WORK}/src.zip"
(cd "${ROOT_DIR}" && \
  zip -rq "${WORK}/src.zip" \
    CMakeLists.txt sdkconfig.defaults \
    components main managed_components docs scripts \
    -x "build/*" "firmware/*" ".git/*" ".github/*" ".seafile.conf" \
    "managed_components/lvgl__lvgl/.git/*" \
    "managed_components/lvgl__lvgl/build/*" \
    "managed_components/lvgl__lvgl/docs/*" \
    "managed_components/lvgl__lvgl/tests/*" \
    "managed_components/lvgl__lvgl/scripts/*" \
    "managed_components/lvgl__lvgl/demos/*" \
    "managed_components/lvgl__lvgl/env_support/*" \
    "managed_components/lvgl__lvgl/.devcontainer/*" \
    "managed_components/lvgl__lvgl/.github/*" \
    "managed_components/lvgl__lvgl/.codecov.yml" \
    "managed_components/lvgl__lvgl/Kconfig" \
    "managed_components/lvgl__lvgl/*.md" \
)
ls -lh "${WORK}/src.zip"

echo "==> 2. 同步 BUILD.md"
cp "${ROOT_DIR}/docs/BUILD.md" "${WORK}/BUILD.md"
ls -lh "${WORK}/BUILD.md"

if [ ! -f "${ROOT_DIR}/firmware/aida64-merged.bin" ]; then
  echo "!! firmware/aida64-merged.bin 不存在，先跑 ./scripts/build_and_merge.sh 生成" >&2
  exit 1
fi

upload() {
  local local_path="$1"
  local remote_name="$2"
  echo "-- 上传 ${remote_name} --"
  curl -sS -H "${AUTH_HDR}" -X DELETE "${BASE}${remote_name}" -w "  DELETE -> HTTP %{http_code}\n" || true
  curl -sS -H "${AUTH_HDR}" -X MKCOL  "${BASE}" -w "  MKCOL  -> HTTP %{http_code}\n" || true
  curl -sS -H "${AUTH_HDR}" -T "${local_path}" "${BASE}${remote_name}" -w "  PUT    -> HTTP %{http_code}\n"
}

echo "==> 3. 上传到 ${BASE}"
upload "${ROOT_DIR}/firmware/aida64-merged.bin" "${SEAFILE_FILENAME}"
upload "${WORK}/src.zip"                            "src.zip"
upload "${WORK}/BUILD.md"                           "BUILD.md"

echo "完成: ${BASE}{aida64-merged.bin, src.zip, BUILD.md}"
