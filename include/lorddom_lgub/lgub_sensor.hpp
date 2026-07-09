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
#include <vector>

#include "lorddom_lgub/modbus_rtu.hpp"
#include "lorddom_lgub/serial_port.hpp"
#include "lorddom_lgub/types.hpp"

namespace lorddom {

// 거리 측정 결과.
struct DistanceReading {
  Status status = Status::NotOpen;
  double distance_m = 0.0;              // Config.scale_to_meter 적용된 물리 거리
  uint32_t raw = 0;                     // 스케일 적용 전 원시값 (배율 역산/디버깅용)
  std::vector<uint16_t> raw_registers;  // 읽은 원시 레지스터 (진단용)
};

class LgubSensor {
 public:
  explicit LgubSensor(const Config& cfg) : cfg_(cfg) {}

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

 private:
  Config cfg_;
  SerialPort serial_;
};

}  // namespace lorddom
