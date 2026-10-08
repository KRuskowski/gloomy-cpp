#=====================================
# Collects the dependencies
#=====================================

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)

find_program(GL_GLSLC glslc REQUIRED)

if(BUILD_PLATFORM_SDL2)
  find_package(SDL2 REQUIRED)
endif()

# nanosvg (zlib licence, header only): parses and rasterises SVG icons
# (src/gloomy/icon/icon_set.cc). Pinned; GLOOMY_NANOSVG_DIR uses a local
# checkout instead. Only populated, never added as a subproject.
if(GLOOMY_NANOSVG_DIR)
  set(GL_NANOSVG_INCLUDE_DIR ${GLOOMY_NANOSVG_DIR}/src)
else()
  include(FetchContent)
  FetchContent_Declare(gl_nanosvg
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz
    SOURCE_SUBDIR no-cmake
  )
  FetchContent_MakeAvailable(gl_nanosvg)
  set(GL_NANOSVG_INCLUDE_DIR ${gl_nanosvg_SOURCE_DIR}/src)
endif()
