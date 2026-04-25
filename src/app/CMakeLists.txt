add_executable(asteria_app
  main.cpp
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

# Copy atlas data files next to the executable
add_custom_command(TARGET asteria_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/atlasbig.as" "$<TARGET_FILE_DIR:asteria_app>/atlasbig.as"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/third_party/astrolog/timezone.as" "$<TARGET_FILE_DIR:asteria_app>/timezone.as"
)
