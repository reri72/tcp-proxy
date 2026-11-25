#!/bin/bash

set -euo pipefail   # 오류 발생하면 바로 다 중단시키도록

# 현재 실행 중인 스크립트 파일의 경로를 정확히 가져와 변수 선언
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OUT_DIR="$ROOT_DIR/test/build"
WORK_DIR="$ROOT_DIR/test/tmp/pyi_work"  # 빌드 중간 작업 파일
SPEC_DIR="$ROOT_DIR/test/tmp/pyi_spec"  # .spec 파일 저장

if ! command -v pyinstaller >/dev/null 2>&1; then
    echo "error: pyinstaller is not installed" >&2
    exit 1
fi

mkdir -p "$OUT_DIR" "$WORK_DIR" "$SPEC_DIR"

# 콜론을 구분자로 지정
TARGETS=(
    "$ROOT_DIR/test/echo.py:echo_server"
)

for item in "${TARGETS[@]}"; do
    src="${item%%:*}"
    name="${item##*:}"

    echo "=== Building $name from $src ==="
    pyinstaller --onefile --clean --noconfirm \
        --name "$name" \
        --distpath "$OUT_DIR" \
        --workpath "$WORK_DIR" \
        --specpath "$SPEC_DIR" \
        "$src"
done

echo "check file(s): $OUT_DIR"