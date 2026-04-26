add_library(asteria_data
  sqlite_database.cpp
  migration_runner.cpp
  library_database_exchange.cpp
  person_repository.cpp
  birth_event_repository.cpp
  location_repository.cpp
  chart_repository.cpp
  theme_repository.cpp
)

target_include_directories(asteria_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(asteria_data PUBLIC asteria_domain sqlite3_lib)
