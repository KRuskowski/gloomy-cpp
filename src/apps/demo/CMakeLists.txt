add_executable(gloomy_demo
  main.cc
)
target_link_libraries(gloomy_demo
  PRIVATE
    gloomy::gloomy
)
if(BUILD_PLATFORM_SDL2)
  target_link_libraries(gloomy_demo PRIVATE gloomy::sdl2)
  target_compile_definitions(gloomy_demo PRIVATE GLOOMY_HAVE_SDL2=1)
endif()
gl_configure_target(gloomy_demo)
