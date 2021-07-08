enable_testing()

add_executable(
        template_test
        template_test.cpp
)
target_link_libraries(
        template_test
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(template_test)