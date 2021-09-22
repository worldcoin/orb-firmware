# Get all subdirectories under ${current_dir} and store them
# in ${result} variable
macro(subdirlist result current_dir)
    file(GLOB children ${current_dir}/*)
    set(dirlist "")

    foreach (child ${children})
        if (IS_DIRECTORY ${child})
            list(APPEND dirlist ${child})
        endif ()
    endforeach ()

    set(${result} ${dirlist})
endmacro()

# Prepend ${CMAKE_CURRENT_SOURCE_DIR} to a ${directory} name
# and save it in PARENT_SCOPE ${variable}
macro(prepend_cur_dir variable directory)
    set(${variable} ${CMAKE_CURRENT_SOURCE_DIR}/${directory})
endmacro()

# Add custom command to print firmware size in Berkley format
function(firmware_size target)
    get_target_property(EXECUTABLE_SUFFIX ${target} SUFFIX)
    if (NOT EXECUTABLE_SUFFIX)
        set(EXECUTABLE_SUFFIX "")
    endif ()
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_SIZE_UTIL} -B
            "${CMAKE_CURRENT_BINARY_DIR}/${target}${EXECUTABLE_SUFFIX}"
            COMMENT "Firmware size details:"
            )
endfunction()

# Add a command to generate firmware in a provided format
function(generate_object target suffix type)
    # get target suffix if any
    get_target_property(EXECUTABLE_SUFFIX ${target} SUFFIX)
    if (NOT EXECUTABLE_SUFFIX)
        set(EXECUTABLE_SUFFIX "")
    endif ()
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O${type}
            "${CMAKE_CURRENT_BINARY_DIR}/${target}${EXECUTABLE_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/${target}${suffix}"
            COMMENT "Building ${target}${suffix}"
            )
endfunction()

#function(generate_object target suffix type)
#    add_custom_command(TARGET ${target} POST_BUILD
#            COMMAND ${CMAKE_OBJCOPY} -O${type}
#            "${CMAKE_CURRENT_BINARY_DIR}/${target}${CMAKE_EXECUTABLE_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/${target}${suffix}"
#            COMMENT "Building ${target}${suffix}"
#            )
#endfunction()