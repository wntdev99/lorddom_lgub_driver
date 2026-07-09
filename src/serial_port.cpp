// SPDX-License-Identifier: MIT
#include "lorddom_lgub/serial_port.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>

namespace lorddom {

namespace {
// 정수 baud → termios 상수. 지원 밖이면 B0 반환(=오류 표시).
speed_t to_speed(int baud) {
  switch (baud) {
    case 1200: return B1200;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default: return B0;
  }
}
}  // namespace

SerialPort::~SerialPort() { close(); }

Status SerialPort::open(const std::string& device, int baud, char parity,
                        int data_bits, int stop_bits) {
  close();

  speed_t speed = to_speed(baud);
  if (speed == B0) return Status::InvalidArgument;

  fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) return Status::SerialError;

  // 블로킹 read 를 원치 않으므로 select() 로 타임아웃을 직접 제어한다.
  // 여기서는 플래그만 정리한다.
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags < 0 || fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK) < 0) {
    close();
    return Status::SerialError;
  }

  struct termios tio;
  std::memset(&tio, 0, sizeof(tio));
  if (tcgetattr(fd_, &tio) != 0) {
    close();
    return Status::SerialError;
  }

  cfmakeraw(&tio);  // 원시 바이너리 모드 (Modbus RTU 필수)

  if (cfsetispeed(&tio, speed) != 0 || cfsetospeed(&tio, speed) != 0) {
    close();
    return Status::SerialError;
  }

  // 데이터 비트
  tio.c_cflag &= ~CSIZE;
  switch (data_bits) {
    case 7: tio.c_cflag |= CS7; break;
    case 8: tio.c_cflag |= CS8; break;
    default: close(); return Status::InvalidArgument;
  }

  // 패리티
  switch (parity) {
    case 'N': case 'n':
      tio.c_cflag &= ~PARENB;
      break;
    case 'E': case 'e':
      tio.c_cflag |= PARENB;
      tio.c_cflag &= ~PARODD;
      break;
    case 'O': case 'o':
      tio.c_cflag |= PARENB;
      tio.c_cflag |= PARODD;
      break;
    default: close(); return Status::InvalidArgument;
  }

  // 스톱 비트
  if (stop_bits == 2) tio.c_cflag |= CSTOPB;
  else if (stop_bits == 1) tio.c_cflag &= ~CSTOPB;
  else { close(); return Status::InvalidArgument; }

  // 로컬 + 수신 활성, 하드웨어 흐름제어 없음
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~CRTSCTS;

  // read 는 select() 로 제어하므로 termios 타임아웃은 비활성(0)
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
    close();
    return Status::SerialError;
  }

  tcflush(fd_, TCIOFLUSH);
  return Status::Ok;
}

void SerialPort::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

Status SerialPort::flush() {
  if (fd_ < 0) return Status::NotOpen;
  return (tcflush(fd_, TCIOFLUSH) == 0) ? Status::Ok : Status::SerialError;
}

Status SerialPort::write_all(const uint8_t* data, size_t len) {
  if (fd_ < 0) return Status::NotOpen;
  size_t written = 0;
  while (written < len) {
    ssize_t n = ::write(fd_, data + written, len - written);
    if (n < 0) {
      if (errno == EINTR) continue;
      return Status::SerialError;
    }
    written += static_cast<size_t>(n);
  }
  // 송신 완료(라인에 실제로 나갈 때까지) 대기.
  tcdrain(fd_);
  return Status::Ok;
}

Status SerialPort::read_exact(std::vector<uint8_t>& out, size_t want,
                              int timeout_ms) {
  if (fd_ < 0) return Status::NotOpen;
  if (want == 0) return Status::Ok;

  size_t got = 0;
  uint8_t buf[256];

  // 남은 시간을 select 로 관리. 바이트가 조금씩 도착해도 want 까지 누적.
  int remaining_ms = timeout_ms;
  while (got < want) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);

    struct timeval tv;
    tv.tv_sec = remaining_ms / 1000;
    tv.tv_usec = (remaining_ms % 1000) * 1000;

    int sel = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (sel < 0) {
      if (errno == EINTR) continue;
      return Status::SerialError;
    }
    if (sel == 0) return Status::Timeout;  // 남은 시간 소진

    size_t chunk = want - got;
    if (chunk > sizeof(buf)) chunk = sizeof(buf);
    ssize_t n = ::read(fd_, buf, chunk);
    if (n < 0) {
      if (errno == EINTR) continue;
      return Status::SerialError;
    }
    if (n == 0) return Status::Timeout;  // EOF/무응답
    out.insert(out.end(), buf, buf + n);
    got += static_cast<size_t>(n);

    // 단순화를 위해 총 타임아웃 예산을 그대로 유지한다.
    // (select 가 다시 즉시 반환되면 남은 바이트를 계속 읽는다.)
  }
  return Status::Ok;
}

}  // namespace lorddom
