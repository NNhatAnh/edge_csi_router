cmake_minimum_required(VERSION 3.10)
project(EdgeCSIRouter LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(edge_csi_daemon
  src/main.cpp
  src/csi_parser.cpp
  src/motion.cpp
  src/breath.cpp
  src/prober.cpp
  src/cfr_controller.cpp
  src/web_server.cpp
  src/wave_detector.cpp
  src/air_draw.cpp
  src/csi_rf_model1.c
)

target_include_directories(edge_csi_daemon PRIVATE src)
target_compile_options(edge_csi_daemon PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(edge_csi_daemon PRIVATE pthread m)
target_link_options(edge_csi_daemon PRIVATE -static)

add_executable(test_air_draw_kinematics
  tools/test_air_draw_kinematics.cpp
  src/air_draw.cpp
  src/csi_parser.cpp
)
target_include_directories(test_air_draw_kinematics PRIVATE src)
target_link_libraries(test_air_draw_kinematics PRIVATE pthread m)

file(COPY web DESTINATION "${CMAKE_BINARY_DIR}")

include(GNUInstallDirs)
install(TARGETS edge_csi_daemon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY web/ DESTINATION "${CMAKE_INSTALL_DATADIR}/edge-csi-router/web")
