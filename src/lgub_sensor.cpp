// SPDX-License-Identifier: MIT
#include "lorddom_lgub/lgub_sensor.hpp"

#include <dirent.h>

#include <algorithm>

namespace lorddom {

namespace {

// 통신 계층 실패인가? (센서는 응답했으나 대상만 없는 NoTarget/OutOfRange 는 제외)
bool is_comm_failure(Status s) {
  switch (s) {
    case Status::Timeout:
    case Status::CrcError:
    case Status::SerialError:
    case Status::NotOpen:
    case Status::InvalidResponse:
      return true;
    default:
      return false;  // Ok, NoTarget, OutOfRange, ExceptionResponse 등
  }
}

// /dev/ttyUSB* 후보 포트 목록 (정렬).
std::vector<std::string> list_usb_ports() {
  std::vector<std::string> ports;
  DIR* d = opendir("/dev");
  if (!d) return ports;
  struct dirent* e;
  while ((e = readdir(d)) != nullptr) {
    std::string name = e->d_name;
    if (name.rfind("ttyUSB", 0) == 0) ports.push_back("/dev/" + name);
  }
  closedir(d);
  std::sort(ports.begin(), ports.end());
  return ports;
}
// 읽은 레지스터들을 word_order 에 따라 정수 raw 로 합성한다.
uint32_t combine(const std::vector<uint16_t>& regs, WordOrder order) {
  if (regs.empty()) return 0;
  if (regs.size() == 1) {
    uint16_t v = regs[0];
    if (order == WordOrder::Little) {
      // 16비트 내 바이트 스왑
      v = static_cast<uint16_t>((v >> 8) | (v << 8));
    }
    return v;
  }
  // 32비트: 상위/하위 레지스터 선택
  uint32_t hi = regs[0];
  uint32_t lo = regs[1];
  switch (order) {
    case WordOrder::Big:            return (hi << 16) | lo;
    case WordOrder::BigWordSwap:    return (lo << 16) | hi;
    case WordOrder::Little:         return (hi << 16) | lo;  // 워드는 유지
    case WordOrder::LittleWordSwap: return (lo << 16) | hi;
  }
  return (hi << 16) | lo;
}
}  // namespace

Status LgubSensor::open() {
  return serial_.open(cfg_.port, cfg_.baud, cfg_.parity, cfg_.data_bits,
                      cfg_.stop_bits);
}

void LgubSensor::close() { serial_.close(); }

DistanceReading LgubSensor::read_distance() {
  DistanceReading out = read_once();

  if (is_comm_failure(out.status)) {
    ++consecutive_failures_;
    if (cfg_.auto_reconnect &&
        consecutive_failures_ >= cfg_.reconnect_after_failures) {
      log("통신 연속 실패 " + std::to_string(consecutive_failures_) +
          "회 → 재접속 시도");
      if (reconnect() == Status::Ok) {
        consecutive_failures_ = 0;
        log("재접속 성공: " + cfg_.port);
        out = read_once();  // 복구 즉시 한 번 재시도
      } else {
        log("재접속 실패 — 다음 주기에 재시도");
      }
    }
  } else {
    consecutive_failures_ = 0;  // 통신 성공(대상 유무 무관)이면 카운터 리셋
  }
  return out;
}

Status LgubSensor::reconnect() {
  serial_.close();

  // 후보: 설정 포트 우선, 그다음(옵션) /dev/ttyUSB* 전체.
  std::vector<std::string> candidates = {cfg_.port};
  if (cfg_.rescan_ports) {
    for (const auto& p : list_usb_ports()) {
      if (p != cfg_.port) candidates.push_back(p);
    }
  }

  for (const auto& port : candidates) {
    if (serial_.open(port, cfg_.baud, cfg_.parity, cfg_.data_bits,
                     cfg_.stop_bits) != Status::Ok) {
      continue;
    }
    // 실제로 응답하는지 1회 검증(프레임 성립 = 통신 복구).
    ModbusRtu mb(serial_);
    ReadResult r = mb.read_registers(cfg_.slave_id, cfg_.function,
                                     cfg_.distance_register, cfg_.register_count,
                                     cfg_.response_timeout_ms,
                                     cfg_.inter_frame_delay_ms);
    if (r.status == Status::Ok) {
      cfg_.port = port;  // 응답 확인된 포트로 확정
      return Status::Ok;
    }
    serial_.close();  // 열렸지만 무응답 → 다음 후보
  }
  return Status::Timeout;
}

DistanceReading LgubSensor::read_once() {
  DistanceReading out;
  ModbusRtu mb(serial_);
  ReadResult r = mb.read_registers(cfg_.slave_id, cfg_.function,
                                   cfg_.distance_register, cfg_.register_count,
                                   cfg_.response_timeout_ms,
                                   cfg_.inter_frame_delay_ms);
  out.status = r.status;
  out.raw_registers = r.registers;
  if (r.status != Status::Ok) return out;  // 통신 자체 실패

  out.raw = combine(r.registers, cfg_.word_order);
  out.distance_m = static_cast<double>(out.raw) * cfg_.scale_to_meter;

  // 실측 확정: reg2=0 은 대상 없음/범위 초과/빔 이탈 (무효 센티넬).
  if (out.raw == 0) {
    out.status = Status::NoTarget;
    out.valid = false;
    return out;
  }
  // 유효 범위 밖 (사각지대 근접 클램핑, 최대 초과 등) 필터.
  if (out.distance_m < cfg_.min_valid_m || out.distance_m > cfg_.max_valid_m) {
    out.status = Status::OutOfRange;
    out.valid = false;
    return out;
  }
  out.valid = true;
  return out;
}

ReadResult LgubSensor::read_registers(uint16_t start_addr, uint16_t count) {
  ModbusRtu mb(serial_);
  return mb.read_registers(cfg_.slave_id, cfg_.function, start_addr, count,
                           cfg_.response_timeout_ms, cfg_.inter_frame_delay_ms);
}

Status LgubSensor::write_register(uint16_t addr, uint16_t value) {
  ModbusRtu mb(serial_);
  return mb.write_single_register(cfg_.slave_id, addr, value,
                                  cfg_.response_timeout_ms,
                                  cfg_.inter_frame_delay_ms);
}

}  // namespace lorddom
