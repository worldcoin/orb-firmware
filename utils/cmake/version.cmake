function(application_version app_version)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ver)

    string(REGEX MATCH "VERSION_MAJOR = ([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})

    string(REGEX MATCH "VERSION_MINOR = ([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_1})

    string(REGEX MATCH "VERSION_PATCH = ([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_1})

    set(CURRENT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
    set(${app_version} ${CURRENT_VERSION} PARENT_SCOPE)

    # append `--pad` to CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS to mark the image as pending right from the binary
    # if not used, the primary slot app has to mark the secondary slot as pending (ready to be used)
    file(WRITE ${CMAKE_BINARY_DIR}/app_version.conf "# generated from utils/cmake/version.cmake\r\nCONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS=\"--version ${CURRENT_VERSION}\"")
endfunction()
