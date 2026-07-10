---
name: lgub-app
description: LGUB 거리센서용 간단한 앱(브라우저 대시보드)을 즉석에서 띄워준다. 하드웨어팀·비전문가가 "앱 만들어줘", "앱 하나 띄워줘", "화면 띄워줘", "대시보드 띄워줘", "보기 좋게 화면으로 보여줘", "GUI로 보여줘" 라고 하면 발동. lgub_webapp(SDK 기반 웹서버)을 백그라운드로 실행하고 브라우저를 열어준다. 사용자는 터미널을 만질 필요가 없다.
---

# LGUB 간단 앱(웹 대시보드) 띄워주기

하드웨어팀이 "앱 하나 띄워줘"라고 하면, **브라우저로 여는 실시간 거리 대시보드**를
즉석에서 띄운다(큰 숫자 + 게이지 + 상태, 250ms 갱신). 앱은 SDK(`LgubSensor`)를 직접
호출하는 C++ 웹서버 `lgub_webapp` 이며 의존성이 없다.

## 띄우기 (사용자는 아무것도 안 함)
1. 바이너리 확인, 없으면 빌드:
   `cmake -S . -B build >/dev/null && cmake --build build -j >/dev/null`
2. 웹서버를 **백그라운드로** 실행(run_in_background):
   `./build/lgub_webapp --http 8080`
3. 브라우저를 대신 열어준다: `xdg-open http://localhost:8080`
   (열기 실패 시 사용자에게 "브라우저에서 http://localhost:8080 을 여세요" 안내)
4. "화면이 떴습니다. 값이 실시간으로 갱신됩니다" 라고 알린다.

## 끄기
사용자가 "그만/꺼줘/닫아줘" 하면 서버를 종료한다:
`pkill -x lgub_webapp`  (정확 매칭. `pgrep -f lgub_webapp` 는 검사 명령 자신을
매칭하는 오탐이 있으니 상태 확인은 `ps -C lgub_webapp` 또는 포트 `ss -ltn | grep :8080` 로 한다.)

## 포트/충돌
- 기본 8080. 이미 쓰는 중이면 `--http 8090` 등 다른 포트로 띄우고 그 URL을 안내한다.
- 서버는 localhost 전용으로 바인드된다(외부 노출 없음, 안전).

## 다른 앱을 원하면 (SDK 기반 확장)
"버튼으로 로깅 시작/정지", "임계값 넘으면 빨간불/알림", "여러 센서 한 화면" 등
추가 요청은 **SDK 기반으로 직접 개발**한다. `lgub_webapp.cpp` 를 복제·수정하거나
새 타깃을 CMake 에 추가해 빌드·실행하고, 시리얼/Modbus 는 재구현하지 말고
`LgubSensor`(read_distance/read_registers/autodetect_port)를 재사용한다.
기본은 읽기 전용. 설정 변경(write)은 명시적 확인 후에만.

## 참고
- 값만 대화창으로 보고받고 싶으면 lgub-monitor, 연결 점검은 lgub-connection,
  단발/로깅/스트리밍은 lgub-sensor.
