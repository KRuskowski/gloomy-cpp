gl_add_test(test_app_shell
  app/test_app_shell.cc
)
target_link_libraries(test_app_shell PRIVATE gloomy::app)
