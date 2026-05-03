cmake_minimum_required(VERSION 3.23)

project(Asteria VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use static CRT in Release to minimize deployed DLLs; dynamic in Debug for GTest compat
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")

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
set(sqlite3_fetch_args
  URL "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"
  TLS_VERIFY OFF
)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  list(APPEND sqlite3_fetch_args DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_Declare(sqlite3 ${sqlite3_fetch_args})
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})

# cpp-httplib — header-only HTTP client for Ollama API (no SSL needed; Ollama is local)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.3
  GIT_SHALLOW TRUE
  TLS_VERIFY OFF
)
FetchContent_MakeAvailable(httplib)

# libharu — lightweight PDF generation for report exports.
set(LIBHPDF_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  libharu
  GIT_REPOSITORY https://github.com/libharu/libharu.git
  GIT_TAG v2.4.5
  GIT_SHALLOW TRUE
  TLS_VERIFY OFF
)
FetchContent_MakeAvailable(libharu)

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
