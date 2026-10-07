add_library(gloomy_app_shell STATIC
  sample_data.cc
)
target_include_directories(gloomy_app_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gloomy_app_shell PUBLIC gloomy::gloomy gloomy::app)
gl_configure_target(gloomy_app_shell)
