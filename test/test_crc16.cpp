// SPDX-License-Identifier: MIT
// 하드웨어 없이 실행하는 오프라인 검증.
// Modbus RTU 표준의 알려진 CRC16 벡터로 crc16() 정확성을 증명한다.
// 이 테스트가 통과하면, 센서 연결 전에도 프로토콜 계층이 규격에 맞음을 보장한다.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "lorddom_lgub/modbus_rtu.hpp"

using lorddom::ModbusRtu;

namespace {
int failures = 0;

// 프레임의 CRC 는 하위바이트 먼저 전송된다: 기대값(crc_lo, crc_hi).
void check(const char* name, const std::vector<uint8_t>& frame,
           uint8_t exp_lo, uint8_t exp_hi) {
  uint16_t crc = ModbusRtu::crc16(frame.data(), frame.size());
  uint8_t lo = crc & 0xFF;
  uint8_t hi = (crc >> 8) & 0xFF;
  bool ok = (lo == exp_lo && hi == exp_hi);
  printf("  [%s] %s : 계산 %02X %02X / 기대 %02X %02X\n", ok ? "PASS" : "FAIL",
         name, lo, hi, exp_lo, exp_hi);
  if (!ok) ++failures;
}
}  // namespace

int main() {
  printf("Modbus RTU CRC16 오프라인 검증\n");

  // 표준 참조 벡터 1: 슬레이브1, func 0x03, reg 0x0000, 1개 읽기 요청.
  //   01 03 00 00 00 01 -> CRC 84 0A  (전송: 84 그다음 0A)
  check("req 01 03 00 00 00 01", {0x01, 0x03, 0x00, 0x00, 0x00, 0x01}, 0x84,
        0x0A);

  // 표준 참조 벡터 2: 널리 인용되는 응답 데이터.
  //   01 04 02 FF FF -> CRC B8 80
  check("resp 01 04 02 FF FF", {0x01, 0x04, 0x02, 0xFF, 0xFF}, 0xB8, 0x80);

  // 참조 벡터 3: 문자열 "123456789" 의 Modbus CRC16 = 0x4B37.
  //   전송 순서: 37 4B
  check("ASCII 123456789",
        {'1', '2', '3', '4', '5', '6', '7', '8', '9'}, 0x37, 0x4B);

  if (failures == 0) {
    printf("\n전체 통과: 프로토콜 CRC 계층이 Modbus 규격과 일치합니다.\n");
    return 0;
  }
  printf("\n실패 %d건.\n", failures);
  return 1;
}
