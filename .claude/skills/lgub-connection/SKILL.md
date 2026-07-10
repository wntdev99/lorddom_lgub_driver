---
name: lgub-connection
description: LGUB 초음파 거리센서의 연결 상태를 점검한다. 하드웨어팀·비전문가가 "연결됐어?", "센서 붙었나/인식됐나", "잘 연결됐는지 확인", "연결 상태 봐줘", "왜 연결이 안 돼", "포트 잡혔어?", "어댑터 인식됐나" 라고 하면 발동. 어댑터→포트→권한→센서 응답의 사슬을 순서대로 점검해 어디서 끊겼는지와 조치를 알려준다.
---

# LGUB 센서 연결 상태 점검

연결은 **① RS485-USB 어댑터 → ② /dev/ttyUSB 포트 → ③ 접근 권한 → ④ 센서 응답**의
사슬이다. 어디 한 곳이 끊겨도 값이 안 나온다. 이 스킬은 각 고리를 순서대로 짚어
**끊긴 지점과 조치**를 콕 집어준다.

## 고정 하드웨어 (테스트 단계)
- RS485→USB 어댑터: **FTDI FT232, VID:PID `0403:6001`** (로봇 탑재 전까지 항상 이 모듈)
- 센서 통신: 9600 8N1, slave id 1

## 실행
저장소 루트에서:
```bash
./tools/check_connection.sh          # 사람용 사슬 점검
./tools/check_connection.sh --json   # 기계판독 (adapter/port/permission/sensor/distance_m)
```
`build/lgub` 가 없으면 먼저 빌드: `cmake -S . -B build >/dev/null && cmake --build build -j >/dev/null`

## 결과를 사람 말로 요약
JSON 필드로 사슬 상태를 판단해 요약한다:
- `sensor=true` → "센서까지 정상 연결됐습니다 (현재 거리 X m). 바로 측정 가능합니다."
- `adapter=false` → "USB-RS485 어댑터(FTDI)가 안 보입니다. PC에 꽂아 주세요."
- `adapter=true, port=false` → "어댑터는 인식되는데 포트가 안 생겼습니다. 다시 꽂거나 드라이버를 확인하세요."
- `port=true, permission=false` → "포트 권한이 없습니다. `sudo usermod -aG dialout $USER` 후 재로그인."
- `permission=true, sensor=false` → "어댑터·포트는 정상이나 센서가 무응답입니다. 센서 전원(10~30V, 빨강+/검정GND)과 RS485 배선(노랑=A+, 초록=B-), A/B 뒤바뀜을 확인하세요."

## 참고
- 연결이 확인되면 이어서 `lgub measure`(현재값)·`lgub monitor`(실시간)·`lgub log`(기록)로 넘어간다.
- CLI 는 통신이 끊겨도 자동 재접속(/dev/ttyUSB* 재탐색)을 시도한다.
