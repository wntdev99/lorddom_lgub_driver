# 초음파 센서 배치 시뮬레이터 — 요구사항 명세서

> 목적: 로봇(URDF)에 **초음파(ultrasonic) 센서를 어디에·몇 개·어떤 포즈로** 배치할지
> 3D로 설계·검증하고, 그 결과가 실제 ROS2 nav2 2D costmap에 어떻게 반영되는지 확인한다.
> 참조물(`dypa21_multisensor.html`)에서는 **기능 아이디어만** 가져오며, 코드는 백지에서 새로 구현한다.
>
> 상태 태그: `[수집]` 아이디어 접수 · `[확정]` 반영 결정 · `[구현]` 개발 완료
> 출처 태그: `[참조]` HTML에서 가져옴 · `[신규]` 커맨더 신규 요청 · `[검증]` 소스/문서 리서치로 확정

---

## 0. 확정된 전제 조건 (리서치 근거)

| # | 전제 | 근거 |
|---|------|------|
| P1 | 브라우저 3D는 `three.js`(≥0.152) + `urdf-loader`(0.13.1) 사용 | urdf-loaders(gkjohnson), npm registry |
| P2 | 라이브러리 인라인 번들 시 **외부 CDN 없는 자체완결**은 가능하나, `file://` 직접 열기는 CORS로 mesh 로딩 불가 → **로컬 정적 서버 필요** | urdf-loader README, CORS |
| P3 | 입력은 **flat URDF + STL/glTF mesh** (xacro·DAE 비권장) | xacro는 브라우저 미지원, DAE 머티리얼 이슈 |
| P4 | Jazzy ↔ **Gazebo Harmonic(gz-sim 8)**. gz-sim에 **네이티브 sonar/ultrasonic 없음** → 좁은 콘 `gpu_lidar`로 모델링 | gazebosim.org/docs/harmonic, gz-sensors repo |
| P5 | `sensor_msgs/Range` ↔ `gz.msgs.LaserScan` 브리지 매핑은 **2025-05 ros_gz에 추가** → 구버전 바이너리엔 없을 수 있음 | ros_gz PR #740, issue #586 |
| P6 | 기술 스택 = **(가) 자체완결 단일 HTML + 로컬 정적 서버** 확정. three.js/urdf-loader 등 라이브러리를 파일에 인라인 번들(외부 CDN 없음). 실행은 정적 서버 경유(P2). | 커맨더 확정 |

---

## 1. 대상 로봇 (입력 URDF)

- 경로: `/home/jeongmin/Package/ros2/w_type_mw/urdf/robot.urdf`
- 로봇명: **mW2.0** (SolidWorks URDF Exporter 산출, flat URDF)
- 패키지 루트: `/home/jeongmin/Package/ros2/w_type_mw/` (`package://w_type_mw/meshes/*.STL`)
- **호환성: 우수** — flat URDF, mesh 전부 STL(40 참조/23 파일), 단일 패키지. urdf-loader `packages={w_type_mw: <루트>}` 매핑만으로 로딩 가능.
- 링크 22 / 조인트 21. 구동계: 4륜 조향(continuous: steering_RH/RT/LH/LT + wheel), 도어(revolute/fixed), 리니어암/도어(prismatic).
- 기존 센서(참고 좌표, base_link 기준):
  - `laser_2D_link`: xyz=(-0.299, -0.047, 0.100), yaw=-90°
  - `laser_3D_link`: xyz=(0.250, 0.200, 0.942), yaw=+90°
  - `imu_link`: xyz=(0.293, 0.212, 0.565)
  - `front_view_middle_camera(camera_FH)`: xyz=(0.337, -0.023, 0.202), roll≈180°, pitch≈-10°
  - `base_footprint → base_link`: z=0.234
- **초음파 부모 링크 기본값**: `base_link` (필요 시 다른 링크 선택 가능)

---

## 2. 기능 명세

### A. 3D 뷰 / 조작
- `[확정][참조+신규]` **단일 3D 뷰** — 참조물의 듀얼 뷰(평면+측면 분리) 대신, **Gazebo처럼 자유 회전 가능한 하나의 3D 씬**. (듀얼 뷰 폐기)
- `[확정]` **뷰 조작** — 회전(orbit)·줌·패닝(OrbitControls 급).
- `[확정]` **URDF 기반 렌더링** — ROS2 Jazzy 호환 URDF를 로드해 그대로 표시.
- `[확정]` **파라미터 변경 시 뷰 유지** — 값 조정해도 카메라 시점 리셋 안 됨.

### B. 로봇 / 환경
- `[확정]` **로봇 형상·치수는 전부 URDF에서 계산** (별도 폭/길이 입력 없음).
- `[확정]` **회전 범위 표시 = 제자리 선회 스윕(swept envelope)** — 로봇이 제자리로 360° 돌 때 footprint 바깥 모서리가 그리는 원(쓸리는 영역)을 표시. "이 통로에서 제자리로 돌 수 있나 / 회전 중 어디를 센서가 커버해야 하나" 판단용. (조향 선회반경은 채택 안 함)
- `[확정]` **3D 통로** — 로봇 **양 사이드**에 벽. 통로 **폭·길이 조절 가능**.
- `[확정]` **간단한 장애물 추가** — 박스·원기둥 등 프리미티브를 씬에 추가/이동/삭제.

### C. 감지 결과 / costmap
- `[확정]` **삭제**: 통로 편측 여유, 콘→벽 거리, 통과 판정(OK/NG). (참조물 대비 제거)
- `[확정]` **감지점 2D 좌표화** — 각 초음파 콘 안 최근접 반사점을 지면(X,Y) 2D 좌표로 산출.
- `[확정]` **nav2 2D costmap 뷰** — 위 감지를 실제 nav2 RangeSensorLayer 모델(§4)로 grid에 렌더.
- `[확정][신규]` **감지 상세 리드아웃** — 초음파에 무언가 걸리면:
  - 인식 거리(measured range r, m/cm)
  - 그 range 호(arc)가 **costmap에서 몇 픽셀을 차지하는지** (기본 resolution **5cm/셀**)
  - 관련 셀들의 θ·φ·γ·δ·sensor_model·posterior 값 등 계산 과정
  - (참조물 "Show Me"의 셀 클릭 분석을 이 방향으로 확장)

### D. 센서 배치 (다중 초음파)
- `[확정]` 센서 리스트 추가/삭제/선택.
- `[확정]` 장착 면(front/rear/left/right) + 면 위 오프셋 (입력 편의).
- `[확정]` 수평 FOV(φ) / 수직 FOV(θ) / 수평틸트(β) / 수직틸트(α).
- `[확정]` 초음파 물리 모델 — 콘 내부 **최근접 반사체 1개**를 측정.
- `[확정][신규]` **네이밍 규칙: `ultrasonic`** (예: `ultrasonic_front_0`, frame `ultrasonic_front_0_link`, 토픽 `/ultrasonic/front_0`).

#### D-1. 센서 config 스키마 (추가 확정 항목, 전부 포함)
| 필드 | 설명 | 필수 |
|------|------|------|
| `name` | 센서 이름(ultrasonic_*) | ✔ |
| `parent_link` | 고정될 URDF 링크(기본 base_link) | ✔ |
| `xyz`, `rpy` | 부모 링크 기준 6-DOF 포즈(면+오프셋+틸트를 내부 변환) | ✔ |
| `min_range` | 사각지대(blind zone) 하한 | ✔ |
| `max_range` | 최대 감지거리 | ✔ |
| `fov_h` (φ) | 수평 시야각(=Range.field_of_view) | ✔ |
| `fov_v` (θ) | 수직 시야각 | ✔ |
| `update_rate` | Hz | ✔ |
| `frame_id` | TF 프레임명 | ✔ |
| `topic` | 발행 토픽명 | ✔ |
| `cone_rays` | 콘 근사 광선 수(내부 계산용, 정확도/성능) | 내부 |

### E. 내보내기
- `[확정]` **기존 URDF에 초음파 센서를 반영한 URDF 출력**.
- `[확정][검증]` 형식: 센서마다 `<link>` + `<joint type="fixed">`(parent 기준 xyz/rpy) + `<gazebo reference><sensor type="gpu_lidar">`(좁은 콘: horizontal min/max_angle=±φ/2, vertical samples=1, range min/max, update_rate, `gz_frame_id`).
- `[확정]` Range 브리지 주의(P5) 안내 문구 포함.
- (부가) CSV/JSON 요약도 병행 출력 검토.

### F. nav2 costmap 정확 재현 → §4 참조
- `[확정][검증]` 참조 HTML은 실제 nav2와 **다름**. 아래 §4의 실제 소스 수식대로 구현.

### G. 신규 합의 기능
- `[확정][신규]` **커버리지 / 사각(blind-spot) 맵** — 전체 초음파 콘의 합집합을 로봇 주변에 표시해 "안 보이는 영역" 가시화. 배치 개수·위치 결정 근거.
- `[확정][신규]` **센서 간 간섭(crosstalk) 경고** — 콘이 서로 겹치는 초음파 쌍 표시(반향 간섭 위험).

---

## 3. 참조물에서 제외/변경한 것
- 듀얼 뷰(평면+측면 분리) → 단일 3D 뷰로 대체
- 통로 편측 여유 / 콘-벽 거리 / 통과 판정 → 제거
- DYP-A21 명칭 → 제거(센서 무관, 네이밍 `ultrasonic`)

---

## 4. nav2 RangeSensorLayer 정확 모델 [검증]

출처: `nav2_costmap_2d/plugins/range_sensor_layer.cpp` (ros-navigation/navigation2), docs.nav2.org.

**gamma(θ)** — 방위각 가중치 (`max_angle = field_of_view/2`):
```
γ(θ) = |θ|>max_angle ? 0 : 1 - (θ/max_angle)²
```
**delta(φ)** — 반경거리 가중치 (`phi_v` = 파라미터 `phi`, 기본 1.2):
```
δ(φ) = 1 - (1 + tanh(2·(φ - phi_v))) / 2
```
**sensor_model(r, φ, θ)** — 점유확률 (r=측정거리, δcell=costmap resolution):
```
λ = δ(φ)·γ(θ)
d = resolution
 φ < r-2·d·r :  (1-λ)·0.5
 φ < r-d·r   :  λ·0.5·((φ-(r-2·d·r))/(d·r))² + (1-λ)·0.5
 φ < r+d·r   :  λ·((1 - 0.5·((r-φ)/(d·r))²) - 0.5) + 0.5     ← 측정점 근방 피크(≤1.0)
 else        :  0.5
```
**베이즈 갱신** (log-odds 아님, 정규화 곱):
```
prior = cost/254
p = (sensor·prior) / (sensor·prior + (1-sensor)·(1-prior))
cost = p·254        (선형 매핑, LETHAL=254)
clear=true 시 sensor=0 (free로 끌어내림)
```
**콘 셀 선택**:
- bbox를 `θ±max_angle` 방향 `d·1.2`(하드코딩)로 확장.
- `inflate_cone < 1.0`일 때만 삼각형(barycentric)으로 콘 밖 제외. **기본 1.0이면 bbox 전체 마킹.**
**master grid 반영** (updateCosts):
- `p > mark_threshold(0.8)` → LETHAL(254)
- `p < clear_threshold(0.2)` → FREE(0)
- 중간 → 유지(NO_INFORMATION)
- 결합은 `max`(더 큰 cost만 덮음)
**입력 분기**: `min_range==max_range` → FIXED(±Inf만 유효), 그 외 VARIABLE(범위 밖 무시, `≥max && clear_on_max_reading`이면 clear).

**파라미터 기본값**: `phi=1.2`, `inflate_cone=1.0`, `clear_threshold=0.2`, `mark_threshold=0.8`, `clear_on_max_reading=false`, `input_sensor_type=ALL`. `field_of_view/min_range/max_range`는 메시지 필드.

**구현 함정**: ①log-odds 금지(정규화 곱) ②cost↔prob 선형 ③inflate_cone=1.0이면 bbox 전체 ④콘 확장 1.2배 반영 ⑤mark/clear는 확률이나 내부 정수 비교.

---

## 5. 열린 결정 / 다음 단계

**명세 동결(FROZEN): 2026-07-10 — 커맨더가 추가 기능 없음 확인, 기술 스택 (가) 확정.**

- [x] 기술 스택 = **(가) 자체완결 단일 HTML + 로컬 정적 서버**로 확정(P6)
- [x] costmap resolution **5cm 확정**. 표시 범위 = **모든 센서 max_range를 담는 크기까지 동적 확장**(가장 멀리 보는 센서 기준). — 커맨더 확정
- [x] 초음파 실제 스펙(min/max_range·FOV·update_rate) 기본값 — **도입 센서 정해지면 교정하기로 동의**. 그전까지 대표 기본값 사용.
- [x] 통로/장애물 재질 반사 가정 — **일단 보류**(모든 면을 이상 반사체로 가정). 추후 재검토.
- [x] 회전 범위 표시 = **제자리 선회 스윕 원**으로 확정 (조향 선회반경 미채택).

---

## 변경 이력
- 2026-07-10 최초 작성 — 참조 기능 선별 + 신규 합의(커버리지/간섭/감지 상세) + URDF(mW2.0) 분석 + nav2 모델 검증 반영.
