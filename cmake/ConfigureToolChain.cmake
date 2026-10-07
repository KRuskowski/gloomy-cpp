#===================================
# Configure tool chain
#===================================

# Applies the project warning level and sanitizers to a target. Only used on
# our own targets so that third party code keeps its own settings.
function(gl_configure_target target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow
    )
    if(ENABLE_SANITIZERS)
      target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer
      )
      target_link_options(${target} PRIVATE
        -fsanitize=address,undefined
      )
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${target} PRIVATE /W4 /utf-8)
  endif()
endfunction()
