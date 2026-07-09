// SPDX-License-Identifier: MIT
// lgub_probe : LORDDOM LGUB 센서 자동 프로빙 도구 (reverse engineering).
//
// 공식 문서가 없으므로, 실물을 연결한 뒤 이 도구로 아래 3가지를 역산한다:
//   1) 슬레이브 주소 + 보드레이트 (통신 성립 조건)
//   2) 거리값이 들어있는 레지스터 주소 + function code(0x03/0x04)
//   3) raw -> 실제거리 배율 (알려진 거리에 대고 반복 읽어 관찰)
//
// 사용:
//   lgub_probe scan   [--port /dev/ttyUSB0]
//       → 보드레이트 x 슬레이브주소 전수 조사, 응답하는 조합 탐색
//   lgub_probe regs   --port .. --baud 9600 --id 1 [--func 3|4]
//       → 레지스터 0x0000~0x000F 를 훑어 값이 나오는 주소 탐색
//   lgub_probe watch  --port .. --baud 9600 --id 1 --reg 0 [--func 3|4]
//       → 해당 레지스터를 반복 읽어 raw 관찰(배율 역산용)

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "lorddom_lgub/lgub_sensor.hpp"
#include "lorddom_lgub/modbus_rtu.hpp"
#include "lorddom_lgub/serial_port.hpp"

using namespace lorddom;

namespace {

struct Args {
  std::string cmd;
  std::string port = "/dev/ttyUSB0";
  int baud = 9600;
  int id = 1;
  int func = 3;
  int reg = 0;
  int count = 1;
};

const int kBaudCandidates[] = {9600, 115200, 4800, 19200, 38400, 57600};

Args parse(int argc, char** argv) {
  Args a;
  if (argc >= 2) a.cmd = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string k = argv[i];
    auto next = [&](int def) { return (i + 1 < argc) ? std::atoi(argv[++i]) : def; };
    if (k == "--port" && i + 1 < argc) a.port = argv[++i];
    else if (k == "--baud") a.baud = next(a.baud);
    else if (k == "--id") a.id = next(a.id);
    else if (k == "--func") a.func = next(a.func);
    else if (k == "--reg") a.reg = next(a.reg);
    else if (k == "--count") a.count = next(a.count);
  }
  return a;
}

FunctionCode fc(int f) {
  return (f == 4) ? FunctionCode::ReadInput : FunctionCode::ReadHolding;
}

// 단일 (baud, id, func, reg) 조합을 1회 시도한다.
Status try_read(const std::string& port, int baud, int id, int func,
                uint16_t reg, uint16_t count, std::vector<uint16_t>& out) {
  SerialPort sp;
  Status os = sp.open(port, baud, 'N', 8, 1);
  if (os != Status::Ok) return os;
  ModbusRtu mb(sp);
  ReadResult r = mb.read_registers(static_cast<uint8_t>(id), fc(func), reg,
                                   count, 250, 15);
  out = r.registers;
  return r.status;
}

int cmd_scan(const Args& a) {
  printf("[scan] port=%s : 보드레이트 x 슬레이브주소 전수 조사\n", a.port.c_str());
  printf("       (function 0x03, 레지스터 0x0000 1개 읽기 기준)\n\n");
  int found = 0;
  for (int baud : kBaudCandidates) {
    printf("  baud=%-6d ", baud);
    fflush(stdout);
    for (int id = 1; id <= 247; ++id) {
      std::vector<uint16_t> out;
      Status s = try_read(a.port, baud, id, 3, 0x0000, 1, out);
      if (s == Status::Ok || s == Status::ExceptionResponse) {
        printf("\n  >>> 응답! baud=%d slave_id=%d status=%s", baud, id,
               to_string(s));
        if (s == Status::Ok && !out.empty())
          printf(" reg[0]=0x%04X(%u)", out[0], out[0]);
        printf("\n  baud=%-6d ", baud);
        ++found;
      }
    }
    printf("done\n");
  }
  printf("\n[scan] 완료. 응답 조합 %d개 발견.\n", found);
  if (found == 0)
    printf("  힌트: 배선(A/B, GND), 종단저항, 전원, TX/RX 교차를 확인하세요.\n");
  return found > 0 ? 0 : 1;
}

int cmd_regs(const Args& a) {
  printf("[regs] port=%s baud=%d id=%d func=0x%02X : 0x0000~0x000F 스캔\n\n",
         a.port.c_str(), a.baud, a.id, a.func);
  int hits = 0;
  for (uint16_t reg = 0x0000; reg <= 0x000F; ++reg) {
    std::vector<uint16_t> out;
    Status s = try_read(a.port, a.baud, a.id, a.func, reg, 1, out);
    printf("  reg 0x%04X : %-18s", reg, to_string(s));
    if (s == Status::Ok && !out.empty()) {
      printf(" -> 0x%04X (%u)", out[0], out[0]);
      ++hits;
    }
    printf("\n");
  }
  printf("\n[regs] 값이 읽힌 레지스터 %d개. 거리와 비례해 변하는 주소를 watch로 확인하세요.\n",
         hits);
  return 0;
}

int cmd_watch(const Args& a) {
  printf("[watch] port=%s baud=%d id=%d func=0x%02X reg=0x%04X count=%d\n",
         a.port.c_str(), a.baud, a.id, a.func, a.reg, a.count);
  printf("  센서를 '알려진 거리'(예: 100mm, 200mm, 500mm)에 대고 raw 변화를 관찰하세요.\n");
  printf("  raw 가 mm면 배율 0.001, cm면 0.01, (mm x10)이면 0.0001 입니다. Ctrl+C 종료.\n\n");
  SerialPort sp;
  if (sp.open(a.port, a.baud, 'N', 8, 1) != Status::Ok) {
    printf("  포트 열기 실패: %s\n", a.port.c_str());
    return 1;
  }
  ModbusRtu mb(sp);
  while (true) {
    ReadResult r = mb.read_registers(static_cast<uint8_t>(a.id), fc(a.func),
                                     static_cast<uint16_t>(a.reg),
                                     static_cast<uint16_t>(a.count), 250, 15);
    printf("  status=%-14s", to_string(r.status));
    if (r.status == Status::Ok) {
      printf(" regs=[");
      for (size_t i = 0; i < r.registers.size(); ++i)
        printf("%s0x%04X(%u)", i ? " " : "", r.registers[i], r.registers[i]);
      printf("]");
    } else if (r.status == Status::ExceptionResponse) {
      printf(" exc=0x%02X", r.exception_code);
    }
    printf("\n");
    fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return 0;
}

void usage() {
  printf(
      "lgub_probe - LORDDOM LGUB 센서 자동 프로빙 도구\n\n"
      "  lgub_probe scan  [--port /dev/ttyUSB0]\n"
      "  lgub_probe regs  --port .. --baud 9600 --id 1 [--func 3|4]\n"
      "  lgub_probe watch --port .. --baud 9600 --id 1 --reg 0 "
      "[--func 3|4] [--count 1]\n");
}

}  // namespace

int main(int argc, char** argv) {
  Args a = parse(argc, argv);
  if (a.cmd == "scan") return cmd_scan(a);
  if (a.cmd == "regs") return cmd_regs(a);
  if (a.cmd == "watch") return cmd_watch(a);
  usage();
  return 2;
}
