cmake_minimum_required(VERSION 3.28)

project(Asteria VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTERIA_BUILD_TESTS "Build tests" ON)
option(ASTERIA_ENABLE_QT_UI "Enable Qt UI integration (future)" OFF)

if (MSVC)
  add_compile_options(/W4 /permissive-)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

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
