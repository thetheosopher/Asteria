add_library(asteria_domain
  person.cpp
  birth_event.cpp
  chart_request.cpp
  computed_chart.cpp
  chart_theme.cpp
  interpretation_note.cpp
  export_artifact.cpp
  location_resolution.cpp
)

target_include_directories(asteria_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
