// SPDX-License-Identifier: MIT
#include "lorddom_lgub/modbus_rtu.hpp"

#include <thread>
#include <chrono>

namespace lorddom {

uint16_t ModbusRtu::crc16(const uint8_t* data, size_t len) {
  // Modbus RTU 표준 CRC16, 다항식 반사형 0xA001, 초기값 0xFFFF.
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);
    for (int b = 0; b < 8; ++b) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;  // 프레임에는 하위바이트 먼저(LSB), 상위바이트 나중(MSB)에 붙인다.
}

namespace {
void append_crc(std::vector<uint8_t>& frame) {
  uint16_t crc = ModbusRtu::crc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));         // CRC lo
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));  // CRC hi
}

bool crc_ok(const std::vector<uint8_t>& frame) {
  if (frame.size() < 3) return false;
  uint16_t calc = ModbusRtu::crc16(frame.data(), frame.size() - 2);
  uint16_t recv = static_cast<uint16_t>(frame[frame.size() - 2]) |
                  (static_cast<uint16_t>(frame[frame.size() - 1]) << 8);
  return calc == recv;
}
}  // namespace

ReadResult ModbusRtu::read_registers(uint8_t slave_id, FunctionCode func,
                                     uint16_t start_addr, uint16_t count,
                                     int timeout_ms, int inter_frame_delay_ms) {
  ReadResult res;
  if (!port_.is_open()) {
    res.status = Status::NotOpen;
    return res;
  }
  if (count == 0 || count > 125) {
    res.status = Status::InvalidArgument;
    return res;
  }

  // 요청 프레임: [addr][func][start_hi][start_lo][cnt_hi][cnt_lo][crc_lo][crc_hi]
  std::vector<uint8_t> req = {
      slave_id,
      static_cast<uint8_t>(func),
      static_cast<uint8_t>((start_addr >> 8) & 0xFF),
      static_cast<uint8_t>(start_addr & 0xFF),
      static_cast<uint8_t>((count >> 8) & 0xFF),
      static_cast<uint8_t>(count & 0xFF),
  };
  append_crc(req);

  port_.flush();
  if (inter_frame_delay_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms));
  }

  Status ws = port_.write_all(req.data(), req.size());
  if (ws != Status::Ok) {
    res.status = ws;
    return res;
  }

  // 정상 응답: [addr][func][byte_count][data...][crc_lo][crc_hi]
  // 예외 응답: [addr][func|0x80][exc_code][crc_lo][crc_hi] (총 5바이트)
  // 먼저 헤더 2바이트(addr, func)를 읽어 정상/예외를 구분한다.
  std::vector<uint8_t> resp;
  Status rs = port_.read_exact(resp, 2, timeout_ms);
  if (rs != Status::Ok) {
    res.status = rs;
    return res;
  }

  if (resp[0] != slave_id) {
    res.status = Status::InvalidResponse;
    return res;
  }

  // 예외 응답 처리
  if (resp[1] == (static_cast<uint8_t>(func) | 0x80)) {
    Status es = port_.read_exact(resp, 3, timeout_ms);  // exc + crc(2)
    if (es != Status::Ok) {
      res.status = es;
      return res;
    }
    if (!crc_ok(resp)) {
      res.status = Status::CrcError;
      return res;
    }
    res.status = Status::ExceptionResponse;
    res.exception_code = resp[2];
    return res;
  }

  if (resp[1] != static_cast<uint8_t>(func)) {
    res.status = Status::InvalidResponse;
    return res;
  }

  // 정상 응답: byte_count 읽기
  Status bs = port_.read_exact(resp, 1, timeout_ms);  // byte_count
  if (bs != Status::Ok) {
    res.status = bs;
    return res;
  }
  uint8_t byte_count = resp[2];
  if (byte_count != count * 2) {
    res.status = Status::InvalidResponse;
    return res;
  }

  // data(byte_count) + crc(2)
  Status ds = port_.read_exact(resp, byte_count + 2, timeout_ms);
  if (ds != Status::Ok) {
    res.status = ds;
    return res;
  }
  if (!crc_ok(resp)) {
    res.status = Status::CrcError;
    return res;
  }

  // data 영역은 resp[3] .. resp[3+byte_count-1]
  res.registers.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    uint16_t hi = resp[3 + i * 2];
    uint16_t lo = resp[3 + i * 2 + 1];
    res.registers.push_back(static_cast<uint16_t>((hi << 8) | lo));
  }
  res.status = Status::Ok;
  return res;
}

Status ModbusRtu::write_single_register(uint8_t slave_id, uint16_t addr,
                                        uint16_t value, int timeout_ms,
                                        int inter_frame_delay_ms) {
  if (!port_.is_open()) return Status::NotOpen;

  // 요청: [addr][0x06][addr_hi][addr_lo][val_hi][val_lo][crc_lo][crc_hi]
  std::vector<uint8_t> req = {
      slave_id,
      0x06,
      static_cast<uint8_t>((addr >> 8) & 0xFF),
      static_cast<uint8_t>(addr & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF),
      static_cast<uint8_t>(value & 0xFF),
  };
  append_crc(req);

  port_.flush();
  if (inter_frame_delay_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms));
  }

  Status ws = port_.write_all(req.data(), req.size());
  if (ws != Status::Ok) return ws;

  // 헤더 2바이트(addr, func)를 먼저 읽어 정상(8바이트 에코)/예외(5바이트)를 구분한다.
  std::vector<uint8_t> resp;
  Status rs = port_.read_exact(resp, 2, timeout_ms);
  if (rs != Status::Ok) return rs;

  if (resp[0] != slave_id) return Status::InvalidResponse;

  if (resp[1] == (0x06 | 0x80)) {
    // 예외 응답: exc_code + crc(2) = 3바이트 더
    Status es = port_.read_exact(resp, 3, timeout_ms);
    if (es != Status::Ok) return es;
    return crc_ok(resp) ? Status::ExceptionResponse : Status::CrcError;
  }
  if (resp[1] != 0x06) return Status::InvalidResponse;

  // 정상 에코: 나머지 6바이트(addr_hi,addr_lo,val_hi,val_lo,crc_lo,crc_hi)
  Status ds = port_.read_exact(resp, 6, timeout_ms);
  if (ds != Status::Ok) return ds;
  if (!crc_ok(resp)) return Status::CrcError;
  return Status::Ok;
}

}  // namespace lorddom
