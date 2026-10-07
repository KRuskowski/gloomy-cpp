add_library(gloomy_app_shell STATIC
  app_shell.cc
  sample_data.cc
)
target_include_directories(gloomy_app_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gloomy_app_shell PUBLIC gloomy::gloomy)
if(BUILD_PLATFORM_SDL2)
  target_link_libraries(gloomy_app_shell PRIVATE gloomy::sdl2)
  target_compile_definitions(gloomy_app_shell PRIVATE GLOOMY_HAVE_SDL2=1)
endif()
gl_configure_target(gloomy_app_shell)
