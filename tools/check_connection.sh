#!/usr/bin/env bash
# LGUB 센서 연결 상태 점검 (어댑터 → 포트 → 권한 → 센서 응답의 사슬).
#
# RS485→USB 어댑터는 테스트 단계 동안 항상 FTDI FT232(VID:PID 0403:6001)를 쓴다.
# 각 고리를 순서대로 검사해 '어디서 끊겼는지'와 다음 조치를 콕 집어 알려준다.
#
# 사용: tools/check_connection.sh [--json]
# 종료코드: 0=센서까지 정상, 1=중간 어디선가 끊김.

set -u
EXPECT_VIDPID="0403:6001"
EXPECT_NAME="FTDI FT232 (RS485-USB)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LGUB="$REPO_DIR/build/lgub"

JSON=0
[ "${1:-}" = "--json" ] && JSON=1

# 결과 수집
adapter_ok=0; port_ok=0; port=""; perm_ok=0; sensor_ok=0; dist="null"

# 1) 어댑터
if lsusb 2>/dev/null | grep -qi "$EXPECT_VIDPID"; then adapter_ok=1; fi

# 2) 포트 (FTDI 소속 ttyUSB 우선 식별)
for p in /dev/ttyUSB*; do
  [ -e "$p" ] || continue
  vid=$(udevadm info -q property -n "$p" 2>/dev/null | sed -n 's/^ID_VENDOR_ID=//p')
  pid=$(udevadm info -q property -n "$p" 2>/dev/null | sed -n 's/^ID_MODEL_ID=//p')
  if [ "${vid}:${pid}" = "$EXPECT_VIDPID" ]; then port="$p"; break; fi
  [ -z "$port" ] && port="$p"   # FTDI 특정 못하면 첫 포트라도
done
[ -n "$port" ] && port_ok=1

# 3) 권한 (dialout 그룹 + 실제 쓰기 가능)
if [ -n "$port" ] && [ -w "$port" ]; then perm_ok=1
elif groups 2>/dev/null | grep -qw dialout; then perm_ok=1; fi

# 4) 센서 응답 (SDK 경유)
if [ -x "$LGUB" ] && [ -n "$port" ]; then
  out=$("$LGUB" measure --json --port "$port" 2>/dev/null)
  if echo "$out" | grep -q '"ok":true'; then
    sensor_ok=1
    dist=$(echo "$out" | sed -n 's/.*"distance_m":\([0-9.]*\).*/\1/p')
  fi
fi

if [ "$JSON" = "1" ]; then
  printf '{"adapter":%s,"port":%s,"port_path":"%s","permission":%s,"sensor":%s,"distance_m":%s}\n' \
    "$([ $adapter_ok = 1 ] && echo true || echo false)" \
    "$([ $port_ok = 1 ] && echo true || echo false)" "$port" \
    "$([ $perm_ok = 1 ] && echo true || echo false)" \
    "$([ $sensor_ok = 1 ] && echo true || echo false)" \
    "${dist:-null}"
  [ $sensor_ok = 1 ] && exit 0 || exit 1
fi

# 사람용 출력
mark() { [ "$1" = 1 ] && echo "✅" || echo "❌"; }
echo "=== LGUB 센서 연결 점검 ==="
echo "$(mark $adapter_ok) 1. RS485-USB 어댑터  : ${EXPECT_NAME} ($EXPECT_VIDPID)"
echo "$(mark $port_ok) 2. 시리얼 포트        : ${port:-없음}"
echo "$(mark $perm_ok) 3. 접근 권한(dialout) : $([ $perm_ok = 1 ] && echo 정상 || echo 불가)"
echo "$(mark $sensor_ok) 4. 센서 응답(9600/id1): $([ $sensor_ok = 1 ] && echo "정상 (거리 ${dist} m)" || echo 무응답)"
echo ""

if [ $sensor_ok = 1 ]; then
  echo "→ 전 구간 정상. 바로 사용 가능합니다 (lgub measure / monitor / log)."
  exit 0
fi
echo "→ 조치:"
if [ $adapter_ok = 0 ]; then
  echo "  · USB-RS485 어댑터(FTDI)가 안 보입니다. PC USB 포트에 꽂으세요."
elif [ $port_ok = 0 ]; then
  echo "  · 어댑터는 보이나 /dev/ttyUSB* 가 없습니다. 드라이버/재꽂기 확인."
elif [ $perm_ok = 0 ]; then
  echo "  · 포트 권한 없음. 'sudo usermod -aG dialout \$USER' 후 재로그인."
else
  echo "  · 어댑터·포트는 정상이나 센서 무응답. 센서 전원 10~30V(빨강+/검정GND),"
  echo "    RS485 배선(노랑=A+, 초록=B-), A/B 뒤바뀜을 확인하세요."
fi
exit 1
