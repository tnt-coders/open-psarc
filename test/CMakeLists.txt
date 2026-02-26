find_package(Catch2 REQUIRED)

add_executable(tests sng_xml_validation.cpp)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain OpenPSARC)

target_compile_definitions(tests PRIVATE TEST_BINARY_DIR="$<TARGET_FILE_DIR:tests>")

# Copy testdata to the build directory so tests can find fixtures
add_custom_command(
    TARGET tests
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/testdata
            $<TARGET_FILE_DIR:tests>/testdata)

include(Catch)
catch_discover_tests(tests)
