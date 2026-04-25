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
)
