#================================
# gloomy library targets
#================================

# Generated sources: SPIR-V shaders and embedded fonts.
set(gl_generated_headers)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/ui.vert kUiVertSpirv)
gl_compile_shader(gl_generated_headers
  ${GL_PROJECT_DIR}/src/gloomy/gfx/shaders/ui.frag kUiFragSpirv)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationSans-Regular.ttf
  kLiberationSansRegular)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationSans-Bold.ttf
  kLiberationSansBold)
gl_embed_file(gl_generated_headers
  ${GL_PROJECT_DIR}/assets/fonts/LiberationMono-Regular.ttf
  kLiberationMonoRegular)
add_custom_target(GlGenerated DEPENDS ${gl_generated_headers})

# Modules
add_library(GlCore OBJECT
  src/gloomy/core/atlas.cc
  src/gloomy/core/image.cc
  src/gloomy/core/utf8.cc
)

add_library(GlGfx OBJECT
  src/gloomy/gfx/device_context.cc
  src/gloomy/gfx/draw_list.cc
  src/gloomy/gfx/offscreen_target.cc
  src/gloomy/gfx/swapchain.cc
  src/gloomy/gfx/ui_renderer.cc
)

add_library(GlText OBJECT
  src/gloomy/text/font_set.cc
  src/gloomy/text/glyph_cache.cc
  src/gloomy/text/shaper.cc
  src/gloomy/text/text_renderer.cc
)

add_library(GlUi OBJECT
  src/gloomy/ui/context.cc
  src/gloomy/ui/layout.cc
  src/gloomy/ui/theme.cc
  src/gloomy/ui/widgets.cc
)

set(gl_module_names
  GlCore
  GlGfx
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
