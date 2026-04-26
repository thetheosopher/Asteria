add_executable(asteria_app WIN32
  main.cpp
  asteria.rc
)

target_include_directories(asteria_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_app PRIVATE
  asteria_domain
  asteria_core
  asteria_engine
  asteria_render
  asteria_data
  asteria_ui
)

# Use standard main() entry point even with WIN32 subsystem
if(MSVC)
  target_link_options(asteria_app PRIVATE /ENTRY:mainCRTStartup)
endif()

# Copy atlas data files and icon next to the executable
add_custom_command(TARGET asteria_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/atlasbig.as" "$<TARGET_FILE_DIR:asteria_app>/atlasbig.as"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/third_party/astrolog/timezone.as" "$<TARGET_FILE_DIR:asteria_app>/timezone.as"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/fonts/Astro.ttf" "$<TARGET_FILE_DIR:asteria_app>/Astro.ttf"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/asteria_icon.png" "$<TARGET_FILE_DIR:asteria_app>/asteria_icon.png"
)
