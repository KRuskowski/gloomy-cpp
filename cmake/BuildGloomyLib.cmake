#================================
# gloomy library targets
#================================

# Generated sources: SPIR-V shaders and embedded fonts.
set(gl_generated_headers)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/ui.vert kUiVertSpirv)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/ui.frag kUiFragSpirv)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/fullscreen.vert kFullscreenVertSpirv)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/blur_down.frag kBlurDownFragSpirv)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/blur_up.frag kBlurUpFragSpirv)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationSans-Regular.ttf
  kLiberationSansRegular)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationSans-Bold.ttf
  kLiberationSansBold)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationSans-Italic.ttf
  kLiberationSansItalic)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationMono-Regular.ttf
  kLiberationMonoRegular)
add_custom_target(GlGenerated DEPENDS ${gl_generated_headers})

# Modules
add_library(GlCore OBJECT
  src/gloomy/core/atlas.cc
  src/gloomy/core/image.cc
  src/gloomy/core/parallel.cc
  src/gloomy/core/utf8.cc
)

add_library(GlData OBJECT
  src/gloomy/data/columnar_table.cc
  src/gloomy/data/display_rows.cc
  src/gloomy/data/document.cc
  src/gloomy/data/fuzzy.cc
  src/gloomy/data/query.cc
  src/gloomy/data/search.cc
  src/gloomy/data/table_source.cc
  src/gloomy/data/table_view.cc
)

add_library(GlGfx OBJECT
  src/gloomy/gfx/backdrop_blur.cc
  src/gloomy/gfx/device_context.cc
  src/gloomy/gfx/draw_list.cc
  src/gloomy/gfx/offscreen_target.cc
  src/gloomy/gfx/swapchain.cc
  src/gloomy/gfx/ui_renderer.cc
)

add_library(GlIcon OBJECT
  src/gloomy/icon/icon_cache.cc
  src/gloomy/icon/icon_set.cc
)
target_include_directories(GlIcon SYSTEM PRIVATE ${GL_NANOSVG_INCLUDE_DIR})

add_library(GlText OBJECT
  src/gloomy/text/font_set.cc
  src/gloomy/text/glyph_cache.cc
  src/gloomy/text/rich_layout.cc
  src/gloomy/text/shaper.cc
  src/gloomy/text/text_renderer.cc
)

add_library(GlUi OBJECT
  src/gloomy/ui/canvas.cc
  src/gloomy/ui/chart.cc
  src/gloomy/ui/context.cc
  src/gloomy/ui/dashboard.cc
  src/gloomy/ui/data_grid.cc
  src/gloomy/ui/disclosure.cc
  src/gloomy/ui/dock.cc
  src/gloomy/ui/document_view.cc
  src/gloomy/ui/finder.cc
  src/gloomy/ui/hud.cc
  src/gloomy/ui/label_placer.cc
  src/gloomy/ui/key_chord.cc
  src/gloomy/ui/layout.cc
  src/gloomy/ui/menu_list.cc
  src/gloomy/ui/navigation.cc
  src/gloomy/ui/overlays.cc
  src/gloomy/ui/radial_menu.cc
  src/gloomy/ui/summary.cc
  src/gloomy/ui/theme.cc
  src/gloomy/ui/timeline.cc
  src/gloomy/ui/view_preset.cc
  src/gloomy/ui/widget_util.cc
  src/gloomy/ui/widgets.cc
)

set(gl_module_names
  GlCore
  GlData
  GlGfx
  GlIcon
  GlText
  GlUi
)

set(gl_module_object_files)

foreach(M IN LISTS gl_module_names)
  list(APPEND gl_module_object_files $<TARGET_OBJECTS:${M}>)

  target_include_directories(${M}
    PUBLIC
      ${GL_PUBLIC_BUILD_INCLUDE_DIRS}
    PRIVATE
      ${GL_PRIVATE_BUILD_INCLUDE_DIRS}
  )
  target_link_libraries(${M}
    PUBLIC
      Vulkan::Vulkan
    PRIVATE
      Freetype::Freetype
      PkgConfig::HARFBUZZ
      PNG::PNG
  )
  add_dependencies(${M} GlGenerated)
  gl_configure_target(${M})
endforeach()

# Static library
add_library(gloomy STATIC
  ${gl_module_object_files}
)

add_library(gloomy::gloomy ALIAS gloomy)

target_include_directories(gloomy
  PUBLIC
    ${GL_PUBLIC_BUILD_INCLUDE_DIRS}
  PRIVATE
    ${GL_PRIVATE_BUILD_INCLUDE_DIRS}
)

target_link_libraries(gloomy
  PUBLIC
    Vulkan::Vulkan
    Threads::Threads
  PRIVATE
    Freetype::Freetype
    PkgConfig::HARFBUZZ
    PNG::PNG
)

gl_configure_target(gloomy)

# SDL2 platform layer: windows, Vulkan surfaces and input events.
if(BUILD_PLATFORM_SDL2)
  add_library(gloomy_sdl2 STATIC
    src/gloomy/platform/sdl_window.cc
  )
  add_library(gloomy::sdl2 ALIAS gloomy_sdl2)
  target_include_directories(gloomy_sdl2
    PUBLIC
      ${GL_PUBLIC_BUILD_INCLUDE_DIRS}
    PRIVATE
      ${GL_PRIVATE_BUILD_INCLUDE_DIRS}
  )
  target_link_libraries(gloomy_sdl2
    PUBLIC
      gloomy::gloomy
      SDL2::SDL2
  )
  gl_configure_target(gloomy_sdl2)
endif()

# App shell: runs a UI function in an SDL2 window or headless into a PNG.
add_library(gloomy_app STATIC
  src/gloomy/app/app_shell.cc
)
add_library(gloomy::app ALIAS gloomy_app)
target_include_directories(gloomy_app
  PUBLIC
    ${GL_PUBLIC_BUILD_INCLUDE_DIRS}
  PRIVATE
    ${GL_PRIVATE_BUILD_INCLUDE_DIRS}
)
target_link_libraries(gloomy_app
  PUBLIC
    gloomy::gloomy
)
if(BUILD_PLATFORM_SDL2)
  target_link_libraries(gloomy_app PRIVATE gloomy::sdl2)
  target_compile_definitions(gloomy_app PRIVATE GLOOMY_HAVE_SDL2=1)
endif()
gl_configure_target(gloomy_app)
