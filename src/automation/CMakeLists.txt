add_library(asteria_automation
  automation_stub.cpp
  cli_dispatcher.cpp
)

target_include_directories(asteria_automation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_automation PUBLIC asteria_core asteria_engine asteria_render)

add_executable(asteria_cli cli_main.cpp)
target_include_directories(asteria_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_cli PRIVATE asteria_automation asteria_engine)
