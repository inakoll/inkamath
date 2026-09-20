add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/third_party)

add_executable(inkamath_tests
    doctest_main.cpp
    dynarray_test.cpp
    interpreter_test.cpp
    )

target_link_libraries(inkamath_tests PRIVATE inkamath doctest inkamath_warnings)
target_compile_definitions(inkamath_tests PRIVATE
    INKAMATH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    INKAMATH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# One ctest entry per doctest suite, so that a crash in one reports as a
# single failure rather than taking the others' results with it.
foreach(suite IN ITEMS interpreter dynarray)
    add_test(NAME ${suite} COMMAND inkamath_tests --test-suite=${suite} --no-intro)
    set_tests_properties(${suite} PROPERTIES TIMEOUT 60)
endforeach()

# Regenerate the golden transcripts from current behaviour. Always review the
# resulting diff: it is the record of what the change you just made did.
add_custom_target(record_goldens
    COMMAND ${CMAKE_COMMAND} -E env INKAMATH_RECORD=1
            $<TARGET_FILE:inkamath_tests> --test-suite=interpreter
    DEPENDS inkamath_tests
    VERBATIM)
