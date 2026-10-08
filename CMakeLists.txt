cmake_minimum_required(VERSION 3.10)
project(EdgeCSIRouter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(edge_csi_daemon
    src/main.cpp
    src/csi_parser.cpp
    src/web_server.cpp
)
target_include_directories(edge_csi_daemon PRIVATE include)
target_compile_options(edge_csi_daemon PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(edge_csi_daemon PRIVATE pthread)
target_link_options(edge_csi_daemon PRIVATE -static)

file(COPY web DESTINATION "${CMAKE_BINARY_DIR}")

include(CTest)
if(BUILD_TESTING)
    add_executable(csi_parser_test
        tests/csi_parser_test.cpp
        src/csi_parser.cpp
    )
    target_include_directories(csi_parser_test PRIVATE include)
    target_compile_options(csi_parser_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME csi_parser_test COMMAND csi_parser_test)
endif()

include(GNUInstallDirs)
install(TARGETS edge_csi_daemon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY web/ DESTINATION "${CMAKE_INSTALL_DATADIR}/edge-csi-router/web")
