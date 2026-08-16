set(RTOS_UNIT_TEST_SOURCES
    unit/logging/LoggerTests.cpp
    unit/messaging/DispatchPortTests.cpp
    unit/messaging/PublisherSubscriberModelTests.cpp
    unit/messaging/SubscriptionRegistryTests.cpp
)

add_executable(rtos_unit_tests TestMain.cpp ${RTOS_UNIT_TEST_SOURCES})

target_compile_features(rtos_unit_tests PRIVATE cxx_std_20)
target_include_directories(rtos_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rtos_unit_tests PRIVATE rtos::framework)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rtos_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(rtos_unit_tests PRIVATE /W4 /permissive-)
endif()

foreach(test_source IN LISTS RTOS_UNIT_TEST_SOURCES)
    file(
        STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}"
        test_case_lines
        REGEX "^[ \t]*TEST_CASE\\(\"[^\"]+\"\\)"
    )

    foreach(test_case_line IN LISTS test_case_lines)
        string(
            REGEX REPLACE ".*TEST_CASE\\(\"([^\"]+)\"\\).*" "\\1"
            test_case_name
            "${test_case_line}"
        )
        string(MAKE_C_IDENTIFIER "${test_case_name}" test_case_id)

        add_test(
            NAME "rtos.unit.${test_case_id}"
            COMMAND rtos_unit_tests --case "${test_case_name}"
        )
    endforeach()
endforeach()
