#===================================
# gloomy project options
#===================================

###
# Build options
#
option(BUILD_APPS
  "Build the demo and example applications."
  ON)

option(BUILD_TESTS
  "Build unit and image tests."
  ON)

option(BUILD_PLOT
  "Build gloomy::plot: matplot++ figures drawn by gloomy (fetches matplot++)."
  ON)

option(BUILD_PLATFORM_SDL2
  "Build the SDL2 window and input platform layer."
  ON)

###
# Debugging
#
option(ENABLE_SANITIZERS
  "Build with address and undefined behaviour sanitizers."
  OFF)

###
# Dependencies
#
set(GLOOMY_NANOSVG_DIR ""
  CACHE PATH "Local nanosvg checkout (with src/nanosvg.h); empty fetches it.")
set(GLOOMY_MATPLOT_DIR ""
  CACHE PATH "Local matplot++ checkout for BUILD_PLOT; empty fetches it.")
