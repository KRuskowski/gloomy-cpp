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
