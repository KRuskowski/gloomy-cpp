cmake_minimum_required(VERSION 3.25)

# Project
project(gloomy
  VERSION   0.1.0
  LANGUAGES C CXX
)

# For include() and find_package() commands. Prepended, so that a parent
# project's modules of the same name (ProjectOptions, ...) do not shadow ours
# when gloomy is built as a subproject.
list(PREPEND CMAKE_MODULE_PATH
  ${PROJECT_SOURCE_DIR}/cmake
)

# As a subproject (add_subdirectory, FetchContent) build only the libraries;
# normal variables take precedence over option() (CMP0077).
if(NOT PROJECT_IS_TOP_LEVEL)
  set(BUILD_APPS OFF)
  set(BUILD_TESTS OFF)
endif()

# Project options
include(ProjectOptions)
# Set project paths
include(ProjectPaths)

# Build Type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release
    CACHE STRING "Build type (Release, Debug, etc.)" FORCE)
endif()

# Needed by clang-tidy and clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
include(SetCppStandard)

# Configure the toolchain
include(ConfigureToolChain)

# Find and collect dependencies
include(ProjectDependencies)

# Build targets
include(CompileShaders)
include(EmbedFiles)
include(BuildGloomyLib)

# Apps
if(BUILD_APPS)
  add_subdirectory(src/apps)
endif()

# Unit tests
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
