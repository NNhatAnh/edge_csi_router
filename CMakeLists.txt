cmake_minimum_required(VERSION 3.10)

# Tự động nạp Toolchain ARM64 cho Router OpenWrt nếu có trình biên dịch chéo
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/arm64_toolchain.cmake")
    find_program(_AARCH64_GCC aarch64-linux-gnu-gcc)
    if(_AARCH64_GCC)
        include("${CMAKE_CURRENT_SOURCE_DIR}/arm64_toolchain.cmake")
    endif()
endif()

project(EdgeCSIRouter LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# TARGET 1: EDGE_CSI_DAEMON (CỔNG 5243 - MOTION SCORE & DASHBOARD CHÍNH)
# ==============================================================================
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
)

target_include_directories(edge_csi_daemon PRIVATE src)
target_compile_options(edge_csi_daemon PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(edge_csi_daemon PRIVATE pthread m)
target_link_options(edge_csi_daemon PRIVATE -static)

# ==============================================================================
# TARGET 2: WAKE-SCORE-WEB (CỔNG 8082 - Demo công nghệ Wi-Fi Sensing MODEL M5R)
# ==============================================================================
add_executable(wake-score-web
  src/wake_score_web/server.cpp
)

target_include_directories(wake-score-web PRIVATE src/wake_score_web)
target_compile_options(wake-score-web PRIVATE -Wall -O2)
target_link_libraries(wake-score-web PRIVATE pthread m)
target_link_options(wake-score-web PRIVATE -static)

# Đồng bộ binary wake-score-web vào thư mục gốc để lưu trữ dự phòng
add_custom_command(TARGET wake-score-web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:wake-score-web>" "${CMAKE_SOURCE_DIR}/wake-score-web"
    COMMAND chmod +x "${CMAKE_SOURCE_DIR}/wake-score-web" 2>/dev/null || true
)

# ==============================================================================
# 1. TỰ ĐỘNG ĐỒNG BỘ ĐẦY ĐỦ TỆP RUNTIME VÀO THƯ MỤC BUILD (${CMAKE_BINARY_DIR})
# ==============================================================================
add_custom_command(TARGET edge_csi_daemon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/web" "${CMAKE_BINARY_DIR}/web"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/models" "${CMAKE_BINARY_DIR}/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/start_all.sh" "${CMAKE_BINARY_DIR}/start_all.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/stop_all.sh" "${CMAKE_BINARY_DIR}/stop_all.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/capture_wake.sh" "${CMAKE_BINARY_DIR}/capture_wake.sh"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/edge_csi.init" "${CMAKE_BINARY_DIR}/edge_csi"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/score.html" "${CMAKE_BINARY_DIR}/score.html"
    COMMENT ">>> Da dong bo cac script .sh, edge_csi, models va web vao: ${CMAKE_BINARY_DIR}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/wake_native")
    add_custom_command(TARGET edge_csi_daemon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/wake_native" "${CMAKE_BINARY_DIR}/wake_native"
    )
endif()

if(UNIX)
    add_custom_command(TARGET edge_csi_daemon POST_BUILD
        COMMAND chmod +x "${CMAKE_BINARY_DIR}/edge_csi_daemon"
                         "${CMAKE_BINARY_DIR}/start_all.sh"
                         "${CMAKE_BINARY_DIR}/stop_all.sh"
                         "${CMAKE_BINARY_DIR}/capture_wake.sh"
                         "${CMAKE_BINARY_DIR}/edge_csi" 2>/dev/null || true
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/wake_native")
        add_custom_command(TARGET edge_csi_daemon POST_BUILD
            COMMAND chmod +x "${CMAKE_BINARY_DIR}/wake_native" 2>/dev/null || true
        )
    endif()
endif()

# ==============================================================================
# 2. TỔNG HỢP GÓI TRIỂN KHAI CHO LINUX ROUTER (/root/build_demo)
# ==============================================================================
get_filename_component(_BUILD_DIR_NAME "${CMAKE_BINARY_DIR}" NAME)

if(NOT "${_BUILD_DIR_NAME}" STREQUAL "build_demo")
    set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/build_demo")

    add_custom_target(dist ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:edge_csi_daemon>" "${PACKAGE_DIR}/edge_csi_daemon"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:wake-score-web>" "${PACKAGE_DIR}/wake-score-web"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/web" "${PACKAGE_DIR}/web"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/models" "${PACKAGE_DIR}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/start_all.sh" "${PACKAGE_DIR}/start_all.sh"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/stop_all.sh" "${PACKAGE_DIR}/stop_all.sh"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/capture_wake.sh" "${PACKAGE_DIR}/capture_wake.sh"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/edge_csi.init" "${PACKAGE_DIR}/edge_csi"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/score.html" "${PACKAGE_DIR}/score.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/web/gioithieu.html" "${PACKAGE_DIR}/gioithieu.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/web/qrcode.min.js" "${PACKAGE_DIR}/qrcode.min.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/web/wifi.jpg" "${PACKAGE_DIR}/wifi.jpg"
        DEPENDS edge_csi_daemon wake-score-web
        COMMENT ">>> Dang tong hop goi trien khai sach vao: ${PACKAGE_DIR}"
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/wake_native")
        add_custom_command(TARGET dist POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/wake_native" "${PACKAGE_DIR}/wake_native"
            COMMAND chmod +x "${PACKAGE_DIR}/wake_native" 2>/dev/null || true
        )
    endif()

    if(UNIX)
        add_custom_command(TARGET dist POST_BUILD
            COMMAND chmod +x "${PACKAGE_DIR}/edge_csi_daemon"
                             "${PACKAGE_DIR}/wake-score-web"
                             "${PACKAGE_DIR}/start_all.sh"
                             "${PACKAGE_DIR}/stop_all.sh"
                             "${PACKAGE_DIR}/capture_wake.sh"
                             "${PACKAGE_DIR}/edge_csi" 2>/dev/null || true
            COMMENT ">>> Da phan quyen thuc thi (chmod +x) cho cac file trong ${PACKAGE_DIR}"
        )
    endif()
else()
    add_custom_target(dist ALL
        DEPENDS edge_csi_daemon wake-score-web
        COMMENT ">>> Thu muc build da la build_demo, toan bo file san sang o goc thu muc."
    )
endif()

include(GNUInstallDirs)
install(TARGETS edge_csi_daemon wake-score-web RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY web/ DESTINATION "${CMAKE_INSTALL_DATADIR}/edge-csi-router/web")
