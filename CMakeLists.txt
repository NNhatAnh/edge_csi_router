cmake_minimum_required(VERSION 3.10)
project(EdgeCSIRouter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(edge_csi_daemon
  src/main.cpp
  src/csi_parser.cpp
  src/motion.cpp
  src/breath.cpp
  src/prober.cpp
  src/web_server.cpp
)

target_include_directories(edge_csi_daemon PRIVATE src)
target_compile_options(edge_csi_daemon PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(edge_csi_daemon PRIVATE pthread)
target_link_options(edge_csi_daemon PRIVATE -static)

file(COPY web DESTINATION "${CMAKE_BINARY_DIR}")

include(GNUInstallDirs)
install(TARGETS edge_csi_daemon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY web/ DESTINATION "${CMAKE_INSTALL_DATADIR}/edge-csi-router/web")
