add_library(asteria_core
  result.cpp
  natal_chart_service.cpp
  comparison_chart_service.cpp
  interpretation_service.cpp
  export_service.cpp
)

target_include_directories(asteria_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_core PUBLIC asteria_domain asteria_engine asteria_render)
