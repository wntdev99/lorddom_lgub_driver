# lorddom_lgub_driver

LORDDOM **LGUB 초음파 거리센서**(측정거리 1m, RS485 출력)용 **순수 C++ SDK**.

이 센서는 공식 개발 문서가 없다. 그러나 RS485 초음파 센서는 사실상 **Modbus-RTU**
표준(9600 8N1, function `0x03`/`0x04`로 거리 레지스터 읽기, CRC16)을 따르므로,
프로토콜 계층을 규격대로 구현해 두고 **실물 프로빙으로 나머지 3개 값
(슬레이브 주소 / 거리 레지스터 주소 / 배율)만 확정**하면 그대로 동작한다.

## 특징
- **의존성 0** — POSIX termios + 자체 Modbus-RTU 마스터. libmodbus 불필요.
- **전부 설정 가능** — `lorddom::Config` 하나로 포트/보드레이트/슬레이브 주소/
  function code/레지스터 주소/배율/바이트 순서 조정.
- **프로빙 도구 내장** — 문서 없이 규격을 역산하는 `lgub_probe` CLI.
- **하드웨어 없이 검증** — Modbus 표준 CRC 벡터로 오프라인 단위테스트.
- ROS 비의존. 이후 rclcpp 노드로 감싸기 쉬운 구조.

## 구성
```
include/lorddom_lgub/   공개 헤더 (types, serial_port, modbus_rtu, lgub_sensor)
src/                    구현
tools/lgub_probe.cpp    자동 프로빙 CLI (reverse engineering)
examples/read_distance.cpp  최소 사용 예제
test/test_crc16.cpp     하드웨어 불필요 오프라인 검증
```

## 빌드
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # 오프라인 CRC 검증
```

## 어댑터 연결 후 규격 확정 절차 (문서 대체)

RS485-USB 어댑터를 연결하면 `/dev/ttyUSB0` 등이 생긴다. 아래 3단계로 확정한다.

1) **통신 성립(주소·보드레이트) 찾기**
```bash
./build/lgub_probe scan --port /dev/ttyUSB0
```
보드레이트 x 슬레이브주소(1~247)를 전수 조사해 응답하는 조합을 찾는다.

2) **거리 레지스터·function code 찾기**
```bash
./build/lgub_probe regs --port /dev/ttyUSB0 --baud 9600 --id 1 --func 3
```
`0x0000~0x000F`를 훑어 값이 읽히는 주소를 확인한다. (`--func 4`도 시도)

3) **배율(scale) 역산**
```bash
./build/lgub_probe watch --port /dev/ttyUSB0 --baud 9600 --id 1 --reg 0 --func 3
```
센서를 **알려진 거리**(100mm, 200mm, 500mm)에 대고 raw 변화를 관찰한다.
raw가 mm면 `scale_to_meter=0.001`, cm면 `0.01`, (mm×10)이면 `0.0001`.

## 사용 (확정값 반영 후)
```cpp
lorddom::Config cfg;
cfg.port = "/dev/ttyUSB0";
cfg.baud = 9600;                 // 프로빙 확정
cfg.slave_id = 1;                // 프로빙 확정
cfg.distance_register = 0x0000;  // 프로빙 확정
cfg.scale_to_meter = 0.001;      // 프로빙 확정

lorddom::LgubSensor sensor(cfg);
sensor.open();
auto r = sensor.read_distance();
if (r.status == lorddom::Status::Ok)
  printf("%.3f m\n", r.distance_m);
```
또는 예제 바이너리:
```bash
./build/lgub_read_distance /dev/ttyUSB0
```

## 견고성 기능

- **무효 측정 필터**: `reg2=0`(대상 없음/범위 초과/빔 이탈)은 `NoTarget`,
  유효범위(`min_valid_m`~`max_valid_m`, 기본 0.06~1.0m) 밖은 `OutOfRange`로 구분.
  `DistanceReading.valid` 가 true 일 때만 거리를 신뢰하면 된다.
- **자동 재접속**: 통신 실패가 `reconnect_after_failures`회 연속되면 포트를 닫고
  다시 연결한다. `rescan_ports=true` 면 `/dev/ttyUSB*` 를 훑어 응답하는 포트를
  스스로 찾는다(케이블 재연결로 포트 번호가 바뀌어도 복구). 진단은 `set_log()` 로 통지.
  ```bash
  ./build/lgub_reconnect_demo   # 잘못된 포트로 시작해 자동 복구되는 결정적 데모
  ```

## 배선 (공식 데이터시트 확정, 모델 LGU1000-18GM55-R4-V15)

M12 x1, 5-pin 커넥터:

| 핀 | 색 | 기능 |
|---|---|---|
| 1 | RD(빨강) | +U_B 전원 + (**10~30V DC**) |
| 3 | BK(검정) | −U_B / GND 전원 − |
| 2 | YE(노랑) | **A, +RS485** |
| 4 | GR(초록) | **B, −RS485** |

- RS485: 어댑터 A(+) ← 노랑, B(−) ← 초록
- 전원: **별도 10~30V DC** 공급 필요 (빨강 +, 검정 GND). USB-RS485의 신호선만으로는 구동 불가.
- 라인 양끝 120Ω 종단저항, 어댑터 half-duplex 자동 방향전환 지원 확인.

### 데이터시트 확정 사양 (60~1000mm 모델)
측정 60~1000mm / 사각 0~60mm / 분해능 0.17mm / 반복정확도 ±0.15% F.S. /
응답 52ms / 동작전압 10~30V DC(역극성 보호) / IP67.
통신은 "RS485 + Modbus"만 명시 — **레지스터맵·슬레이브주소·보드레이트·배율은
미기재**이므로 `lgub_probe`로 확정한다.

## scan이 아무것도 못 찾을 때 점검
- A/B 극성 뒤바뀜(노랑/초록 교차 시도)
- 센서 전원(10~30V DC) 실제 인가 여부
- 종단저항, 어댑터 방향전환 지원
