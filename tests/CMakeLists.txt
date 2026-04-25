set(TEST_SOURCES
  unit/smoke_test.cpp
  unit/domain_test.cpp
  unit/engine_test.cpp
  unit/render_test.cpp
  unit/comparison_service_test.cpp
  unit/comparison_render_test.cpp
  unit/interpretation_test.cpp
  unit/export_test.cpp
  unit/cli_dispatcher_test.cpp
  integration/database_test.cpp
)

add_executable(asteria_tests ${TEST_SOURCES})

target_include_directories(asteria_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(asteria_tests PRIVATE
  asteria_domain
  asteria_core
  asteria_engine
  asteria_render
  asteria_data
  asteria_automation
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(asteria_tests)
