// SPDX-License-Identifier: MIT
// LORDDOM LGUB 초음파 거리센서 고수준 SDK API.
//
// 사용 흐름:
//   lorddom::Config cfg;              // 필요시 파라미터 조정
//   lorddom::LgubSensor sensor(cfg);
//   sensor.open();
//   auto r = sensor.read_distance();
//   if (r.status == lorddom::Status::Ok) use(r.distance_m);
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lorddom_lgub/modbus_rtu.hpp"
#include "lorddom_lgub/serial_port.hpp"
#include "lorddom_lgub/types.hpp"

namespace lorddom {

// 거리 측정 결과.
struct DistanceReading {
  Status status = Status::NotOpen;
  bool valid = false;                   // status==Ok 이고 유효범위 내일 때만 true
  double distance_m = 0.0;              // Config.scale_to_meter 적용된 물리 거리
  uint32_t raw = 0;                     // 스케일 적용 전 원시값 (배율 역산/디버깅용)
  std::vector<uint16_t> raw_registers;  // 읽은 원시 레지스터 (진단용)
};

class LgubSensor {
 public:
  explicit LgubSensor(const Config& cfg) : cfg_(cfg) {}

  // /dev/ttyUSB* 후보 포트 목록(정렬). 하드웨어 진단/자동탐지용.
  static std::vector<std::string> list_serial_ports();

  // /dev/ttyUSB* 를 훑어 현재 파라미터로 '응답하는' 첫 포트를 cfg.port 에 채운다.
  // 사용자가 포트를 지정하지 않아도 되게 하는 편의 함수. 성공 시 Ok.
  static Status autodetect_port(Config& cfg);

  Status open();
  void close();
  bool is_open() const { return serial_.is_open(); }

  const Config& config() const { return cfg_; }
  Config& config() { return cfg_; }  // open 전 파라미터 수정용

  // 설정된 거리 레지스터를 읽어 물리 거리(m)로 변환한다.
  DistanceReading read_distance();

  // 임의 레지스터 직접 읽기 (프로빙/진단용).
  ReadResult read_registers(uint16_t start_addr, uint16_t count);

  // 슬레이브 주소 변경 등 설정 레지스터 쓰기 (0x06).
  Status write_register(uint16_t addr, uint16_t value);

  // 포트를 닫고 다시 연결한다(설정 포트 → 실패 시 rescan_ports 면 /dev/ttyUSB* 탐색).
  // 성공하면 응답이 확인된 포트로 cfg_.port 를 갱신한다.
  Status reconnect();

  // 진단/재접속 상황을 문자열로 통지받는 선택적 콜백 (기본 없음).
  void set_log(std::function<void(const std::string&)> cb) { log_ = std::move(cb); }

  // 연속 통신 실패 횟수 (진단용).
  int consecutive_failures() const { return consecutive_failures_; }

 private:
  DistanceReading read_once();
  void log(const std::string& msg) { if (log_) log_(msg); }

  Config cfg_;
  SerialPort serial_;
  int consecutive_failures_ = 0;
  std::function<void(const std::string&)> log_;
};

}  // namespace lorddom
