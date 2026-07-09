cmake_minimum_required(VERSION 3.16)
project(lorddom_lgub_driver LANGUAGES CXX VERSION 0.1.0)

# 순수 C++ SDK (ROS 비의존). 의존성 없음: termios + 자체 Modbus-RTU 구현.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- 코어 라이브러리 ---
add_library(lorddom_lgub
  src/types.cpp
  src/serial_port.cpp
  src/modbus_rtu.cpp
  src/lgub_sensor.cpp
)
target_include_directories(lorddom_lgub PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED)
target_link_libraries(lorddom_lgub PUBLIC Threads::Threads)

# --- 프로빙 도구 (reverse engineering CLI) ---
add_executable(lgub_probe tools/lgub_probe.cpp)
target_link_libraries(lgub_probe PRIVATE lorddom_lgub)

# --- 사용 예제 ---
add_executable(lgub_read_distance examples/read_distance.cpp)
target_link_libraries(lgub_read_distance PRIVATE lorddom_lgub)

# --- 자동 재접속 검증 데모 ---
add_executable(lgub_reconnect_demo tools/reconnect_demo.cpp)
target_link_libraries(lgub_reconnect_demo PRIVATE lorddom_lgub)

# --- 오프라인 테스트 (하드웨어 불필요) ---
enable_testing()
add_executable(test_crc16 test/test_crc16.cpp)
target_link_libraries(test_crc16 PRIVATE lorddom_lgub)
add_test(NAME crc16 COMMAND test_crc16)

# --- 설치 ---
install(TARGETS lorddom_lgub lgub_probe lgub_read_distance
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
