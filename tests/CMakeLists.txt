#===================================
# Unit and image tests
#===================================

# Prefer the system GoogleTest, fall back to fetching it
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  )
  set(BUILD_GMOCK OFF)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

# Vulkan driver for image tests: Mesa's software rasteriser, so results do
# not depend on the GPU.
set(GL_TEST_ICD "/usr/share/vulkan/icd.d/lvp_icd.json"
  CACHE STRING "VK_ICD_FILENAMES for GPU tests (empty: system default)")

# Helpers shared by all tests: GPU fixture, reference image comparison.
add_library(gl_test_support STATIC
  test_support/gpu_test.cc
)
target_link_libraries(gl_test_support
  PUBLIC
    gloomy::gloomy
    GTest::gtest
)
target_include_directories(gl_test_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(gl_test_support
  PRIVATE
    GL_TEST_REFERENCE_DIR="${GL_PROJECT_DIR}/tests/reference"
    GL_TEST_OUTPUT_DIR="${GL_BUILD_DIR}/test_output"
)
gl_configure_target(gl_test_support)

# Adds a test executable linked against the library. Tests may include
# private library headers.
function(gl_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name}
    PRIVATE
      gloomy::gloomy
      gl_test_support
      GTest::gtest_main
  )
  target_include_directories(${name}
    PRIVATE
      ${GL_PRIVATE_BUILD_INCLUDE_DIRS}
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  gl_configure_target(${name})
  # Designated initializers that leave defaulted fields out are idiomatic.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${name} PRIVATE
      -Wno-missing-designated-field-initializers)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${name} PRIVATE -Wno-missing-field-initializers)
  endif()
  set(env "")
  if(GL_TEST_ICD)
    set(env "VK_ICD_FILENAMES=${GL_TEST_ICD}")
  endif()
  gtest_discover_tests(${name}
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "${env}"
  )
endfunction()

# Modules
include(cmake/core/test_atlas.cmake)
include(cmake/core/test_image.cmake)
include(cmake/core/test_utf8.cmake)
include(cmake/data/test_document.cmake)
include(cmake/data/test_query.cmake)
include(cmake/data/test_search.cmake)
include(cmake/data/test_table_source.cmake)
include(cmake/data/test_table_view.cmake)
include(cmake/gfx/test_draw_list.cmake)
include(cmake/gfx/test_primitives_image.cmake)
include(cmake/text/test_shaper.cmake)
include(cmake/text/test_text_image.cmake)
include(cmake/ui/test_data_grid.cmake)
include(cmake/ui/test_data_grid_image.cmake)
include(cmake/ui/test_document_view.cmake)
include(cmake/ui/test_layout.cmake)
include(cmake/ui/test_summary.cmake)
include(cmake/ui/test_widgets.cmake)
include(cmake/ui/test_widgets_image.cmake)
include(cmake/platform/test_sdl_input.cmake)
