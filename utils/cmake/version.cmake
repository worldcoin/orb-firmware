include(${PROJECT_DIR}/utils/cmake/GetGitRevisionDescription.cmake)

function(application_version app_version)
    if(DEFINED BUILD_FROM_CI)
        get_git_head_revision(branch hash ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
        # cmake does not support {n} syntax to get the 8 digits
        string(REGEX MATCH "^[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]" short_hash ${hash})
        string(LENGTH ${short_hash} short_hash_len)
        if (${short_hash_len} EQUAL 8)
            message(STATUS "Git branch: ${branch}, commit: ${short_hash}")
        else()
            message(FATAL_ERROR "Expected short hash length is 8, actually ${short_hash_len} (hash: ${short_hash})")
        endif()
        math(EXPR SHORT_HASH_DEC "0x${short_hash}" OUTPUT_FORMAT DECIMAL)
        set(SHORT_HASH_HEX ${short_hash})
    else()
        set(SHORT_HASH_HEX 0)
        set(SHORT_HASH_DEC 0)
        message(STATUS "Not embedding commit hash in image version")
    endif()

    file(READ ${PROJECT_DIR}/VERSION ver)

    string(REGEX MATCH "VERSION_MAJOR=([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})

    string(REGEX MATCH "VERSION_MINOR=([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_1})

    string(REGEX MATCH "VERSION_PATCH=([0-9]*)" _ ${ver})
    set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_1})

    # use short hash as build number
    set(CURRENT_VERSION_HEX_BUILD ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}+${SHORT_HASH_HEX})
    set(CURRENT_VERSION_DEC_BUILD ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}+${SHORT_HASH_DEC})
    set(${app_version} ${CURRENT_VERSION} PARENT_SCOPE)

    add_compile_definitions(FW_VERSION_FULL=${CURRENT_VERSION_HEX_BUILD})

    # append `--pad` to CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS to mark the image as pending right from the binary
    # if not used, the primary slot app has to mark the secondary slot as pending (ready to be used)
    file(WRITE ${CMAKE_BINARY_DIR}/app_version.conf
            "# generated from utils/cmake/version.cmake\r\nCONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS=\"--version ${CURRENT_VERSION_DEC_BUILD}\"")
endfunction()
