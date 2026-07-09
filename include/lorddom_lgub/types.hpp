// SPDX-License-Identifier: MIT
// LORDDOM LGUB Ultrasonic Distance Sensor - SDK 공통 타입 정의
//
// 이 센서는 공식 개발 문서가 없다. 따라서 프로토콜 파라미터(슬레이브 주소,
// 보드레이트, function code, 거리 레지스터 주소, 배율, 바이트 순서)를 모두
// 런타임에 설정 가능하도록 만든다. 실물 프로빙으로 확정한 값만 Config에
// 채워 넣으면 그대로 동작한다.
#pragma once

#include <cstdint>
#include <string>

namespace lorddom {

// SDK 호출 결과 코드. 예외 대신 명시적 상태 코드를 반환한다
// (임베디드/실시간 환경 이식성 및 예측 가능성 확보 목적).
enum class Status {
  Ok = 0,
  NotOpen,            // 포트가 열려 있지 않음
  SerialError,        // read/write/설정 등 시리얼 계층 오류 (errno 참고)
  Timeout,            // 응답 시간 초과 (센서 무응답 또는 파라미터 불일치)
  CrcError,           // CRC16 불일치 (노이즈 또는 보드레이트 불일치)
  ExceptionResponse,  // Modbus 예외 응답 (function|0x80). exception_code 참고
  InvalidResponse,    // 프레임 구조 불일치 (슬레이브 주소/function 불일치 등)
  InvalidArgument,    // 잘못된 인자
};

const char* to_string(Status s);

// Modbus 레지스터 읽기 function code.
enum class FunctionCode : uint8_t {
  ReadHolding = 0x03,  // Read Holding Registers (가장 흔함)
  ReadInput = 0x04,    // Read Input Registers
};

// 16비트 레지스터 값을 물리량으로 해석할 때의 바이트/워드 순서.
// 단일 레지스터(16비트)에서는 EndianBig/EndianLittle만 의미가 있으며,
// 32비트(2 레지스터) 값이면 워드 스왑 조합까지 사용한다.
enum class WordOrder {
  Big,           // 16비트: [hi lo] (Modbus 표준). 32비트: [reg0=hi, reg1=lo]
  Little,        // 16비트: [lo hi]
  BigWordSwap,   // 32비트: reg 순서 교환
  LittleWordSwap,
};

// SDK 접속/해석 파라미터. 문서 부재를 흡수하는 핵심 구조체.
struct Config {
  // --- 시리얼 계층 ---
  std::string port = "/dev/ttyUSB0";
  int baud = 9600;          // 대개 9600, 프로빙으로 확정
  char parity = 'N';        // 'N' | 'E' | 'O'
  int data_bits = 8;
  int stop_bits = 1;

  // --- Modbus 계층 ---
  uint8_t slave_id = 1;                          // 기본 슬레이브 주소
  FunctionCode function = FunctionCode::ReadHolding;
  uint16_t distance_register = 0x0000;           // 거리값 레지스터 주소
  uint16_t register_count = 1;                   // 1=16비트, 2=32비트 값
  int response_timeout_ms = 300;                 // 응답 대기 한계
  int inter_frame_delay_ms = 10;                 // 요청 전 라인 안정화 대기

  // --- 물리량 해석 ---
  // 실제 거리(m) = raw * scale_to_meter.
  // 예: raw가 mm 단위면 0.001, cm 단위면 0.01, (mm x10) 단위면 0.0001.
  // 프로빙 시 알려진 거리에 대고 raw를 읽어 역산한다.
  double scale_to_meter = 0.001;
  WordOrder word_order = WordOrder::Big;
};

}  // namespace lorddom
