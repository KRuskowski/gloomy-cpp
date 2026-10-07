#=====================================
# Defines the project paths
#=====================================

# Project root directory
set(GL_PROJECT_DIR
  ${PROJECT_SOURCE_DIR}
)

# Directory holding the current build space
set(GL_BUILD_DIR
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Generated sources (SPIR-V, embedded assets)
set(GL_GENERATED_DIR
  ${GL_BUILD_DIR}/generated
)

# Public and private include directories while building the library
set(GL_PUBLIC_BUILD_INCLUDE_DIRS
  ${GL_PROJECT_DIR}/include
)

set(GL_PRIVATE_BUILD_INCLUDE_DIRS
  ${GL_PROJECT_DIR}/src/gloomy
  ${GL_GENERATED_DIR}
)
