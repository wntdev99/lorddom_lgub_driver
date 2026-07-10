# 초음파 센서 배치 시뮬레이터

mW2.0 로봇(URDF)에 초음파(`ultrasonic`) 센서를 3D로 배치·검증하고, 그 결과가
ROS2 nav2 2D costmap에 어떻게 반영되는지 확인하는 브라우저 도구.

요구사항 명세: [`../docs/simulator_spec.md`](../docs/simulator_spec.md)

## 실행

로봇 mesh(약 40MB)는 용량 때문에 git에 넣지 않는다. 클론 후 **1회 복사**가 필요하다.
또한 `file://`로 직접 열면 CORS로 mesh 로딩이 막히므로 **로컬 정적 서버**가 필요하다.

```bash
cd simulator

# 1) 최초 1회: 원본 패키지에서 mesh 복사 (기본 원본 경로 사용)
./setup_robot.sh
#   원본 경로가 다르면: ./setup_robot.sh /경로/to/w_type_mw

# 2) 정적 서버 실행
python3 -m http.server 8791

# 3) 브라우저에서 접속
#    http://localhost:8791
```

> mesh 없이 서버만 켜면 로봇이 뜨지 않는다(콘솔에 mesh 404). 반드시 1)을 먼저 실행.

## 구조

```
simulator/
  index.html        # 앱 본체 (ES module)
  lib/              # 인라인 번들 라이브러리 (외부 CDN 없음)
    three.module.js       three.js r160
    OrbitControls.js      뷰 조작
    STLLoader.js          STL mesh
    ColladaLoader.js      DAE (urdf-loader 의존, TGALoader import 경로 패치됨)
    TGALoader.js
    URDFLoader.js         urdf-loader 0.13.1
    URDFClasses.js
  robot/            # 대상 로봇 자체 포함
    robot.urdf            mW2.0 (flat URDF, package:// 두 이름 → ./robot 매핑)
    meshes/*.STL          링크 메시 22개
```

## 진행 상태 (마일스톤)

- [x] **M1** — 3D 뷰 + URDF 로딩 (Z-up ROS 좌표계, 관절 조작, 자동 시점맞춤)
- [x] **M2** — 양측 3D 통로 + 프리미티브 장애물 + 제자리 선회 스윕
- [x] **M3** — 다중 `ultrasonic` 센서 배치 (부모링크·6DOF·FOV·min/max_range, 콘 시각화, 최근접 반사 감지)
- [x] **M4** — nav2 RangeSensorLayer costmap 재현 (5cm, γ·δ·4구간 sensor_model·정규화 베이즈, 감지 상세 리드아웃)
- [x] **M5** — 커버리지/사각 맵 + 센서 간 간섭(crosstalk) 경고
- [x] **M6** — 기존 URDF에 센서 반영 내보내기 (link+fixed joint+gazebo/gpu_lidar, 복사/다운로드)

## 참고

- 좌표계: X 전방 · Y 좌 · Z 상 (ROS REP-103).
- 로봇 에셋은 `/home/jeongmin/Package/ros2/w_type_mw` 에서 복사해 자체 포함.
