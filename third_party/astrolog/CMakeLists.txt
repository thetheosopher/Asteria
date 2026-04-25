# Astrolog 7.80 - Built as a static library for Asteria embedding.
#
# Vendor modifications (documented in docs/02-adr-001):
# 1. astrolog.h: Enabled PC, disabled X11, disabled GRAPH for library build
# 2. astrolog.cpp: Added ASTROLOG_NO_MAIN guard around main()
#
# Excluded files (not needed for computation-only library):
# - wdialog.cpp, wdriver.cpp (Windows GUI)
# - xcharts0.cpp, xcharts1.cpp, xcharts2.cpp, xdata.cpp,
#   xdevice.cpp, xgeneral.cpp, xscreen.cpp (X11 graphics)

set(ASTROLOG_SOURCES
  astrolog.cpp
  atlas.cpp
  calc.cpp
  charts0.cpp
  charts1.cpp
  charts2.cpp
  charts3.cpp
  data.cpp
  express.cpp
  general.cpp
  intrpret.cpp
  io.cpp
  matrix.cpp
  placalc.cpp
  placalc2.cpp
  swecl.cpp
  swedate.cpp
  swehouse.cpp
  swejpl.cpp
  swemmoon.cpp
  swemplan.cpp
  sweph.cpp
  swephlib.cpp
)

add_library(astrolog_lib STATIC ${ASTROLOG_SOURCES})

target_include_directories(astrolog_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Suppress main() so we can use Astrolog as a library
target_compile_definitions(astrolog_lib PRIVATE
  ASTROLOG_NO_MAIN
  _CRT_SECURE_NO_DEPRECATE
  _CRT_NONSTDC_NO_DEPRECATE
)

# Suppress vendor code warnings (Astrolog is C-style C++ with many casts)
if(MSVC)
  target_compile_options(astrolog_lib PRIVATE
    /W0           # Suppress all warnings for vendor code
    /wd4996       # Deprecated POSIX functions
    /Zc:strictStrings-  # Allow string literal to char* (vendor C code)
    /permissive   # Allow non-conforming code (vendor C compatibility)
  )
else()
  target_compile_options(astrolog_lib PRIVATE
    -w            # Suppress all warnings for vendor code
  )
endif()

# Copy data files to build output so engine can find them at runtime
set(ASTROLOG_DATA_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/astrolog.as
  ${CMAKE_CURRENT_SOURCE_DIR}/atlas.as
  ${CMAKE_CURRENT_SOURCE_DIR}/timezone.as
  ${CMAKE_CURRENT_SOURCE_DIR}/sefstars.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/seorbel.txt
)

# Create astrolog data directory in build output
add_custom_command(TARGET astrolog_lib POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:astrolog_lib>/astrolog"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASTROLOG_DATA_FILES}
    "$<TARGET_FILE_DIR:astrolog_lib>/astrolog/"
  COMMENT "Copying Astrolog data files to build output"
)

# Copy ephemeris directory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ephem")
  add_custom_command(TARGET astrolog_lib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/ephem"
      "$<TARGET_FILE_DIR:astrolog_lib>/astrolog/ephem"
    COMMENT "Copying Astrolog ephemeris files to build output"
  )
endif()
