if(BUILD_PLATFORM_SDL2)
  gl_add_test(test_sdl_input
    platform/test_sdl_input.cc
  )
  target_link_libraries(test_sdl_input PRIVATE gloomy::sdl2)
endif()
