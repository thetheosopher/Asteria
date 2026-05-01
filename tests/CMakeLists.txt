set(TEST_SOURCES
  unit/smoke_test.cpp
  unit/domain_test.cpp
  unit/engine_test.cpp
  unit/render_test.cpp
  unit/comparison_service_test.cpp
  unit/comparison_render_test.cpp
  unit/interpretation_test.cpp
  unit/report_service_test.cpp
  unit/transit_report_service_test.cpp
  unit/export_test.cpp
  unit/cli_dispatcher_test.cpp
  unit/cli_json_input_test.cpp
  unit/uncertain_time_test.cpp
  unit/birth_event_resolver_test.cpp
  unit/svg_determinism_test.cpp
  unit/atlas_service_test.cpp
  integration/database_test.cpp
  integration/library_database_exchange_test.cpp
  integration/astrolog_engine_test.cpp
)

add_executable(asteria_tests ${TEST_SOURCES})

target_include_directories(asteria_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Pass source directory so tests can find Astrolog data files
target_compile_definitions(asteria_tests PRIVATE
  ASTERIA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(asteria_tests PRIVATE
  asteria_domain
  asteria_core
  asteria_engine
  asteria_render
  asteria_data
  asteria_automation
  asteria_util
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(asteria_tests)
