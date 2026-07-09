---
name: lgub-sensor
description: LORDDOM LGUB RS485 초음파 거리센서를 자연어로 조작한다. "거리 재줘/지금 값/현재 거리", "스트리밍/실시간으로 보여줘", "로깅/기록/CSV로 남겨줘", "센서 연결 안 돼/진단해줘" 같은 요청에서 사용. lgub CLI(measure/stream/log/doctor)를 호출하고 결과를 비전문가가 이해할 평이한 한국어로 요약한다. distance sensor, ultrasonic, LGUB, 초음파 트리거.
---

# LGUB 초음파 거리센서 자연어 조작

LORDDOM LGUB(모델 LGU1000-18GM55-R4-V15, 측정 60~1000mm, RS485/Modbus) 센서를
`lgub` CLI로 조작한다. **센서 로직은 전부 SDK에 있고, 이 CLI는 SDK를 호출하는 얇은
실행 입구다.** 사용자는 하드웨어를 몰라도 되며, 포트(`/dev/ttyUSB*`)는 CLI가 자동 탐지한다.

## 준비: 바이너리 확보
CLI 경로는 이 저장소의 `build/lgub` 이다. 없으면 먼저 빌드한다(저장소 루트에서):
```bash
cmake -S . -B build >/dev/null && cmake --build build -j >/dev/null
```
이후 항상 `./build/lgub ...` 로 실행한다. (포트를 자동으로 못 찾으면 `--port /dev/ttyUSB0` 지정)

## 자연어 → 명령 매핑
| 사용자가 말하면 | 실행 |
|---|---|
| "지금 거리", "현재 값", "얼마야", "한 번 재줘" | `./build/lgub measure --json` |
| "실시간으로 보여줘", "스트리밍", "계속 보여줘" | `./build/lgub stream --hz 5 --sec 30` (원하는 시간/주기로 조정) |
| "N초/분 동안 기록/로깅/CSV" | `./build/lgub log --out <경로>.csv --sec <초> --hz 5 --json` |
| "연결 안 돼", "진단", "왜 안 되지", "센서 확인" | `./build/lgub doctor` |

- **measure/log 는 `--json` 을 붙여 실행**하고, 그 JSON을 파싱해 사람 말로 요약한다.
- **stream 은 오래 걸리므로 백그라운드로 실행**(run_in_background)하고, 완료 시 요약한다.
  무한 스트리밍은 피하고 `--sec` 로 시간을 정한다. 사용자가 "그만"이라 하면 중단한다.
- 로그 파일 경로를 사용자가 안 주면 홈에 날짜 기반 이름(예: `~/lgub_YYYYMMDD_HHMM.csv`)을 제안·사용한다.

## 결과 해석(사람 말로 바꾸기)
measure/stream/log JSON 의 필드:
- `valid=true` + `distance_m` → "현재 약 X cm(=Y m) 앞에 물체가 있습니다."
- `status="NoTarget"` (valid=false) → "센서 앞 유효 범위(약 6cm~1m)에 잡히는 물체가 없습니다."
- `status="OutOfRange"` → "측정값이 유효 범위를 벗어나 무시했습니다."
- `ok=false` 또는 통신 오류(Timeout/CrcError/SerialError) → 통신 문제. `doctor` 로 넘어간다.
- log 요약: `samples/valid/valid_pct/mean_m/min_m/max_m` 를 "총 N회 중 M회 유효(P%), 평균 A m" 식으로.

거리는 항상 **m와 cm를 함께** 알려주면 비전문가가 이해하기 쉽다.

## 연결 문제 대응 (doctor 안내)
`doctor` 가 센서를 못 찾으면 아래를 순서대로 안내한다(데이터시트 확정 배선):
1. 센서 전원 10~30V DC 인가 (빨강=+, 검정=GND)
2. RS485 배선: **노랑=A(+), 초록=B(−)** — 어댑터에 단단히
3. A/B 가 서로 바뀌지 않았는지
4. USB-RS485 어댑터가 꽂혀 있는지 (`/dev/ttyUSB*` 생성 여부)

CLI는 통신이 끊겨도 자동 재접속(`/dev/ttyUSB*` 재탐색)을 시도하므로, 케이블을 다시
꽂으면 대개 스스로 복구된다.

## 안전 수칙
- 이 Skill의 명령은 **읽기 전용**이다.
- 슬레이브 주소·보드레이트 변경(write) 은 잘못하면 통신이 끊긴다. 사용자가 명시적으로
  요청하고 위험을 확인하기 전에는 절대 수행하지 않는다.
