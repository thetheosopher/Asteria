add_library(asteria_util
  ollama_client.cpp
  atlas_service.cpp
)

target_include_directories(asteria_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_util PUBLIC httplib::httplib asteria_domain)
