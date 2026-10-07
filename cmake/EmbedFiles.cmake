#=====================================
# Embeds binary files (fonts) as C++ arrays
#=====================================

# gl_embed_file(<out_var> <file> <symbol>)
#
# Generates a header in GL_GENERATED_DIR that defines
# `inline constexpr unsigned char <symbol>[]` with the bytes of <file>.
# Appends the generated header to <out_var>.
function(gl_embed_file out_var file symbol)
  get_filename_component(name ${file} NAME)
  set(header ${GL_GENERATED_DIR}/assets/${name}.h)
  add_custom_command(
    OUTPUT ${header}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GL_GENERATED_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -DINPUT=${file} -DOUTPUT=${header}
      -DSYMBOL=${symbol} -DWORDS=OFF
      -P ${GL_PROJECT_DIR}/cmake/EmbedFileScript.cmake
    DEPENDS ${file} ${GL_PROJECT_DIR}/cmake/EmbedFileScript.cmake
    COMMENT "Embedding ${name}"
    VERBATIM
  )
  set(${out_var} ${${out_var}} ${header} PARENT_SCOPE)
endfunction()
