// SPDX-License-Identifier: MIT
// LgubSensor SDK 최소 사용 예제.
// 프로빙으로 확정한 값(포트/보드레이트/슬레이브주소/레지스터/배율)을 Config 에
// 넣고 빌드하면 실제 거리(m)를 출력한다.
#include <cstdio>
#include <chrono>
#include <thread>

#include "lorddom_lgub/lgub_sensor.hpp"

int main(int argc, char** argv) {
  lorddom::Config cfg;
  cfg.port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  cfg.baud = 9600;                 // 프로빙으로 확정
  cfg.slave_id = 1;                // 프로빙으로 확정
  cfg.function = lorddom::FunctionCode::ReadHolding;
  cfg.distance_register = 0x0000;  // 프로빙으로 확정
  cfg.register_count = 1;
  cfg.scale_to_meter = 0.001;      // raw 가 mm 라고 가정. 프로빙으로 확정

  lorddom::LgubSensor sensor(cfg);
  lorddom::Status os = sensor.open();
  if (os != lorddom::Status::Ok) {
    printf("포트 열기 실패: %s (%s)\n", cfg.port.c_str(), lorddom::to_string(os));
    return 1;
  }
  printf("연결됨: %s. 거리 측정 시작 (Ctrl+C 종료)\n", cfg.port.c_str());

  for (;;) {
    lorddom::DistanceReading r = sensor.read_distance();
    if (r.status == lorddom::Status::Ok) {
      printf("거리 = %.3f m  (raw=%u)\n", r.distance_m, r.raw);
    } else {
      printf("읽기 실패: %s\n", lorddom::to_string(r.status));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return 0;
}
