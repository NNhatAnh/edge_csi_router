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
  src/driver_audit_logger.cpp
  src/web_server.cpp
  src/wave_detector.cpp
  src/distance_estimator.cpp
  src/link_stability_estimator.cpp
  src/wakeup_detector.cpp
  src/v6_native_engine.cpp
)

target_include_directories(edge_csi_daemon PRIVATE src)
target_compile_options(edge_csi_daemon PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(edge_csi_daemon PRIVATE pthread m)
target_link_options(edge_csi_daemon PRIVATE -static)

file(COPY web DESTINATION "${CMAKE_BINARY_DIR}")

# ==============================================================================
# TỰ ĐỘNG TỔNG HỢP GÓI TRIỂN KHAI CHO LINUX ROUTER (/root/build_demo)
# ==============================================================================
set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/build_demo")

add_custom_target(dist ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:edge_csi_daemon>" "${PACKAGE_DIR}/edge_csi_daemon"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/web" "${PACKAGE_DIR}/web"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/models" "${PACKAGE_DIR}/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/start_all.sh" "${PACKAGE_DIR}/start_all.sh"
    DEPENDS edge_csi_daemon
    COMMENT ">>> Dang tong hop toan bo file chay vao thu muc: ${PACKAGE_DIR}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/wake_native")
    add_custom_command(TARGET dist POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/wake_native" "${PACKAGE_DIR}/wake_native"
        COMMAND chmod +x "${PACKAGE_DIR}/wake_native"
        COMMENT ">>> Da bo sung binary wake_native vao goi chay"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/capture_wake.sh")
    add_custom_command(TARGET dist POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/capture_wake.sh" "${PACKAGE_DIR}/capture_wake.sh"
        COMMAND chmod +x "${PACKAGE_DIR}/capture_wake.sh"
    )
endif()

if(UNIX)
    add_custom_command(TARGET dist POST_BUILD
        COMMAND chmod +x "${PACKAGE_DIR}/edge_csi_daemon" "${PACKAGE_DIR}/start_all.sh"
        COMMENT ">>> Da phan quyen thuc thi (chmod +x) cho cac file nhi phan va script"
    )
endif()

include(GNUInstallDirs)
install(TARGETS edge_csi_daemon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY web/ DESTINATION "${CMAKE_INSTALL_DATADIR}/edge-csi-router/web")
