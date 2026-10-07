#=====================================
# Compiles GLSL shaders to SPIR-V and embeds them as C++ arrays
#=====================================

# gl_compile_shader(<out_var> <shader> <symbol>)
#
# Compiles <shader> with glslc and generates a header in GL_GENERATED_DIR that
# defines `inline constexpr std::uint32_t <symbol>[]`. Appends the generated
# header to <out_var>.
function(gl_compile_shader out_var shader symbol)
  get_filename_component(name ${shader} NAME)
  set(spv ${GL_GENERATED_DIR}/shaders/${name}.spv)
  set(header ${GL_GENERATED_DIR}/shaders/${name}.h)
  add_custom_command(
    OUTPUT ${header}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GL_GENERATED_DIR}/shaders
    COMMAND ${GL_GLSLC} --target-env=vulkan1.3 -O -o ${spv} ${shader}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${spv} -DOUTPUT=${header}
      -DSYMBOL=${symbol} -DWORDS=ON
      -P ${GL_PROJECT_DIR}/cmake/EmbedFileScript.cmake
    DEPENDS ${shader} ${GL_PROJECT_DIR}/cmake/EmbedFileScript.cmake
    COMMENT "Compiling shader ${name}"
    VERBATIM
  )
  set(${out_var} ${${out_var}} ${header} PARENT_SCOPE)
endfunction()
