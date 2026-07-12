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

# Core unit tests — no wxWidgets; always built when tests/ is in the graph.
add_executable(thoth-core-tests unit_tests.cpp)
target_compile_definitions(thoth-core-tests PRIVATE THOTH_HAS_GUI=0)
target_include_directories(thoth-core-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/external/basic_agent/include
)
target_link_libraries(thoth-core-tests PRIVATE basic_agent)
add_test(NAME thoth-core-tests COMMAND thoth-core-tests)
set_tests_properties(thoth-core-tests PROPERTIES
    LABELS "pr;unit"
    TIMEOUT 1800
)

if(THOTH_BUILD_GUI)
    add_executable(thoth-gui-tests
        unit_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/AgentInterface.cpp
    )
    target_compile_definitions(thoth-gui-tests PRIVATE
        THOTH_HAS_GUI=1
        THOTH_GUI_TESTS_ONLY=1
    )
    target_include_directories(thoth-gui-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/includes
        ${CMAKE_SOURCE_DIR}/external/basic_agent/include
        ${wxWidgets_INCLUDE_DIRS}
    )
    target_link_libraries(thoth-gui-tests PRIVATE
        basic_agent
        ${wxWidgets_LIBRARIES}
    )
    add_test(NAME thoth-gui-tests COMMAND thoth-gui-tests)
    set_tests_properties(thoth-gui-tests PROPERTIES
        LABELS "gui"
        TIMEOUT 300
    )
endif()

# Build-level compatibility alias — not a CTest entry. Run tests via
# ctest -R thoth-core-tests (and ctest -L gui for GUI tests).
add_custom_target(thoth-unit-tests
    DEPENDS thoth-core-tests
    COMMENT "Build alias — run ctest -R thoth-core-tests; GUI: ctest -L gui"
)
if(THOTH_BUILD_GUI)
    add_dependencies(thoth-unit-tests thoth-gui-tests)
endif()

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

add_test(NAME c6-longitudinal-join COMMAND python3 scripts/test_c6_longitudinal_join.py)
set_tests_properties(c6-longitudinal-join PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c6-longitudinal-analyzer COMMAND python3 scripts/test_c6_longitudinal_analyzer.py)
set_tests_properties(c6-longitudinal-analyzer PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_test(NAME c6-longitudinal-reporting COMMAND python3 scripts/test_c6_longitudinal_reporting.py)
set_tests_properties(c6-longitudinal-reporting PROPERTIES
    LABELS "pr;fast"
    TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

option(THOTH_TEST_SUITE_FULL "Register nightly Ollama TEST_SUITE (--full) in CTest" OFF)
if(THOTH_TEST_SUITE_FULL)
    add_test(NAME test-suite-full COMMAND run_test_suite --full)
    set_tests_properties(test-suite-full PROPERTIES
        LABELS "nightly;ollama;cognitive"
        TIMEOUT 3600
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
