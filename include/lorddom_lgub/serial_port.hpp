// SPDX-License-Identifier: MIT
// POSIX termios 기반 시리얼 포트 래퍼.
// RS485-USB 어댑터(FTDI/CH340/CP210x 등)는 방향 전환(DE/RE)을 하드웨어에서
// 자동 처리하므로, 소프트웨어 관점에서는 일반 시리얼과 동일하게 다룬다.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lorddom_lgub/types.hpp"

namespace lorddom {

class SerialPort {
 public:
  SerialPort() = default;
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  // 포트를 열고 baud/parity/data/stop 을 설정한다.
  Status open(const std::string& device, int baud, char parity, int data_bits,
              int stop_bits);
  void close();
  bool is_open() const { return fd_ >= 0; }

  // 입출력 버퍼를 비운다 (요청 전 잔여 데이터 제거).
  Status flush();

  // 전체 버퍼를 기록한다. 부분 기록은 오류로 처리한다.
  Status write_all(const uint8_t* data, size_t len);

  // 정확히 want 바이트를 읽거나 timeout_ms 안에 실패한다.
  // 읽은 바이트는 out 뒤에 append 된다. 부족분은 Timeout.
  Status read_exact(std::vector<uint8_t>& out, size_t want, int timeout_ms);

  int native_handle() const { return fd_; }

 private:
  int fd_ = -1;
};

}  // namespace lorddom
