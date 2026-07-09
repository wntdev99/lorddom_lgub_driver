// SPDX-License-Identifier: MIT
// 최소 Modbus-RTU 마스터 구현 (읽기 중심).
// 의존성 없이 프레임을 직접 조립/파싱하므로, 문서 없는 센서를 프로빙할 때
// 임의의 주소/레지스터/function code 로 요청을 자유롭게 쏠 수 있다.
#pragma once

#include <cstdint>
#include <vector>

#include "lorddom_lgub/serial_port.hpp"
#include "lorddom_lgub/types.hpp"

namespace lorddom {

// 레지스터 읽기 결과.
struct ReadResult {
  Status status = Status::NotOpen;
  std::vector<uint16_t> registers;  // 성공 시 읽은 16비트 레지스터들
  uint8_t exception_code = 0;        // ExceptionResponse 일 때 유효
};

class ModbusRtu {
 public:
  // Modbus RTU 표준 CRC16 (다항식 0xA001). 오프라인 테스트로 검증됨.
  static uint16_t crc16(const uint8_t* data, size_t len);

  explicit ModbusRtu(SerialPort& port) : port_(port) {}

  // Read Holding(0x03) / Read Input(0x04) Registers.
  // count 개의 16비트 레지스터를 start_addr 부터 읽는다.
  ReadResult read_registers(uint8_t slave_id, FunctionCode func,
                            uint16_t start_addr, uint16_t count,
                            int timeout_ms, int inter_frame_delay_ms);

  // Write Single Register(0x06). 슬레이브 주소/보드레이트 등 설정 변경용.
  Status write_single_register(uint8_t slave_id, uint16_t addr, uint16_t value,
                               int timeout_ms, int inter_frame_delay_ms);

 private:
  SerialPort& port_;
};

}  // namespace lorddom
