add_executable(thoth-unit-tests
    unit_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/AgentInterface.cpp
)

target_include_directories(thoth-unit-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(thoth-unit-tests PRIVATE
    basic_agent
    ${wxWidgets_LIBRARIES}
)

add_test(NAME thoth-unit-tests COMMAND thoth-unit-tests)

add_executable(run_test_suite run_test_suite.cpp)
target_include_directories(run_test_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
)
target_link_libraries(run_test_suite PRIVATE basic_agent)
