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
set_tests_properties(thoth-unit-tests PROPERTIES
    LABELS "pr;unit"
    TIMEOUT 600
)

add_executable(run_test_suite run_test_suite.cpp)
target_include_directories(run_test_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
)
target_link_libraries(run_test_suite PRIVATE basic_agent)

add_test(NAME test-suite-dev COMMAND run_test_suite --dev)
set_tests_properties(test-suite-dev PROPERTIES
    LABELS "pr;fast;cognitive"
    TIMEOUT 180
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

if(TARGET run_reflection_ab_benchmark)
    add_test(NAME reflection-ab-benchmark COMMAND run_reflection_ab_benchmark)
    set_tests_properties(reflection-ab-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 120
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(TARGET run_robustness_suite)
    add_test(NAME robustness-suite COMMAND run_robustness_suite)
    set_tests_properties(robustness-suite PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if(TARGET run_trajectory_ablation_benchmark)
    add_test(NAME trajectory-ablation-benchmark COMMAND run_trajectory_ablation_benchmark --sample 2)
    set_tests_properties(trajectory-ablation-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "THOTH_TRAJECTORY_ABLATION_TFIDF=1"
    )
endif()

if(TARGET run_episodic_learning_benchmark)
    add_test(NAME episodic-learning-benchmark COMMAND run_episodic_learning_benchmark)
    set_tests_properties(episodic-learning-benchmark PROPERTIES
        LABELS "pr;fast;cognitive"
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

option(THOTH_TEST_SUITE_FULL "Register nightly Ollama TEST_SUITE (--full) in CTest" OFF)
if(THOTH_TEST_SUITE_FULL)
    add_test(NAME test-suite-full COMMAND run_test_suite --full)
    set_tests_properties(test-suite-full PROPERTIES
        LABELS "nightly;ollama;cognitive"
        TIMEOUT 3600
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
