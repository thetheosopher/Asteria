add_library(asteria_render
  chart_scene.cpp
  natal_chart_layout.cpp
  comparison_chart_layout.cpp
  transit_chart_layout.cpp
  svg_serializer.cpp
  theme_presets.cpp
)

target_include_directories(asteria_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_render PUBLIC asteria_domain)
