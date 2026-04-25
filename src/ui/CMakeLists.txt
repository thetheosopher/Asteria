add_library(asteria_ui STATIC
  app_context.cpp
  app_window.cpp
  astrology_font.cpp
  export_options.cpp
  file_dialog.cpp
  library_panel.cpp
  markdown_render.cpp
  chart_workspace_panel.cpp
  transit_timeline_panel.cpp
  compare_workspace_panel.cpp
  settings_panel.cpp
  ai_interpretation_panel.cpp
)

target_include_directories(asteria_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_ui PUBLIC
  imgui_lib
  asteria_domain
  asteria_core
  asteria_render
  asteria_data
  asteria_util
  comdlg32
)
