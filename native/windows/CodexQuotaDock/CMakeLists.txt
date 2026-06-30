cmake_minimum_required(VERSION 3.24)

project(CodexQuotaDockNativeWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_library(cqd_native_core
    src/core.cpp
    src/json.cpp
)

target_include_directories(cqd_native_core PUBLIC src)
target_compile_definitions(cqd_native_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(cqd_native_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
)
target_link_libraries(cqd_native_core PUBLIC
    advapi32
    shell32
    shlwapi
    winhttp
)

add_executable(codex-quota-dock-native WIN32
    src/app.rc
    src/main.cpp
    src/win_app.cpp
)

target_include_directories(codex-quota-dock-native PRIVATE "${REPO_ROOT}")
target_compile_definitions(codex-quota-dock-native PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(codex-quota-dock-native PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
)
target_link_libraries(codex-quota-dock-native PRIVATE
    cqd_native_core
    comctl32
    comdlg32
    dwmapi
    gdi32
    user32
    uxtheme
)

add_executable(cqd_native_tests
    tests/core_tests.cpp
)

target_compile_definitions(cqd_native_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(cqd_native_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
)
target_link_libraries(cqd_native_tests PRIVATE cqd_native_core)

enable_testing()
add_test(NAME cqd_native_tests COMMAND cqd_native_tests)
