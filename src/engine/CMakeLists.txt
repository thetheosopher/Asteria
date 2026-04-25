add_library(asteria_engine
  fake_chart_engine.cpp
  astrolog_adapter.cpp
  astrolog_embedded_engine.cpp
  astrolog_cli_bridge.cpp
)

target_include_directories(asteria_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_engine PUBLIC asteria_domain astrolog_lib)
