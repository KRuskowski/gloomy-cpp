gl_add_test(test_plot
  plot/test_plot.cc
)
target_link_libraries(test_plot PRIVATE gloomy::plot)
