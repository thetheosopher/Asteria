cmake_minimum_required(VERSION 3.28)

project(Asteria VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTERIA_BUILD_TESTS "Build tests" ON)

if (MSVC)
  add_compile_options(/W4 /permissive-)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# Dear ImGui (docking branch) for the desktop UI
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
  GIT_SHALLOW TRUE
  TLS_VERIFY OFF
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library with Win32+DX11 backend
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC d3d11 dxgi d3dcompiler)
if (MSVC)
  target_compile_options(imgui_lib PRIVATE /W0)
endif()

# SQLite3 via amalgamation
FetchContent_Declare(
  sqlite3
  URL "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  TLS_VERIFY OFF
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})

# Astrolog computation library (vendored third-party)
add_subdirectory(third_party/astrolog)

add_subdirectory(src)

if (ASTERIA_BUILD_TESTS)
  include(CTest)
  enable_testing()
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    TLS_VERIFY OFF
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()
