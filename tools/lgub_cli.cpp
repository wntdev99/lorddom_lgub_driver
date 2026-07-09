// SPDX-License-Identifier: MIT
// lgub : LORDDOM LGUB 초음파 거리센서 통합 CLI.
//
// 이 파일은 센서 로직을 재구현하지 않는다 — 오직 인자를 파싱해 SDK(LgubSensor)를
// 호출하는 얇은 껍데기다. 자연어 비서(Claude Skill)가 이 명령들을 불러 쓴다.
//
//   lgub measure [--json]                         현재 거리 1회
//   lgub stream  [--hz N] [--sec S] [--json]       실시간 스트리밍
//   lgub log     --out FILE [--sec S] [--hz N]     CSV 로깅 + 통계 요약
//   lgub doctor  [--json]                          포트 자동탐지·연결·진단
//   공통: [--port /dev/ttyUSBx]  (생략 시 자동탐지)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#include "lorddom_lgub/lgub_sensor.hpp"

using namespace lorddom;

namespace {

struct Args {
  std::string cmd;
  std::string port;      // 비면 자동탐지
  bool json = false;
  double hz = 5.0;
  double sec = -1.0;     // <0: 무한(stream), log는 기본 60
  std::string out;
};

Args parse(int argc, char** argv) {
  Args a;
  if (argc >= 2) a.cmd = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string k = argv[i];
    auto val = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
    if (k == "--port") a.port = val();
    else if (k == "--json") a.json = true;
    else if (k == "--hz") a.hz = std::atof(val().c_str());
    else if (k == "--sec") a.sec = std::atof(val().c_str());
    else if (k == "--out") a.out = val();
  }
  return a;
}

// ISO8601 로컬 타임스탬프 + epoch ms.
std::string iso_now(long long* epoch_ms_out = nullptr) {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
  if (epoch_ms_out) *epoch_ms_out = ms;
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  char full[48];
  std::snprintf(full, sizeof(full), "%s.%03d", buf, (int)(ms % 1000));
  return full;
}

// 포트를 확정(지정 or 자동탐지)하고 열린 센서를 준비한다.
// 실패하면 사람이 읽을 안내를 stderr 로 내고 nullptr 대신 Status 반환.
Status prepare(Config& cfg, const Args& a, bool announce) {
  if (!a.port.empty()) cfg.port = a.port;
  if (a.port.empty()) {
    Status ds = LgubSensor::autodetect_port(cfg);
    if (ds != Status::Ok) return ds;
    if (announce) fprintf(stderr, "포트 자동탐지: %s\n", cfg.port.c_str());
  }
  return Status::Ok;
}

const char* valid_kor(const DistanceReading& r) {
  if (r.valid) return "유효";
  if (r.status == Status::NoTarget) return "대상 없음";
  if (r.status == Status::OutOfRange) return "범위 밖";
  return to_string(r.status);
}

int cmd_measure(const Args& a) {
  Config cfg;
  if (prepare(cfg, a, !a.json) != Status::Ok) {
    if (a.json) printf("{\"ok\":false,\"error\":\"no_sensor\"}\n");
    else fprintf(stderr, "센서를 찾지 못했습니다. `lgub doctor` 로 진단하세요.\n");
    return 1;
  }
  LgubSensor sensor(cfg);
  if (sensor.open() != Status::Ok) { fprintf(stderr, "포트 열기 실패\n"); return 1; }

  DistanceReading r = sensor.read_distance();
  // ok = 통신 자체는 성공(대상 없음/범위 밖 포함). 통신 실패면 false.
  bool ok = (r.status == Status::Ok || r.status == Status::NoTarget ||
             r.status == Status::OutOfRange);
  if (a.json) {
    printf("{\"ok\":%s,\"valid\":%s,\"distance_m\":%.4f,\"raw\":%u,"
           "\"status\":\"%s\",\"port\":\"%s\"}\n",
           ok ? "true" : "false",
           r.valid ? "true" : "false", r.distance_m, r.raw,
           to_string(r.status), sensor.config().port.c_str());
  } else if (r.valid) {
    printf("현재 거리: %.3f m (%.1f cm)\n", r.distance_m, r.distance_m * 100.0);
  } else {
    printf("측정값 없음: %s\n", valid_kor(r));
  }
  return r.valid ? 0 : 2;
}

int cmd_stream(Args a) {
  if (a.sec < 0) a.sec = 0;  // 0 = 무한
  Config cfg;
  if (prepare(cfg, a, !a.json) != Status::Ok) {
    fprintf(stderr, "센서를 찾지 못했습니다. `lgub doctor` 로 진단하세요.\n");
    return 1;
  }
  LgubSensor sensor(cfg);
  if (sensor.open() != Status::Ok) { fprintf(stderr, "포트 열기 실패\n"); return 1; }

  if (a.hz <= 0) a.hz = 5.0;
  int period_ms = static_cast<int>(1000.0 / a.hz);
  using namespace std::chrono;
  auto start = steady_clock::now();
  if (!a.json)
    printf("스트리밍 시작 (%.1f Hz, %s). Ctrl+C 종료.\n", a.hz,
           a.sec > 0 ? (std::to_string((int)a.sec) + "초").c_str() : "무한");

  while (true) {
    long long ep;
    std::string ts = iso_now(&ep);
    DistanceReading r = sensor.read_distance();
    if (a.json) {
      printf("{\"t\":\"%s\",\"valid\":%s,\"distance_m\":%.4f,\"status\":\"%s\"}\n",
             ts.c_str(), r.valid ? "true" : "false", r.distance_m,
             to_string(r.status));
    } else if (r.valid) {
      printf("%s  %.3f m\n", ts.c_str(), r.distance_m);
    } else {
      printf("%s  [%s]\n", ts.c_str(), valid_kor(r));
    }
    fflush(stdout);
    if (a.sec > 0 &&
        duration_cast<milliseconds>(steady_clock::now() - start).count() >=
            (long long)(a.sec * 1000))
      break;
    std::this_thread::sleep_for(milliseconds(period_ms));
  }
  return 0;
}

int cmd_log(Args a) {
  if (a.out.empty()) {
    fprintf(stderr, "로깅 대상 파일이 필요합니다: --out FILE.csv\n");
    return 1;
  }
  if (a.sec < 0) a.sec = 60;  // 기본 60초
  if (a.hz <= 0) a.hz = 5.0;

  Config cfg;
  if (prepare(cfg, a, true) != Status::Ok) {
    fprintf(stderr, "센서를 찾지 못했습니다. `lgub doctor` 로 진단하세요.\n");
    return 1;
  }
  LgubSensor sensor(cfg);
  if (sensor.open() != Status::Ok) { fprintf(stderr, "포트 열기 실패\n"); return 1; }

  FILE* f = std::fopen(a.out.c_str(), "w");
  if (!f) { fprintf(stderr, "파일 열기 실패: %s\n", a.out.c_str()); return 1; }
  std::fprintf(f, "timestamp,epoch_ms,distance_m,raw,valid,status\n");

  int period_ms = static_cast<int>(1000.0 / a.hz);
  using namespace std::chrono;
  auto start = steady_clock::now();
  long long n = 0, n_valid = 0;
  double sum = 0, mn = 1e9, mx = -1e9;

  fprintf(stderr, "로깅 시작: %s (%.0f초, %.1f Hz)\n", a.out.c_str(), a.sec, a.hz);
  while (true) {
    long long ep;
    std::string ts = iso_now(&ep);
    DistanceReading r = sensor.read_distance();
    std::fprintf(f, "%s,%lld,%.4f,%u,%d,%s\n", ts.c_str(), ep, r.distance_m,
                 r.raw, r.valid ? 1 : 0, to_string(r.status));
    ++n;
    if (r.valid) {
      ++n_valid; sum += r.distance_m;
      if (r.distance_m < mn) mn = r.distance_m;
      if (r.distance_m > mx) mx = r.distance_m;
    }
    if (duration_cast<milliseconds>(steady_clock::now() - start).count() >=
        (long long)(a.sec * 1000))
      break;
    std::this_thread::sleep_for(milliseconds(period_ms));
  }
  std::fclose(f);

  double valid_pct = n ? 100.0 * n_valid / n : 0;
  double mean = n_valid ? sum / n_valid : 0;
  if (a.json) {
    printf("{\"ok\":true,\"file\":\"%s\",\"samples\":%lld,\"valid\":%lld,"
           "\"valid_pct\":%.1f,\"mean_m\":%.4f,\"min_m\":%.4f,\"max_m\":%.4f}\n",
           a.out.c_str(), n, n_valid, valid_pct, mean,
           n_valid ? mn : 0.0, n_valid ? mx : 0.0);
  } else {
    printf("로깅 완료: %s\n", a.out.c_str());
    printf("  표본 %lld개 중 유효 %lld개 (%.1f%%)\n", n, n_valid, valid_pct);
    if (n_valid)
      printf("  거리 평균 %.3f m, 최소 %.3f m, 최대 %.3f m\n", mean, mn, mx);
  }
  return 0;
}

int cmd_doctor(const Args& a) {
  std::vector<std::string> ports = LgubSensor::list_serial_ports();
  if (!a.json) {
    printf("=== LGUB 센서 진단 ===\n");
    printf("USB 시리얼 포트 %zu개 발견: ", ports.size());
    for (auto& p : ports) printf("%s ", p.c_str());
    printf("%s\n", ports.empty() ? "(없음)" : "");
  }
  Config cfg;
  Status ds = LgubSensor::autodetect_port(cfg);
  if (ds == Status::Ok) {
    LgubSensor sensor(cfg);
    sensor.open();
    DistanceReading r = sensor.read_distance();
    if (a.json) {
      printf("{\"ok\":true,\"port\":\"%s\",\"reading_valid\":%s,\"distance_m\":%.4f}\n",
             cfg.port.c_str(), r.valid ? "true" : "false", r.distance_m);
    } else {
      printf("✅ 센서 응답 정상: %s (9600/8N1, slave 1)\n", cfg.port.c_str());
      if (r.valid) printf("   현재 거리 %.3f m — 정상 측정 중\n", r.distance_m);
      else printf("   현재 [%s] — 통신은 정상, 대상만 없음\n", valid_kor(r));
    }
    return 0;
  }
  // 실패 안내
  if (a.json) { printf("{\"ok\":false,\"error\":\"no_response\"}\n"); return 1; }
  printf("❌ 응답하는 센서를 찾지 못했습니다. 점검하세요:\n");
  printf("  1) 센서 전원 10~30V DC (빨강 +, 검정 GND) 인가 여부\n");
  printf("  2) RS485 배선: 노랑=A(+), 초록=B(-) — 어댑터에 단단히\n");
  printf("  3) A/B 가 서로 바뀌지 않았는지\n");
  printf("  4) USB 어댑터 연결 (현재 포트: %s)\n",
         ports.empty() ? "없음" : ports.front().c_str());
  return 1;
}

void usage() {
  printf(
      "lgub - LORDDOM LGUB 초음파 거리센서 CLI\n\n"
      "  lgub measure [--json]                     현재 거리 1회\n"
      "  lgub stream  [--hz N] [--sec S] [--json]  실시간 스트리밍\n"
      "  lgub log     --out FILE [--sec S] [--hz N] [--json]  CSV 로깅+요약\n"
      "  lgub doctor  [--json]                     포트 자동탐지·연결·진단\n\n"
      "  공통: [--port /dev/ttyUSBx] (생략 시 자동탐지)\n");
}

}  // namespace

int main(int argc, char** argv) {
  Args a = parse(argc, argv);
  if (a.cmd == "measure") return cmd_measure(a);
  if (a.cmd == "stream") return cmd_stream(a);
  if (a.cmd == "log") return cmd_log(a);
  if (a.cmd == "doctor") return cmd_doctor(a);
  usage();
  return 2;
}
