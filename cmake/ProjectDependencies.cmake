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

# matplot++ (MIT): the plotting API and figure model that gloomy::plot
# draws (docs/design.md section 11.4). Used unmodified at a pinned commit;
# GLOOMY_MATPLOT_DIR uses a local checkout instead. Only the library is
# built: no examples, tests, installer, high resolution world maps, OpenCV
# or CImg's optional image libraries; gnuplot is never started.
if(BUILD_PLOT)
  set(MATPLOTPP_BUILD_EXAMPLES OFF)
  set(MATPLOTPP_BUILD_TESTS OFF)
  set(MATPLOTPP_BUILD_INSTALLER OFF)
  set(MATPLOTPP_BUILD_PACKAGE OFF)
  set(MATPLOTPP_BUILD_WITH_PEDANTIC_WARNINGS OFF)
  set(MATPLOTPP_BUILD_WITH_SANITIZERS OFF)
  set(MATPLOTPP_BUILD_SHARED_LIBS OFF)
  set(MATPLOTPP_BUILD_HIGH_RESOLUTION_WORLD_MAP OFF)
  set(MATPLOTPP_WITH_OPENCV OFF)
  foreach(pkg JPEG TIFF LAPACK BLAS FFTW OpenCV)
    set(CMAKE_DISABLE_FIND_PACKAGE_${pkg} ON)
  endforeach()
  if(GLOOMY_MATPLOT_DIR)
    add_subdirectory(${GLOOMY_MATPLOT_DIR}
      ${CMAKE_BINARY_DIR}/_deps/matplot-build EXCLUDE_FROM_ALL SYSTEM)
  else()
    include(FetchContent)
    FetchContent_Declare(gl_matplot
      DOWNLOAD_EXTRACT_TIMESTAMP OFF
      # v1.2.2 + 6 (2025-12-01).
      URL https://github.com/alandefreitas/matplotplusplus/archive/b2fed97fca6e7a5380efe14ec76e227c2087b77e.tar.gz
      URL_HASH SHA256=4f79d1f304851f6412be1ceeb99cfcb92174a160adedfe0e1a9a4ddba99315a9
      EXCLUDE_FROM_ALL
      SYSTEM
    )
    FetchContent_MakeAvailable(gl_matplot)
  endif()
endif()
