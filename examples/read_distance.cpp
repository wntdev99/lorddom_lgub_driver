// SPDX-License-Identifier: MIT
// LgubSensor SDK 최소 사용 예제.
// 프로빙으로 확정한 값(포트/보드레이트/슬레이브주소/레지스터/배율)을 Config 에
// 넣고 빌드하면 실제 거리(m)를 출력한다.
#include <cstdio>
#include <chrono>
#include <thread>

#include "lorddom_lgub/lgub_sensor.hpp"

int main(int argc, char** argv) {
  // 아래 값은 LGU1000-18GM55-R4-V15 실물 프로빙으로 확정된 값이다.
  // (Config 기본값과 동일하므로 사실 명시하지 않아도 되지만, 예시로 남겨둔다.)
  lorddom::Config cfg;
  cfg.port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  cfg.baud = 9600;                                 // 확정
  cfg.slave_id = 1;                                // 확정
  cfg.function = lorddom::FunctionCode::ReadInput; // 확정: 0x04
  cfg.distance_register = 0x0002;                  // 확정: input reg 0x0002
  cfg.register_count = 1;
  cfg.scale_to_meter = 0.00017;                    // 확정: 1 count = 0.17mm

  lorddom::LgubSensor sensor(cfg);
  // 재접속 등 내부 진단 메시지를 화면에 표시.
  sensor.set_log([](const std::string& m) { printf("[SDK] %s\n", m.c_str()); });
  lorddom::Status os = sensor.open();
  if (os != lorddom::Status::Ok) {
    printf("포트 열기 실패: %s (%s)\n", cfg.port.c_str(), lorddom::to_string(os));
    return 1;
  }
  printf("연결됨: %s. 거리 측정 시작 (Ctrl+C 종료)\n", cfg.port.c_str());

  for (;;) {
    lorddom::DistanceReading r = sensor.read_distance();
    if (r.valid) {
      printf("거리 = %.3f m  (raw=%u)\n", r.distance_m, r.raw);
    } else if (r.status == lorddom::Status::NoTarget) {
      printf("대상 없음 (범위 밖/사각/빔 이탈)\n");
    } else if (r.status == lorddom::Status::OutOfRange) {
      printf("범위 밖 값 무시 (%.3f m)\n", r.distance_m);
    } else {
      printf("읽기 실패: %s\n", lorddom::to_string(r.status));
    }
    fflush(stdout);  // 실시간 스트림: 파이프/리다이렉트에서도 즉시 반영
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return 0;
}
