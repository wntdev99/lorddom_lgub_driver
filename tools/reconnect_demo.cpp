// SPDX-License-Identifier: MIT
// 자동 재접속 로직 결정적 검증 데모.
//
// 일부러 존재하지 않는 포트로 시작한다. 정상이라면 SDK 는:
//   (1) 통신 실패를 연속 감지하고
//   (2) reconnect_after_failures 도달 시 reconnect() 실행,
//   (3) rescan_ports 로 /dev/ttyUSB* 를 훑어 응답하는 실제 센서를 찾아
//   (4) 스스로 복구해 유효 거리를 읽는다.
// 이는 케이블 중간 분리(포트 사라짐/이름 변경)와 동일한 코드 경로다.
#include <cstdio>
#include <chrono>
#include <thread>

#include "lorddom_lgub/lgub_sensor.hpp"

int main() {
  lorddom::Config cfg;
  cfg.port = "/dev/ttyGHOST_does_not_exist";  // 고의로 잘못된 시작 포트
  cfg.auto_reconnect = true;
  cfg.reconnect_after_failures = 2;           // 빠른 시연을 위해 낮춤
  cfg.rescan_ports = true;                    // /dev/ttyUSB* 재탐색 허용

  lorddom::LgubSensor sensor(cfg);
  sensor.set_log([](const std::string& m) { printf("[SDK] %s\n", m.c_str()); });

  lorddom::Status os = sensor.open();
  printf("초기 open(%s) 결과 = %s (실패가 정상)\n",
         "/dev/ttyGHOST_does_not_exist", lorddom::to_string(os));

  for (int i = 0; i < 8; ++i) {
    lorddom::DistanceReading r = sensor.read_distance();
    if (r.valid) {
      printf("read[%d] 유효 거리 = %.3f m  (현재 포트=%s)\n", i, r.distance_m,
             sensor.config().port.c_str());
    } else {
      printf("read[%d] 무효/실패 = %s (연속실패=%d)\n", i,
             lorddom::to_string(r.status), sensor.consecutive_failures());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return 0;
}
