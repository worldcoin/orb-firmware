# workspace-wide header files
zephyr_include_directories(include)

# add specific libraries depending on configuration
add_subdirectory_ifdef(CONFIG_ORB_LIB_ERRORS errors)
