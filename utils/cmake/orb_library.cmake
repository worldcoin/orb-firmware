# Determines what the lib name would be according to the
# provided base and writes it to the argument "lib_name"
macro(library_get_lib_name lib_name)
    # Remove the prefix (/home/sebo/zephyr/driver/serial/CMakeLists.txt => driver/serial/CMakeLists.txt)
    file(RELATIVE_PATH name ${ZEPHYR_BASE}/.. ${CMAKE_CURRENT_SOURCE_DIR})

    # Replace / with __ (driver/serial => driver__serial)
    string(REGEX REPLACE "/" "__" name ${name})
    # Replace : with __ (C:/zephyrproject => C____zephyrproject)
    string(REGEX REPLACE ":" "__" name ${name})

    set(${lib_name} ${name})
endmacro()

function(orb_library)
    # add include by default
    zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

    library_get_lib_name(lib_name)
    zephyr_library_named(${lib_name})

    if(APP_FILES_COMPILE_OPTIONS)
        zephyr_library_compile_options(${APP_FILES_COMPILE_OPTIONS})
    endif()
    if(EXTRA_COMPILE_FLAGS)
        zephyr_library_compile_options(${EXTRA_COMPILE_FLAGS})
    endif()

    zephyr_library_sources(${src_files} ${ARGN})
endfunction()
