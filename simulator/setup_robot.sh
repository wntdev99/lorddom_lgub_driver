#!/usr/bin/env bash
# 로봇 mesh를 원본 패키지에서 simulator/robot/meshes 로 복사한다.
# mesh는 용량이 커서 git에 넣지 않으므로(.gitignore), 클론 후 1회 실행 필요.
#
# 사용: ./setup_robot.sh [원본_패키지_루트]
#   기본 원본: /home/jeongmin/Package/ros2/w_type_mw
set -euo pipefail

SRC="${1:-/home/jeongmin/Package/ros2/w_type_mw}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DST="$HERE/robot"

if [[ ! -d "$SRC/meshes" ]]; then
  echo "오류: 원본 mesh 폴더를 찾을 수 없음: $SRC/meshes" >&2
  echo "사용법: $0 [원본_패키지_루트]" >&2
  exit 1
fi

mkdir -p "$DST/meshes"
cp "$SRC/meshes/"*.STL "$DST/meshes/"
# URDF도 최신본으로 갱신(있으면)
[[ -f "$SRC/urdf/robot.urdf" ]] && cp "$SRC/urdf/robot.urdf" "$DST/robot.urdf"

n=$(ls "$DST/meshes/"*.STL 2>/dev/null | wc -l)
echo "완료: mesh $n개 복사 → $DST/meshes"
echo "이제 'python3 -m http.server 8791' 실행 후 http://localhost:8791 접속."
