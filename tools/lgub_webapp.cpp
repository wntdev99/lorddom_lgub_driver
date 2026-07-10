// SPDX-License-Identifier: MIT
// lgub_webapp : 브라우저로 여는 초간단 실시간 거리 대시보드.
//
// 터미널을 못 쓰는 하드웨어팀을 위해, 브라우저에서 http://localhost:8080 만 열면
// 큰 숫자 + 게이지 + 상태가 실시간 갱신되는 화면을 띄운다.
// 의존성 없음(POSIX 소켓만). 센서 접근은 전부 SDK(LgubSensor)를 그대로 호출한다.
//
// 사용: lgub_webapp [--http 8080] [--port /dev/ttyUSBx]
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lorddom_lgub/lgub_sensor.hpp"

using namespace lorddom;

namespace {

// 브라우저에 보내는 단일 페이지(인라인 CSS/JS, /api 를 주기적으로 폴링).
const char* kHtml = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LGUB 거리 센서</title>
<style>
  :root{color-scheme:dark light}
  body{margin:0;font-family:system-ui,sans-serif;background:#0f1420;color:#e8edf5;
       display:flex;flex-direction:column;align-items:center;justify-content:center;
       min-height:100vh;gap:18px}
  h1{font-size:15px;font-weight:600;letter-spacing:.5px;color:#8fa3bf;margin:0}
  #dist{font-size:24vw;font-weight:800;line-height:1;font-variant-numeric:tabular-nums}
  #m{font-size:5vw;color:#8fa3bf;margin-top:-1vw}
  #barwrap{width:80vw;max-width:640px;height:26px;background:#1b2436;border-radius:14px;
           overflow:hidden;border:1px solid #263149}
  #bar{height:100%;width:0;background:linear-gradient(90deg,#3b82f6,#22d3ee);
       transition:width .12s linear}
  #status{font-size:4.5vw;font-weight:700;padding:6px 18px;border-radius:999px}
  .ok{background:#0d3320;color:#4ade80}
  .none{background:#3a2f10;color:#fbbf24}
  .err{background:#3a1414;color:#f87171}
  #meta{font-size:13px;color:#5f6f88}
  @media(min-width:700px){#dist{font-size:180px}#m,#status{font-size:28px}}
</style></head><body>
  <h1>LGUB 초음파 거리 센서</h1>
  <div id="dist">--</div>
  <div id="m">-- m</div>
  <div id="barwrap"><div id="bar"></div></div>
  <div id="status" class="none">연결 확인 중…</div>
  <div id="meta"></div>
<script>
const MAX_M=1.0;
async function tick(){
  try{
    const r=await fetch('/api',{cache:'no-store'});
    const d=await r.json();
    const s=document.getElementById('status');
    if(d.valid){
      const cm=(d.distance_m*100).toFixed(1);
      document.getElementById('dist').textContent=cm;
      document.getElementById('m').textContent=d.distance_m.toFixed(3)+' m ('+cm+' cm)';
      document.getElementById('bar').style.width=Math.min(100,d.distance_m/MAX_M*100)+'%';
      s.textContent='측정 중';s.className='ok';
    }else if(d.status==='NoTarget'){
      document.getElementById('dist').textContent='--';
      document.getElementById('m').textContent='-- m';
      document.getElementById('bar').style.width='0%';
      s.textContent='대상 없음';s.className='none';
    }else if(d.status==='OutOfRange'){
      s.textContent='범위 밖';s.className='none';
    }else{
      document.getElementById('dist').textContent='--';
      s.textContent='연결 문제 ('+d.status+')';s.className='err';
    }
    document.getElementById('meta').textContent='포트 '+d.port+' · '+new Date().toLocaleTimeString();
  }catch(e){
    const s=document.getElementById('status');s.textContent='서버 연결 끊김';s.className='err';
  }
}
setInterval(tick,250);tick();
</script></body></html>)HTML";

std::string http_resp(const std::string& ctype, const std::string& body) {
  std::string h = "HTTP/1.1 200 OK\r\nContent-Type: " + ctype +
                  "\r\nContent-Length: " + std::to_string(body.size()) +
                  "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
  return h + body;
}

void send_all(int fd, const std::string& s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = ::write(fd, s.data() + off, s.size() - off);
    if (n <= 0) break;
    off += (size_t)n;
  }
}

}  // namespace

int main(int argc, char** argv) {
  signal(SIGPIPE, SIG_IGN);

  int http_port = 8080;
  Config cfg;
  std::string forced_port;
  for (int i = 1; i < argc; ++i) {
    std::string k = argv[i];
    if (k == "--http" && i + 1 < argc) http_port = std::atoi(argv[++i]);
    else if (k == "--port" && i + 1 < argc) forced_port = argv[++i];
  }
  if (!forced_port.empty()) cfg.port = forced_port;
  else if (LgubSensor::autodetect_port(cfg) != Status::Ok)
    fprintf(stderr, "경고: 센서 자동탐지 실패. 연결되면 자동 재접속을 시도합니다.\n");

  LgubSensor sensor(cfg);
  sensor.set_log([](const std::string& m) { fprintf(stderr, "[SDK] %s\n", m.c_str()); });
  sensor.open();

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) { perror("socket"); return 1; }
  int yes = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost 전용(안전)
  addr.sin_port = htons(http_port);
  if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "포트 %d 바인드 실패(이미 사용 중일 수 있음)\n", http_port);
    return 1;
  }
  listen(srv, 8);
  printf("LGUB 대시보드 실행 중 → 브라우저에서 http://localhost:%d 여세요 (Ctrl+C 종료)\n",
         http_port);
  fflush(stdout);

  char buf[2048];
  for (;;) {
    int cli = accept(srv, nullptr, nullptr);
    if (cli < 0) continue;
    ssize_t n = ::read(cli, buf, sizeof(buf) - 1);
    if (n <= 0) { close(cli); continue; }
    buf[n] = '\0';

    // 요청 라인에서 경로만 파싱: "GET /path HTTP/1.1"
    std::string path = "/";
    if (strncmp(buf, "GET ", 4) == 0) {
      char* sp = strchr(buf + 4, ' ');
      if (sp) path.assign(buf + 4, sp - (buf + 4));
    }

    if (path.rfind("/api", 0) == 0) {
      DistanceReading r = sensor.read_distance();
      bool comm_ok = (r.status == Status::Ok || r.status == Status::NoTarget ||
                      r.status == Status::OutOfRange);
      char json[256];
      std::snprintf(json, sizeof(json),
                    "{\"valid\":%s,\"distance_m\":%.4f,\"status\":\"%s\","
                    "\"comm_ok\":%s,\"port\":\"%s\"}",
                    r.valid ? "true" : "false", r.distance_m, to_string(r.status),
                    comm_ok ? "true" : "false", sensor.config().port.c_str());
      send_all(cli, http_resp("application/json", json));
    } else if (path == "/") {
      send_all(cli, http_resp("text/html; charset=utf-8", kHtml));
    } else {
      std::string body = "not found";
      send_all(cli, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n"
                    "Connection: close\r\n\r\n" + body);
    }
    close(cli);
  }
  return 0;
}
